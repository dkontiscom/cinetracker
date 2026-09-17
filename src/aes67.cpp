#include "aes67.h"

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

static EthernetUDP g_rtp_socket;
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
static uint16_t g_expected_seq = 0;
static bool g_have_expected_seq = false;

static uint32_t g_last_announce_ms = 0;

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

static void process_rx() {
  int size = g_rtp_socket.parsePacket();
  if (size < 12) return;

  uint8_t packet[12 + AES67_SAMPLES_PER_PACKET * 3];
  int len = g_rtp_socket.read(packet, sizeof(packet));
  if (len < 12) return;

  uint16_t seq = (packet[2] << 8) | packet[3];

  if (g_have_expected_seq && seq != g_expected_seq) {
    uint16_t gap = seq - g_expected_seq;
    g_packets_lost += gap;
  }
  g_expected_seq = seq + 1;
  g_have_expected_seq = true;
  g_packets_received++;

  int payload_len = len - 12;
  if (payload_len > AES67_SAMPLES_PER_PACKET * 3) payload_len = AES67_SAMPLES_PER_PACKET * 3;

  int slot = seq % JITTER_DEPTH;
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
  char sdp[256];
  IPAddress local = Ethernet.localIP();
  int len = snprintf(sdp, sizeof(sdp),
      "v=0\r\n"
      "o=- %lu %lu IN IP4 %u.%u.%u.%u\r\n"
      "s=%s\r\n"
      "c=IN IP4 %u.%u.%u.%u/32\r\n"
      "t=0 0\r\n"
      "m=audio %u RTP/AVP %d\r\n"
      "a=rtpmap:%d L24/%d/1\r\n"
      "a=ptime:1\r\n",
      (unsigned long)millis(), (unsigned long)millis(), local[0], local[1], local[2], local[3],
      g_config.stream_name, g_config.dest_ip[0], g_config.dest_ip[1], g_config.dest_ip[2],
      g_config.dest_ip[3], g_config.dest_port, AES67_PAYLOAD_TYPE, AES67_PAYLOAD_TYPE,
      AES67_SAMPLE_RATE);

  uint8_t sap_header[8] = {0x20, 0x00, 0, 0, 0, 0, 0, 0};
  sap_header[4] = local[0];
  sap_header[5] = local[1];
  sap_header[6] = local[2];
  sap_header[7] = local[3];

  g_sap_socket.beginPacket(g_sap_ip, AES67_SAP_PORT);
  g_sap_socket.write(sap_header, sizeof(sap_header));
  g_sap_socket.write((const uint8_t *)"application/sdp", 16);
  g_sap_socket.write((const uint8_t *)"\0", 1);
  g_sap_socket.write((const uint8_t *)sdp, len);
  g_sap_socket.endPacket();
}

int aes67_init(const Aes67Config *config) {
  g_config = *config;
  g_dest_ip = IPAddress(g_config.dest_ip[0], g_config.dest_ip[1], g_config.dest_ip[2], g_config.dest_ip[3]);
  g_ssrc = micros();

  AudioMemory_F32(16);

  g_record_queue.begin();

  if (g_rtp_socket.begin(g_config.dest_port) == 0) {
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
  memcpy(g_config.dest_ip, ip, 4);
  g_dest_ip = IPAddress(ip[0], ip[1], ip[2], ip[3]);

  if (port != g_config.dest_port) {
    g_config.dest_port = port;
    g_rtp_socket.stop();
    if (g_rtp_socket.begin(port) == 0) {
      Serial.println("Aes67: rtp socket rebind failed");
      return -1;
    }
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

  uint32_t now = millis();
  if (now - g_last_announce_ms >= AES67_ANNOUNCE_INTERVAL_MS) {
    send_sap_announce();
    g_last_announce_ms = now;
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
