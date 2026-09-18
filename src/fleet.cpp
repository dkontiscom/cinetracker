#include "fleet.h"

#include <QNEthernet.h>
#include <string.h>

using namespace qindesign::network;

static EthernetUDP g_udp;
static char g_device_name[32];
static uint8_t g_camera_id = 0;
static uint32_t g_last_announce_ms = 0;

static FleetPeer g_peers[FLEET_MAX_PEERS];
static int g_peer_count = 0;

static FleetPeer *find_or_add_peer(const uint8_t ip[4]) {
  for (int i = 0; i < g_peer_count; i++) {
    if (memcmp(g_peers[i].ip, ip, 4) == 0) return &g_peers[i];
  }
  if (g_peer_count < FLEET_MAX_PEERS) {
    FleetPeer *p = &g_peers[g_peer_count++];
    memcpy(p->ip, ip, 4);
    return p;
  }
  return nullptr;
}

static void expire_peers() {
  uint32_t now = millis();
  int write = 0;
  for (int i = 0; i < g_peer_count; i++) {
    if (now - g_peers[i].last_seen_ms < FLEET_PEER_TIMEOUT_MS) {
      if (write != i) g_peers[write] = g_peers[i];
      write++;
    }
  }
  g_peer_count = write;
}

int fleet_init(const char *device_name, uint8_t camera_id) {
  strncpy(g_device_name, device_name, sizeof(g_device_name) - 1);
  g_device_name[sizeof(g_device_name) - 1] = '\0';
  g_camera_id = camera_id;

  if (g_udp.begin(FLEET_PORT) == 0) {
    Serial.println("Fleet: udp begin failed");
    return -1;
  }

  return 0;
}

// camera id can change live from the web ui with no reboot, main.cpp's
// device_name is just "cinetracker-<camera_id>" derived from the same
// number at boot, so both go stale together unless this keeps them in
// sync - otherwise this device keeps announcing its old id to peers and
// fleet_camera_id_conflict() keeps checking against the wrong one too
void fleet_set_camera_id(uint8_t camera_id) {
  g_camera_id = camera_id;
  snprintf(g_device_name, sizeof(g_device_name), "cinetracker-%u", camera_id);
}

static void send_announce() {
  char msg[64];
  int len = snprintf(msg, sizeof(msg), "CINETRACKER|%s|%u", g_device_name, g_camera_id);

  IPAddress broadcast(255, 255, 255, 255);
  g_udp.beginPacket(broadcast, FLEET_PORT);
  g_udp.write((const uint8_t *)msg, len);
  g_udp.endPacket();
}

static void handle_incoming() {
  int size = g_udp.parsePacket();
  if (size <= 0) return;

  char buf[64];
  int len = g_udp.read(buf, sizeof(buf) - 1);
  if (len <= 0) return;
  buf[len] = '\0';

  if (strncmp(buf, "CINETRACKER|", 12) != 0) return;

  char *name_start = buf + 12;
  char *sep = strchr(name_start, '|');
  if (!sep) return;
  *sep = '\0';
  uint8_t camera_id = (uint8_t)atoi(sep + 1);

  IPAddress remote = g_udp.remoteIP();
  uint8_t ip[4] = {remote[0], remote[1], remote[2], remote[3]};

  FleetPeer *peer = find_or_add_peer(ip);
  if (!peer) return;

  strncpy(peer->name, name_start, sizeof(peer->name) - 1);
  peer->name[sizeof(peer->name) - 1] = '\0';
  peer->camera_id = camera_id;
  peer->last_seen_ms = millis();
}

void fleet_process() {
  uint32_t now = millis();
  if (now - g_last_announce_ms >= FLEET_ANNOUNCE_INTERVAL_MS) {
    send_announce();
    g_last_announce_ms = now;
  }

  handle_incoming();
  expire_peers();
}

int fleet_get_peers(FleetPeer *out, int max_count) {
  int count = (g_peer_count < max_count) ? g_peer_count : max_count;
  memcpy(out, g_peers, count * sizeof(FleetPeer));
  return count;
}

bool fleet_camera_id_conflict() {
  for (int i = 0; i < g_peer_count; i++) {
    if (g_peers[i].camera_id == g_camera_id) return true;
  }
  return false;
}
