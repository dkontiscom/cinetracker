#pragma once

#include <Arduino.h>
#include "math_engine.h"

#define FREED_PORT 40000
#define FREED_NORMALIZED_MAX 16777215

struct FreedPacket {
  uint8_t header;
  uint8_t camera_id;
  uint8_t pan[3];
  uint8_t tilt[3];
  uint8_t roll[3];
  uint8_t x[3];
  uint8_t y[3];
  uint8_t z[3];
  uint8_t zoom[3];
  uint8_t focus[3];
  uint8_t spare[2];
  uint8_t checksum;
} __attribute__((packed));

struct NetworkConfig {
  bool dhcp_enabled;
  uint8_t ip_address[4];
  uint8_t subnet_mask[4];
  uint8_t gateway[4];
  uint8_t dest_ip[4];
  uint16_t dest_port;
  uint8_t camera_id;
  uint8_t frame_rate;
  uint8_t user_data;
};

struct NetworkStats {
  uint32_t packets_sent;
  uint32_t packets_dropped;
  uint64_t bytes_sent;
  uint32_t min_latency_us;
  uint32_t max_latency_us;
  uint32_t avg_latency_us;
  uint32_t missed_frames;
  uint32_t link_down_events;
  float actual_fps;
  float target_fps;
};

void network_get_default_config(NetworkConfig *config);
int network_validate_config(const NetworkConfig *config);

int network_stack_init(const NetworkConfig *config);
int network_reconfigure(const NetworkConfig *config);
bool network_check_link_status();

void network_build_freed_packet(FreedPacket *packet, const EncoderState encoder_states[3],
                                 uint8_t camera_id, uint8_t user_data);
int network_send_freed(const FreedPacket *packet);
int network_process_frame(const EncoderState encoder_states[3]);

void network_set_camera_id(uint8_t camera_id);
void network_set_frame_rate(uint8_t fps);
uint8_t network_get_camera_id();
uint8_t network_get_frame_rate();

void network_get_statistics(NetworkStats *stats);
void network_reset_statistics();
float network_get_actual_fps();
uint32_t network_get_uptime_seconds();
