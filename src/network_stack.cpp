#include "network_stack.h"

#include <QNEthernet.h>

using namespace qindesign::network;

static NetworkConfig g_config;
static bool g_initialized = false;

static EthernetUDP g_udp;
static IPAddress g_dest_ip;

static uint32_t g_frame_interval_us = 0;
static uint32_t g_next_frame_time = 0;
static uint32_t g_last_process_us = 0;
static bool g_have_last_process = false;
static uint32_t g_last_frame_time = 0;
static bool g_have_last_frame = false;

static uint32_t g_start_millis = 0;
static NetworkStats g_stats;

void network_get_default_config(NetworkConfig *config) {
  if (!config) return;

  memset(config, 0, sizeof(NetworkConfig));

  config->dhcp_enabled = true;
  config->ip_address[0] = 192;
  config->ip_address[1] = 168;
  config->ip_address[2] = 1;
  config->ip_address[3] = 100;
  config->subnet_mask[0] = 255;
  config->subnet_mask[1] = 255;
  config->subnet_mask[2] = 255;
  config->subnet_mask[3] = 0;
  config->gateway[0] = 192;
  config->gateway[1] = 168;
  config->gateway[2] = 1;
  config->gateway[3] = 1;

  // broadcast not multicast livelink freed wants unicast/broadcast only
  config->dest_ip[0] = 255;
  config->dest_ip[1] = 255;
  config->dest_ip[2] = 255;
  config->dest_ip[3] = 255;
  config->dest_port = FREED_PORT;
  config->camera_id = 0;
  config->frame_rate = 60;
  config->user_data = 0;
}

int network_validate_config(const NetworkConfig *config) {
  if (!config) return -1;

  uint32_t dest = (config->dest_ip[0] << 24) | (config->dest_ip[1] << 16) |
                  (config->dest_ip[2] << 8) | config->dest_ip[3];
  if (dest == 0) return -1;

  if (config->frame_rate == 0 || config->frame_rate > 240) return -1;

  return 0;
}

static int bring_up_ethernet() {
  g_dest_ip = IPAddress(g_config.dest_ip[0], g_config.dest_ip[1], g_config.dest_ip[2], g_config.dest_ip[3]);

  bool up;
  if (g_config.dhcp_enabled) {
    up = Ethernet.begin();
  } else {
    IPAddress ip(g_config.ip_address[0], g_config.ip_address[1], g_config.ip_address[2], g_config.ip_address[3]);
    IPAddress subnet(g_config.subnet_mask[0], g_config.subnet_mask[1], g_config.subnet_mask[2], g_config.subnet_mask[3]);
    IPAddress gateway(g_config.gateway[0], g_config.gateway[1], g_config.gateway[2], g_config.gateway[3]);
    up = Ethernet.begin(ip, subnet, gateway);
  }

  if (!up) {
    Serial.println("Network: Ethernet.begin failed");
    return -1;
  }

  if (g_config.dhcp_enabled) {
    Ethernet.waitForLocalIP(10000);
  }

  if (g_udp.begin(g_config.dest_port) == 0) {
    Serial.println("Network: udp begin failed");
    return -1;
  }

  return 0;
}

int network_stack_init(const NetworkConfig *config) {
  if (!config) return -1;
  if (network_validate_config(config) != 0) {
    Serial.println("Network: invalid config");
    return -1;
  }

  g_config = *config;

  if (bring_up_ethernet() != 0) return -1;

  g_frame_interval_us = 1000000 / g_config.frame_rate;
  g_next_frame_time = micros() + g_frame_interval_us;
  g_have_last_frame = false;

  g_start_millis = millis();
  g_stats = {};
  g_stats.min_latency_us = 0xFFFFFFFF;
  g_stats.target_fps = g_config.frame_rate;

  g_initialized = true;

  Serial.printf("Network: dest %u.%u.%u.%u:%u camera %u fps %u\n",
                g_config.dest_ip[0], g_config.dest_ip[1], g_config.dest_ip[2], g_config.dest_ip[3],
                g_config.dest_port, g_config.camera_id, g_config.frame_rate);

  return 0;
}

int network_reconfigure(const NetworkConfig *config) {
  if (!config) return -1;
  if (network_validate_config(config) != 0) {
    Serial.println("Network: invalid config");
    return -1;
  }

  g_udp.stop();
  Ethernet.end();

  g_config = *config;

  if (bring_up_ethernet() != 0) return -1;

  g_frame_interval_us = 1000000 / g_config.frame_rate;
  g_stats.target_fps = g_config.frame_rate;

  Serial.printf("Network: reconfigured, dhcp=%d ip %u.%u.%u.%u\n", g_config.dhcp_enabled, g_config.ip_address[0],
                g_config.ip_address[1], g_config.ip_address[2], g_config.ip_address[3]);

  return 0;
}

bool network_check_link_status() {
  if (!g_initialized) return false;

  bool up = Ethernet.linkState();
  static bool prev_up = up;
  if (up != prev_up) {
    Serial.println(up ? "Network: link up" : "Network: link down");
    if (!up) g_stats.link_down_events++;
    prev_up = up;
  }
  return up;
}

// freed zoom/focus are unsigned 24 bit raw encoder values (thats what
// FREED_NORMALIZED_MAX = 0xFFFFFF is for), this used to clamp to the
// signed range instead so everything past half the lens throw sat flat
// at 8388607. only ever used for those two fields, pan/tilt/xyz are
// never populated by a lens only tracker
static void freed_encode_24bit(int32_t value, uint8_t *buffer) {
  if (value > FREED_NORMALIZED_MAX) value = FREED_NORMALIZED_MAX;
  if (value < 0) value = 0;

  buffer[0] = (value >> 16) & 0xFF;
  buffer[1] = (value >> 8) & 0xFF;
  buffer[2] = value & 0xFF;
}

// vizrt spec is 40 hex minus the sum of the preceding bytes not a plain sum
static uint8_t freed_calculate_checksum(const FreedPacket *packet) {
  uint8_t sum = 0;
  const uint8_t *bytes = (const uint8_t *)packet;
  for (int i = 0; i < 28; i++) sum += bytes[i];
  return (uint8_t)(0x40 - sum);
}

// cal_max isnt always the bigger number, direction says which side is the
// physical min, so use cal_range (already a plain magnitude) and flip the
// offset the same way math_engine's process_encoder_sample does. the
// multiply has to be 64 bit: offset * 0xFFFFFF overflows int32 for any
// offset past 127 counts and a lens throw is thousands, so every value
// past about a degree of travel used to wrap negative on the wire. clamps
// so a position outside the window (mid set on the wrong side of min, a
// stale window) gives an end stop instead of a negative or wrapped value
static int32_t freed_normalize(const EncoderState *e, int32_t max) {
  if (!e->enabled || e->cal_range <= 0) return 0;
  int32_t offset = e->direction == 0 ? (e->filtered_angle - e->cal_min) : (e->cal_min - e->filtered_angle);
  if (offset < 0) return 0;
  if (offset > e->cal_range) return max;
  return (int32_t)(((int64_t)offset * max) / e->cal_range);
}

void network_build_freed_packet(FreedPacket *packet, const EncoderState encoder_states[3],
                                 uint8_t camera_id, uint8_t user_data) {
  if (!packet || !encoder_states) return;

  memset(packet, 0, sizeof(FreedPacket));
  packet->header = 0xD1;
  packet->camera_id = camera_id;

  freed_encode_24bit(freed_normalize(&encoder_states[2], FREED_NORMALIZED_MAX), packet->zoom);
  freed_encode_24bit(freed_normalize(&encoder_states[0], FREED_NORMALIZED_MAX), packet->focus);
  packet->spare[0] = (uint8_t)freed_normalize(&encoder_states[1], 255);
  packet->spare[1] = user_data;

  packet->checksum = freed_calculate_checksum(packet);
}

int network_send_freed(const FreedPacket *packet) {
  if (!g_initialized || !packet) return -1;

  if (!network_check_link_status()) {
    g_stats.packets_dropped++;
    return -1;
  }

  uint32_t start = micros();

  g_udp.beginPacket(g_dest_ip, g_config.dest_port);
  size_t written = g_udp.write((const uint8_t *)packet, sizeof(FreedPacket));
  bool sent = g_udp.endPacket() != 0;

  uint32_t latency = micros() - start;

  if (sent && written == sizeof(FreedPacket)) {
    g_stats.packets_sent++;
    g_stats.bytes_sent += sizeof(FreedPacket);

    if (latency < g_stats.min_latency_us) g_stats.min_latency_us = latency;
    if (latency > g_stats.max_latency_us) g_stats.max_latency_us = latency;
    g_stats.avg_latency_us = (g_stats.packets_sent == 1) ? latency : (g_stats.avg_latency_us + latency) / 2;

    g_last_frame_time = micros();
    g_have_last_frame = true;
    return 0;
  }

  g_stats.packets_dropped++;
  return -1;
}

int network_process_frame(const EncoderState encoder_states[3]) {
  if (!g_initialized) return -1;

  uint32_t now = micros();

  // not called for a while means streaming was off (or the loop stalled
  // hard) and the schedule is meaningless: resync to now instead of
  // trusting a next_frame_time left behind 36+ minutes ago - past 2^31us
  // the signed diff below reads that as "not yet" and nothing went out
  // for up to another 36 minutes while the ui said streaming. this also
  // keeps a resume from being counted as a missed frame
  if (!g_have_last_process || (now - g_last_process_us) > 2 * g_frame_interval_us) {
    g_next_frame_time = now;
  }
  g_last_process_us = now;
  g_have_last_process = true;

  // signed diff trick so this still works when micros wraps around
  if ((int32_t)(now - g_next_frame_time) < 0) return 1;

  FreedPacket packet;
  network_build_freed_packet(&packet, encoder_states, g_config.camera_id, g_config.user_data);
  int result = network_send_freed(&packet);

  g_next_frame_time += g_frame_interval_us;
  if ((int32_t)(g_next_frame_time - now) < 0) {
    g_stats.missed_frames++;
    g_next_frame_time = now + g_frame_interval_us;
  }

  if (g_have_last_frame) {
    uint32_t delta = now - g_last_frame_time;
    if (delta > 0) g_stats.actual_fps = 1000000.0f / (float)delta;
  }

  return result;
}

void network_set_camera_id(uint8_t camera_id) {
  g_config.camera_id = camera_id;
}

void network_set_frame_rate(uint8_t fps) {
  if (fps == 0 || fps > 240) return;
  g_config.frame_rate = fps;
  g_frame_interval_us = 1000000 / fps;
  g_stats.target_fps = fps;
}

void network_get_current_config(NetworkConfig *config) {
  if (!config) return;
  *config = g_config;
}

uint8_t network_get_camera_id() {
  return g_config.camera_id;
}

uint8_t network_get_frame_rate() {
  return g_config.frame_rate;
}

void network_get_statistics(NetworkStats *stats) {
  if (!stats) return;
  *stats = g_stats;
}

void network_reset_statistics() {
  g_stats = {};
  g_stats.min_latency_us = 0xFFFFFFFF;
  g_stats.target_fps = g_config.frame_rate;
}

float network_get_actual_fps() {
  return g_stats.actual_fps;
}

uint32_t network_get_uptime_seconds() {
  return (millis() - g_start_millis) / 1000;
}
