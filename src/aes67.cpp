#include "aes67.h"
#include "ptp_client.h"

#include <Audio.h>
#include <AudioStream_F32.h>
#include <input_i2s_f32.h>
#include <output_i2s_f32.h>
#include <play_queue_f32.h>
#include <record_queue_f32.h>

#include <QNEthernet.h>
#include <string.h>

using namespace qindesign::network;

// pcm5102a dac out and pcm1808 adc in format pins strapped on the boards
// themselves no i2c no control chip nothing to enable or configure here
static AudioSettings_F32 g_audio_settings(AES67_SAMPLE_RATE, AES67_SAMPLES_PER_PACKET);
static AudioInputI2S_F32 g_i2s_in(g_audio_settings);
static AudioOutputI2S_F32 g_i2s_out(g_audio_settings);
static AudioRecordQueue_F32 g_record_queue;
static AudioPlayQueue_F32 g_play_queue;

static AudioConnection_F32 g_conn_in(g_i2s_in, 0, g_record_queue, 0);
static AudioConnection_F32 g_conn_out(g_play_queue, 0, g_i2s_out, 0);

static Aes67Direction g_direction = AES67_OFF;
static Aes67Config g_config;

// default queue depth is 1 packet: with 1ms packets, any loop iteration
// longer than that (a web request, a flash write) dropped everything but
// the newest at the socket, before the jitter buffer ever saw it. 8 parks
// an 8ms stall's worth to be drained afterwards
#define AES67_RX_QUEUE_DEPTH 8
static EthernetUDP g_rtp_socket(AES67_RX_QUEUE_DEPTH);
static EthernetUDP g_sap_socket;
static IPAddress g_dest_ip;
static IPAddress g_sap_ip(AES67_SAP_ADDR_A, AES67_SAP_ADDR_B, AES67_SAP_ADDR_C, AES67_SAP_ADDR_D);

static uint16_t g_seq = 0;
static uint32_t g_rtp_timestamp = 0;
static uint32_t g_ssrc = 0;

static uint32_t g_packets_sent = 0;
static uint32_t g_packets_received = 0;
static uint32_t g_packets_lost = 0;
static uint32_t g_underruns = 0;
static uint32_t g_jitter_overwrites = 0;
static uint16_t g_expected_seq = 0;
static bool g_have_expected_seq = false;

static uint32_t g_last_announce_ms = 0;

// sdp origin: sess-id has to be stable for the life of the session and
// sess-version has to go up only when the description changes (rfc 4566).
// both used to be millis(), so every 30s announce looked like a brand new
// session to receivers and the old ones lingered for up to an hour
static uint64_t g_sess_id = 0;
static uint32_t g_sdp_version = 1;
static uint8_t g_announced_master[8];
static uint8_t g_announced_domain = 0;
static bool g_announced_master_seen = false;

static uint32_t fnv1a(const uint8_t *data, size_t len) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < len; i++) {
    h ^= data[i];
    h *= 16777619u;
  }
  return h;
}

// small reorder/jitter buffer for rx only tx already paced by the i2s input clock
#define JITTER_DEPTH 4
#define JITTER_PRIME_PACKETS 2
#define PACKET_PERIOD_US (AES67_SAMPLES_PER_PACKET * 1000000UL / AES67_SAMPLE_RATE)

struct JitterSlot {
  bool valid;
  uint8_t payload[AES67_SAMPLES_PER_PACKET * 3];
};
static JitterSlot g_jitter[JITTER_DEPTH];
static uint16_t g_playout_seq = 0;
static bool g_playout_primed = false;
static int g_priming_count = 0;
static uint32_t g_last_playout_us = 0;

static void float_to_l24(const float32_t *in, uint8_t *out, int count) {
  for (int i = 0; i < count; i++) {
    int32_t v = (int32_t)(in[i] * 8388607.0f);
    if (v > 8388607) v = 8388607;
    if (v < -8388608) v = -8388608;
    out[i * 3 + 0] = (v >> 16) & 0xFF;
    out[i * 3 + 1] = (v >> 8) & 0xFF;
    out[i * 3 + 2] = v & 0xFF;
  }
}

static void l24_to_float(const uint8_t *in, float32_t *out, int count) {
  for (int i = 0; i < count; i++) {
    int32_t v = (in[i * 3 + 0] << 16) | (in[i * 3 + 1] << 8) | in[i * 3 + 2];
    if (v & 0x800000) v |= 0xFF000000;
    out[i] = (float32_t)v / 8388608.0f;
  }
}

// payload type 98 isnt an iana standard for l24 just has to match what send_sap_announce advertises
static void send_rtp_packet(const uint8_t *payload, int payload_len) {
  uint8_t packet[12 + AES67_SAMPLES_PER_PACKET * 3];
  packet[0] = 0x80;
  packet[1] = AES67_PAYLOAD_TYPE & 0x7F;
  packet[2] = (g_seq >> 8) & 0xFF;
  packet[3] = g_seq & 0xFF;
  packet[4] = (g_rtp_timestamp >> 24) & 0xFF;
  packet[5] = (g_rtp_timestamp >> 16) & 0xFF;
  packet[6] = (g_rtp_timestamp >> 8) & 0xFF;
  packet[7] = g_rtp_timestamp & 0xFF;
  packet[8] = (g_ssrc >> 24) & 0xFF;
  packet[9] = (g_ssrc >> 16) & 0xFF;
  packet[10] = (g_ssrc >> 8) & 0xFF;
  packet[11] = g_ssrc & 0xFF;
  memcpy(packet + 12, payload, payload_len);

  g_rtp_socket.beginPacket(g_dest_ip, g_config.dest_port);
  g_rtp_socket.write(packet, 12 + payload_len);
  g_rtp_socket.endPacket();

  g_seq++;
  g_rtp_timestamp += AES67_SAMPLES_PER_PACKET;
  g_packets_sent++;
}

static void process_tx() {
  while (g_record_queue.available()) {
    float32_t *block = g_record_queue.readBuffer();
    uint8_t payload[AES67_SAMPLES_PER_PACKET * 3];
    float_to_l24(block, payload, AES67_SAMPLES_PER_PACKET);
    g_record_queue.freeBuffer();
    send_rtp_packet(payload, AES67_SAMPLES_PER_PACKET * 3);
  }
}

// one packet, false only when the socket had nothing left
static bool process_rx_one() {
  int size = g_rtp_socket.parsePacket();
  if (size <= 0) return false;
  if (size < 12) return true;

  uint8_t packet[12 + AES67_SAMPLES_PER_PACKET * 3];
  int len = g_rtp_socket.read(packet, sizeof(packet));
  if (len < 12) return true;

  uint16_t seq = (packet[2] << 8) | packet[3];

  // signed so a reordered/late packet (seq behind expected) shows up as a
  // small negative number instead of wrapping a uint16 into a huge one.
  // a late packet must not rewind expected either - it used to, so the
  // next in-order one then read as a second loss for the same gap
  if (g_have_expected_seq) {
    int16_t gap = (int16_t)(seq - g_expected_seq);
    if (gap > 0) g_packets_lost += gap;
    if (gap >= 0) g_expected_seq = seq + 1;
  } else {
    g_expected_seq = seq + 1;
    g_have_expected_seq = true;
  }
  g_packets_received++;

  int payload_len = len - 12;
  if (payload_len > AES67_SAMPLES_PER_PACKET * 3) payload_len = AES67_SAMPLES_PER_PACKET * 3;

  int slot = seq % JITTER_DEPTH;
  if (g_jitter[slot].valid) g_jitter_overwrites++;
  memcpy(g_jitter[slot].payload, packet + 12, payload_len);
  g_jitter[slot].valid = true;

  if (!g_playout_primed) {
    if (g_priming_count == 0) g_playout_seq = seq;
    g_priming_count++;
    if (g_priming_count >= JITTER_PRIME_PACKETS) {
      g_playout_primed = true;
      g_last_playout_us = micros();
    }
  }
  return true;
}

// drains whatever the socket queued, not one per loop - reading one per
// iteration meant a stall's backlog could never catch up, it just kept
// overflowing the queue one packet at a time
static void process_rx() {
  for (int i = 0; i < AES67_RX_QUEUE_DEPTH; i++) {
    if (!process_rx_one()) break;
  }
}

// paced independently of packet arrival so network jitter doesnt reach the
// analog output silence on underrun rather than stalling since ltc just
// needs to stay continuous not perfectly accurate to the millisecond
static void process_playout() {
  if (!g_playout_primed) return;

  uint32_t now = micros();
  if (now - g_last_playout_us < PACKET_PERIOD_US) return;
  g_last_playout_us += PACKET_PERIOD_US;

  float32_t *block = g_play_queue.getBuffer();
  if (!block) return;

  int slot = g_playout_seq % JITTER_DEPTH;
  if (g_jitter[slot].valid) {
    l24_to_float(g_jitter[slot].payload, block, AES67_SAMPLES_PER_PACKET);
    g_jitter[slot].valid = false;
  } else {
    memset(block, 0, sizeof(float32_t) * AES67_SAMPLES_PER_PACKET);
    g_underruns++;
  }

  g_play_queue.playBuffer();
  g_playout_seq++;
}

static void send_sap_announce() {
  uint8_t master[8] = {0};
  uint8_t domain = 0;
  bool master_seen = ptp_client_master_identity(master, &domain);

  // the grandmaster appearing (or changing) changes the description, so
  // the version has to move with it - a receiver keyed on the origin
  // line then sees an update, not a duplicate to ignore
  if (master_seen != g_announced_master_seen ||
      (master_seen && (memcmp(master, g_announced_master, 8) != 0 || domain != g_announced_domain))) {
    g_sdp_version++;
    g_announced_master_seen = master_seen;
    memcpy(g_announced_master, master, 8);
    g_announced_domain = domain;
  }

  // aes67 requires a=ts-refclk and a=mediaclk (rfc 7273), dante controller
  // may hide a stream without them. no master seen yet means no honest
  // refclk to give, so its left out until one shows up rather than
  // claiming "traceable" for a master we know nothing about
  char refclk[80] = "";
  if (master_seen) {
    snprintf(refclk, sizeof(refclk),
             "a=ts-refclk:ptp=IEEE1588-2008:%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X:%u\r\n",
             master[0], master[1], master[2], master[3], master[4], master[5], master[6], master[7], domain);
  }

  char sdp[320];
  IPAddress local = Ethernet.localIP();
  int len = snprintf(sdp, sizeof(sdp),
      "v=0\r\n"
      "o=- %llu %lu IN IP4 %u.%u.%u.%u\r\n"
      "s=%s\r\n"
      "c=IN IP4 %u.%u.%u.%u/32\r\n"
      "t=0 0\r\n"
      "m=audio %u RTP/AVP %d\r\n"
      "a=rtpmap:%d L24/%d/1\r\n"
      "a=ptime:1\r\n"
      "a=recvonly\r\n"
      "%s"
      "a=mediaclk:direct=0\r\n",
      (unsigned long long)g_sess_id, (unsigned long)g_sdp_version, local[0], local[1], local[2], local[3],
      g_config.stream_name, g_config.dest_ip[0], g_config.dest_ip[1], g_config.dest_ip[2],
      g_config.dest_ip[3], g_config.dest_port, AES67_PAYLOAD_TYPE, AES67_PAYLOAD_TYPE,
      AES67_SAMPLE_RATE, refclk);
  if (len <= 0 || (size_t)len >= sizeof(sdp)) return;

  // rfc 2974 msg id hash, bytes 2-3: receivers use it to tell a repeat of
  // the same announcement from a changed one without parsing the sdp
  uint16_t hash = (uint16_t)fnv1a((const uint8_t *)sdp, (size_t)len);
  uint8_t sap_header[8] = {0x20, 0x00, (uint8_t)(hash >> 8), (uint8_t)(hash & 0xFF),
                           local[0], local[1], local[2], local[3]};

  g_sap_socket.beginPacket(g_sap_ip, AES67_SAP_PORT);
  g_sap_socket.write(sap_header, sizeof(sap_header));
  // 15 chars plus its own nul, thats the whole payload-type field - a
  // second "\0" used to follow it so the sdp started with a 0x00 byte
  g_sap_socket.write((const uint8_t *)"application/sdp", 16);
  g_sap_socket.write((const uint8_t *)sdp, len);
  g_sap_socket.endPacket();
}

static bool is_multicast(const uint8_t ip[4]) {
  return ip[0] >= 224 && ip[0] <= 239;
}

// a plain begin(port) is a unicast bind - lwip has igmp on and drops
// multicast for any group the netif hasnt joined, and the mac hash filter
// rejects it even earlier, so LTC_OUT mode heard nothing at all, ever.
// the join is by destination type not direction on purpose: direction can
// flip at runtime and a socket already sitting in the group costs nothing
// in transmit mode (process_rx isnt called then anyway), so theres no
// second rebind path to get wrong
static int bind_rtp_socket() {
  uint8_t ok = is_multicast(g_config.dest_ip)
      ? g_rtp_socket.beginMulticast(g_dest_ip, g_config.dest_port)
      : g_rtp_socket.begin(g_config.dest_port);
  return ok ? 0 : -1;
}

int aes67_init(const Aes67Config *config) {
  g_config = *config;
  g_dest_ip = IPAddress(g_config.dest_ip[0], g_config.dest_ip[1], g_config.dest_ip[2], g_config.dest_ip[3]);
  g_ssrc = micros();

  // mac as a 48 bit number: unique per device, stable across boots
  uint8_t mac[6];
  Ethernet.macAddress(mac);
  g_sess_id = 0;
  for (int i = 0; i < 6; i++) g_sess_id = (g_sess_id << 8) | mac[i];

  // with the settings, otherwise the pool blocks default to 128 samples
  // and the i2s output prints a mismatch warning per block from the audio
  // interrupt, a thousand times a second, in receive mode
  AudioMemory_F32(16, g_audio_settings);

  g_record_queue.begin();

  if (bind_rtp_socket() != 0) {
    Serial.println("Aes67: rtp socket begin failed");
    return -1;
  }
  if (g_sap_socket.begin(AES67_SAP_PORT) == 0) {
    Serial.println("Aes67: sap socket begin failed");
    return -1;
  }

  return 0;
}

void aes67_set_direction(Aes67Direction dir) {
  g_direction = dir;
  g_record_queue.clear();

  for (int i = 0; i < JITTER_DEPTH; i++) g_jitter[i].valid = false;
  g_playout_primed = false;
  g_priming_count = 0;
  g_have_expected_seq = false;
}

Aes67Direction aes67_get_direction() {
  return g_direction;
}

int aes67_set_destination(const uint8_t ip[4], uint16_t port) {
  bool changed = memcmp(g_config.dest_ip, ip, 4) != 0 || port != g_config.dest_port;
  if (!changed) return 0;
  // the c= and m= lines change with it, so the sdp version has to too
  g_sdp_version++;

  // stop() only removes the pcb - the group membership lives on the netif
  // and is ref counted (checked in QNEthernetUDP.cpp), so the old group has
  // to be left by hand or joins pile up across destination changes. an ip
  // change used to skip the rebind entirely, fine for unicast, wrong for
  // multicast where the group itself is what changed
  if (is_multicast(g_config.dest_ip)) Ethernet.leaveGroup(g_dest_ip);
  g_rtp_socket.stop();

  memcpy(g_config.dest_ip, ip, 4);
  g_config.dest_port = port;
  g_dest_ip = IPAddress(ip[0], ip[1], ip[2], ip[3]);

  if (bind_rtp_socket() != 0) {
    Serial.println("Aes67: rtp socket rebind failed");
    return -1;
  }

  return 0;
}

void aes67_get_destination(uint8_t ip[4], uint16_t *port) {
  memcpy(ip, g_config.dest_ip, 4);
  *port = g_config.dest_port;
}

void aes67_process() {
  if (g_direction == AES67_LTC_IN) {
    process_tx();
  } else {
    // mic input keeps feeding the queue regardless of direction drain it so it doesnt fill up
    while (g_record_queue.available()) {
      g_record_queue.freeBuffer();
    }
  }

  if (g_direction == AES67_LTC_OUT) {
    process_rx();
    process_playout();
  }

  // sap is a sender's mechanism - only announce when we're actually the
  // one putting this stream on the wire. a receiver announcing itself as
  // a source at the same multicast address showed up as a conflicting
  // duplicate next to the real source in anything discovering via sap.
  // the timer stays outside the gate on purpose, so flipping to LTC_IN
  // announces right away instead of up to 30s later
  if (g_direction == AES67_LTC_IN) {
    uint32_t now = millis();
    if (now - g_last_announce_ms >= AES67_ANNOUNCE_INTERVAL_MS) {
      send_sap_announce();
      g_last_announce_ms = now;
    }
  }
}

uint32_t aes67_packets_sent() {
  return g_packets_sent;
}

uint32_t aes67_packets_received() {
  return g_packets_received;
}

uint32_t aes67_packets_lost() {
  return g_packets_lost;
}

uint32_t aes67_underruns() {
  return g_underruns;
}

uint32_t aes67_jitter_overwrites() {
  return g_jitter_overwrites;
}
