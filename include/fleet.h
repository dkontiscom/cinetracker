#pragma once

#include <Arduino.h>

#define FLEET_PORT 40100
#define FLEET_MAX_PEERS 16
#define FLEET_ANNOUNCE_INTERVAL_MS 2000
#define FLEET_PEER_TIMEOUT_MS 8000

struct FleetPeer {
  uint8_t ip[4];
  char name[32];
  uint8_t camera_id;
  uint32_t last_seen_ms;
};

int fleet_init(const char *device_name, uint8_t camera_id);
void fleet_process();
int fleet_get_peers(FleetPeer *out, int max_count);
bool fleet_camera_id_conflict();
