#pragma once

#include <Arduino.h>

#define AES67_SAMPLE_RATE 48000
#define AES67_SAMPLES_PER_PACKET 48
#define AES67_PAYLOAD_TYPE 98
#define AES67_SAP_ADDR_A 239
#define AES67_SAP_ADDR_B 255
#define AES67_SAP_ADDR_C 255
#define AES67_SAP_ADDR_D 255
#define AES67_SAP_PORT 9875
#define AES67_ANNOUNCE_INTERVAL_MS 30000

enum Aes67Direction {
  AES67_OFF = 0,
  AES67_LTC_IN = 1,
  AES67_LTC_OUT = 2,
};

struct Aes67Config {
  uint8_t dest_ip[4];
  uint16_t dest_port;
  char stream_name[32];
};

// direction + destination only stream_name isnt user settable so no
// point persisting it
struct Aes67Settings {
  uint8_t direction;
  uint8_t dest_ip[4];
  uint16_t dest_port;
};

int aes67_init(const Aes67Config *config);
void aes67_set_direction(Aes67Direction dir);
Aes67Direction aes67_get_direction();
int aes67_set_destination(const uint8_t ip[4], uint16_t port);
void aes67_get_destination(uint8_t ip[4], uint16_t *port);
void aes67_process();
uint32_t aes67_packets_sent();
uint32_t aes67_packets_received();
uint32_t aes67_packets_lost();
uint32_t aes67_underruns();
uint32_t aes67_jitter_overwrites();
