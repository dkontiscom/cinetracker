#include "web_interface.h"

#include <QNEthernet.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "encoder_driver.h"
#include "lens_profiles.h"
#include "network_stack.h"
#include "web_content.h"
#include "fleet.h"

using namespace qindesign::network;

struct ClientSlot {
  EthernetClient client;
  char buffer[WEB_REQUEST_BUFFER_SIZE];
  size_t buffer_len;
  bool active;
};

static EthernetServer g_server{WEB_SERVER_PORT};
static ClientSlot g_slots[WEB_MAX_CLIENTS];
static bool g_streaming = false;
static LensProfile g_active_profile;

static int find_headers_end(const char *buf, size_t len) {
  for (size_t i = 3; i < len; i++) {
    if (buf[i - 3] == '\r' && buf[i - 2] == '\n' && buf[i - 1] == '\r' && buf[i] == '\n') return (int)(i + 1);
  }
  return -1;
}

static bool ci_starts_with(const char *s, const char *prefix) {
  while (*prefix) {
    if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) return false;
    s++;
    prefix++;
  }
  return true;
}

static int get_content_length(const char *headers) {
  const char *p = headers;
  while (*p) {
    if (ci_starts_with(p, "content-length:")) {
      return atoi(p + 15);
    }
    p++;
  }
  return 0;
}

static bool json_get_string(const char *body, const char *key, char *out, size_t out_size) {
  char pattern[40];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char *p = strstr(body, pattern);
  if (!p) return false;
  p = strchr(p + strlen(pattern), ':');
  if (!p) return false;
  p++;
  while (*p == ' ') p++;
  if (*p != '"') return false;
  p++;
  size_t i = 0;
  while (*p && *p != '"' && i < out_size - 1) out[i++] = *p++;
  out[i] = '\0';
  return true;
}

static bool json_get_int(const char *body, const char *key, int *out) {
  char pattern[40];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char *p = strstr(body, pattern);
  if (!p) return false;
  p = strchr(p + strlen(pattern), ':');
  if (!p) return false;
  p++;
  while (*p == ' ') p++;
  *out = atoi(p);
  return true;
}

static void parse_ip(const char *s, uint8_t out[4]) {
  int a, b, c, d;
  if (sscanf(s, "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
    out[0] = (uint8_t)a;
    out[1] = (uint8_t)b;
    out[2] = (uint8_t)c;
    out[3] = (uint8_t)d;
  }
}

static bool query_get_int(const char *query, const char *key, int *out) {
  if (!query) return false;
  char pattern[40];
  snprintf(pattern, sizeof(pattern), "%s=", key);
  const char *p = strstr(query, pattern);
  if (!p) return false;
  *out = atoi(p + strlen(pattern));
  return true;
}

static void send_response(EthernetClient &client, int status, const char *status_text,
                           const char *content_type, const char *body, size_t body_len) {
  char header[256];
  int header_len = snprintf(header, sizeof(header),
      "HTTP/1.1 %d %s\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %u\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "Connection: close\r\n"
      "\r\n",
      status, status_text, content_type, (unsigned)body_len);

  client.write((const uint8_t *)header, header_len);
  client.write((const uint8_t *)body, body_len);
}

static void send_json(EthernetClient &client, int status, const char *status_text, const char *body) {
  send_response(client, status, status_text, "application/json", body, strlen(body));
}

static void send_html(EthernetClient &client, const char *html) {
  send_response(client, 200, "OK", "text/html", html, strlen(html));
}

static void handle_status(WebContext *ctx, EthernetClient &client) {
  EncoderState *encoders = ctx->encoders;

  const char *mode;
  if (ctx->cal_session->state != CAL_STATE_IDLE && ctx->cal_session->state != CAL_STATE_COMPLETE) {
    mode = "calibrating";
  } else if (ctx->cal_data->valid) {
    mode = g_streaming ? "streaming" : "ready";
  } else {
    mode = "uncalibrated";
  }

  NetworkStats stats;
  network_get_statistics(&stats);

  bool connected = encoders[0].enabled || encoders[1].enabled || encoders[2].enabled;

  char body[512];
  snprintf(body, sizeof(body),
      "{\"mode\":\"%s\",\"streaming\":%s,\"connected\":%s,"
      "\"focus_mm\":%.2f,\"iris_fstop\":%.2f,\"zoom_mm\":%.2f,"
      "\"focus_angle\":%u,\"iris_angle\":%u,\"zoom_angle\":%u,"
      "\"frames_sent\":%lu,\"errors\":%lu,\"uptime\":%lu,\"fps\":%.1f}",
      mode, g_streaming ? "true" : "false", connected ? "true" : "false",
      math_engine_get_position_mm(&encoders[0]), math_engine_get_position_mm(&encoders[1]),
      math_engine_get_position_mm(&encoders[2]), encoders[0].filtered_angle, encoders[1].filtered_angle,
      encoders[2].filtered_angle, (unsigned long)stats.packets_sent, (unsigned long)stats.packets_dropped,
      (unsigned long)network_get_uptime_seconds(), network_get_actual_fps());

  send_json(client, 200, "OK", body);
}

static void handle_encoders(WebContext *ctx, EthernetClient &client) {
  EncoderState *e = ctx->encoders;
  char body[400];
  snprintf(body, sizeof(body),
      "{\"encoders\":["
      "{\"id\":0,\"name\":\"focus\",\"angle\":%u,\"mm\":%.2f,\"enabled\":%s},"
      "{\"id\":1,\"name\":\"iris\",\"angle\":%u,\"fstop\":%.2f,\"enabled\":%s},"
      "{\"id\":2,\"name\":\"zoom\",\"angle\":%u,\"mm\":%.2f,\"enabled\":%s}"
      "]}",
      e[0].filtered_angle, math_engine_get_position_mm(&e[0]), e[0].enabled ? "true" : "false",
      e[1].filtered_angle, math_engine_get_position_mm(&e[1]), e[1].enabled ? "true" : "false",
      e[2].filtered_angle, math_engine_get_position_mm(&e[2]), e[2].enabled ? "true" : "false");
  send_json(client, 200, "OK", body);
}

static void handle_cal_status(WebContext *ctx, EthernetClient &client) {
  CalibrationSession *s = ctx->cal_session;
  char body[300];
  snprintf(body, sizeof(body),
      "{\"state\":\"%s\",\"calibrated\":%s,\"focus_calibrated\":%s,\"iris_calibrated\":%s,"
      "\"zoom_calibrated\":%s,\"lens_name\":\"%s\"}",
      calibration_get_state_name(s->state), ctx->cal_data->valid ? "true" : "false",
      s->temp_encoders[0].calibrated ? "true" : "false", s->temp_encoders[1].calibrated ? "true" : "false",
      s->temp_encoders[2].calibrated ? "true" : "false", s->active_lens ? s->active_lens->name : "");
  send_json(client, 200, "OK", body);
}

static void handle_cal_start(WebContext *ctx, const char *query, EthernetClient &client) {
  int preset = 10;
  query_get_int(query, "preset", &preset);

  calibration_start_session(ctx->cal_session, ctx->cal_data);
  calibration_select_preset(ctx->cal_session, preset, &g_active_profile);
  uint8_t mask = 0;
  calibration_detect_encoders(ctx->cal_session, &mask);

  send_json(client, 200, "OK", "{\"success\":true}");
}

// ignores whatever the browser thinks the step is trusts session state instead
static void handle_cal_point(WebContext *ctx, EthernetClient &client) {
  CalibrationSession *s = ctx->cal_session;
  uint16_t angle;
  if (!encoder_read_angle(s->current_encoder, &angle)) {
    send_json(client, 200, "OK", "{\"success\":false,\"message\":\"encoder read failed\"}");
    return;
  }

  int result = calibration_set_point(s, s->current_encoder, angle);
  char body[64];
  snprintf(body, sizeof(body), "{\"success\":%s}", result == CAL_OK ? "true" : "false");
  send_json(client, 200, "OK", body);
}

static void handle_cal_save(WebContext *ctx, EthernetClient &client) {
  int result = calibration_end_session(ctx->cal_session, true);
  char body[64];
  snprintf(body, sizeof(body), "{\"success\":%s}", result == CAL_OK ? "true" : "false");
  send_json(client, 200, "OK", body);
}

static void handle_cal_stop(WebContext *ctx, EthernetClient &client) {
  calibration_abort_session(ctx->cal_session);
  send_json(client, 200, "OK", "{\"success\":true}");
}

static void handle_profiles_list(EthernetClient &client) {
  ProfileSummary summaries[PROFILE_MAX_SLOTS];
  int count = profiles_list(summaries, PROFILE_MAX_SLOTS, NULL);

  char body[800];
  int len = snprintf(body, sizeof(body), "{\"profiles\":[");
  for (int i = 0; i < count; i++) {
    len += snprintf(body + len, sizeof(body) - len, "%s{\"id\":%d,\"name\":\"%s\"}", i > 0 ? "," : "",
                     summaries[i].slot, summaries[i].name);
  }
  snprintf(body + len, sizeof(body) - len, "]}");
  send_json(client, 200, "OK", body);
}

static void handle_profiles_load(WebContext *ctx, const char *body, EthernetClient &client) {
  (void)ctx;
  int id = -1;
  json_get_int(body, "id", &id);
  int result = (id >= 0) ? profiles_quick_load(id) : -1;
  char resp[64];
  snprintf(resp, sizeof(resp), "{\"success\":%s}", result == 0 ? "true" : "false");
  send_json(client, 200, "OK", resp);
}

static void handle_network_get(EthernetClient &client) {
  IPAddress ip = Ethernet.localIP();
  IPAddress subnet = Ethernet.subnetMask();
  IPAddress gateway = Ethernet.gatewayIP();

  char body[256];
  snprintf(body, sizeof(body),
      "{\"ip\":\"%u.%u.%u.%u\",\"subnet\":\"%u.%u.%u.%u\",\"gateway\":\"%u.%u.%u.%u\"}",
      ip[0], ip[1], ip[2], ip[3], subnet[0], subnet[1], subnet[2], subnet[3], gateway[0], gateway[1],
      gateway[2], gateway[3]);
  send_json(client, 200, "OK", body);
}

static void handle_network_set(const char *body, EthernetClient &client) {
  NetworkConfig config;
  network_get_default_config(&config);
  config.camera_id = network_get_camera_id();
  config.frame_rate = network_get_frame_rate();
  config.dhcp_enabled = false;

  char ip_str[32];
  if (json_get_string(body, "ip", ip_str, sizeof(ip_str))) {
    parse_ip(ip_str, config.ip_address);
    config.gateway[0] = config.ip_address[0];
    config.gateway[1] = config.ip_address[1];
    config.gateway[2] = config.ip_address[2];
    config.gateway[3] = 1;
    config.subnet_mask[0] = 255;
    config.subnet_mask[1] = 255;
    config.subnet_mask[2] = 255;
    config.subnet_mask[3] = 0;
  }

  char mc_str[32];
  if (json_get_string(body, "multicast", mc_str, sizeof(mc_str))) {
    parse_ip(mc_str, config.dest_ip);
  }

  int port;
  if (json_get_int(body, "port", &port)) config.dest_port = (uint16_t)port;

  int result = network_reconfigure(&config);
  char resp[100];
  snprintf(resp, sizeof(resp), "{\"success\":%s,\"message\":\"%s\"}", result == 0 ? "true" : "false",
           result == 0 ? "network updated" : "reconfigure failed");
  send_json(client, 200, "OK", resp);
}

static void handle_freed_get(EthernetClient &client) {
  char body[100];
  snprintf(body, sizeof(body), "{\"camera_id\":%u,\"frame_rate\":%u}", network_get_camera_id(),
           network_get_frame_rate());
  send_json(client, 200, "OK", body);
}

static void handle_freed_set(const char *body, EthernetClient &client) {
  int camera_id, frame_rate;
  if (json_get_int(body, "camera_id", &camera_id)) network_set_camera_id((uint8_t)camera_id);
  if (json_get_int(body, "frame_rate", &frame_rate)) network_set_frame_rate((uint8_t)frame_rate);
  send_json(client, 200, "OK", "{\"success\":true}");
}

static void handle_stream_start(EthernetClient &client) {
  g_streaming = true;
  send_json(client, 200, "OK", "{\"success\":true,\"message\":\"streaming started\"}");
}

static void handle_stream_stop(EthernetClient &client) {
  g_streaming = false;
  send_json(client, 200, "OK", "{\"success\":true,\"message\":\"streaming stopped\"}");
}

static void handle_encoders_reset(WebContext *ctx, EthernetClient &client) {
  for (int i = 0; i < 3; i++) {
    ctx->encoders[i].cal_min = ctx->encoders[i].original_cal_min;
    ctx->encoders[i].cal_max = ctx->encoders[i].original_cal_max;
    ctx->encoders[i].overshoot_detected = false;
  }
  send_json(client, 200, "OK", "{\"success\":true}");
}

static void handle_fleet(EthernetClient &client) {
  FleetPeer peers[FLEET_MAX_PEERS];
  int count = fleet_get_peers(peers, FLEET_MAX_PEERS);

  char body[700];
  int len = snprintf(body, sizeof(body), "{\"conflict\":%s,\"peers\":[",
                      fleet_camera_id_conflict() ? "true" : "false");
  for (int i = 0; i < count; i++) {
    len += snprintf(body + len, sizeof(body) - len, "%s{\"ip\":\"%u.%u.%u.%u\",\"name\":\"%s\",\"camera_id\":%u}",
                     i > 0 ? "," : "", peers[i].ip[0], peers[i].ip[1], peers[i].ip[2], peers[i].ip[3],
                     peers[i].name, peers[i].camera_id);
  }
  snprintf(body + len, sizeof(body) - len, "]}");
  send_json(client, 200, "OK", body);
}

static void handle_reboot(EthernetClient &client) {
  send_json(client, 200, "OK", "{\"success\":true}");
  client.flush();
  delay(100);
  // magic reset word for cortex m7 not teensy specific
  SCB_AIRCR = 0x05FA0004;
}

static void dispatch(WebContext *ctx, const char *method, char *path, const char *body, EthernetClient &client) {
  char *query = strchr(path, '?');
  if (query) {
    *query = '\0';
    query++;
  }

  bool is_get = strcmp(method, "GET") == 0;
  bool is_post = strcmp(method, "POST") == 0;

  if (is_get && (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0)) {
    send_html(client, web_content_index_html());
  } else if (is_get && strcmp(path, "/api/status") == 0) {
    handle_status(ctx, client);
  } else if (is_get && strcmp(path, "/api/encoders") == 0) {
    handle_encoders(ctx, client);
  } else if (is_get && strcmp(path, "/api/calibration/status") == 0) {
    handle_cal_status(ctx, client);
  } else if (is_post && strcmp(path, "/api/calibration/start") == 0) {
    handle_cal_start(ctx, query, client);
  } else if (is_post && strcmp(path, "/api/calibration/set") == 0) {
    handle_cal_point(ctx, client);
  } else if (is_post && strcmp(path, "/api/calibration/save") == 0) {
    handle_cal_save(ctx, client);
  } else if (is_post && strcmp(path, "/api/calibration/stop") == 0) {
    handle_cal_stop(ctx, client);
  } else if (is_get && strcmp(path, "/api/profiles") == 0) {
    handle_profiles_list(client);
  } else if (is_post && strcmp(path, "/api/profiles/load") == 0) {
    handle_profiles_load(ctx, body, client);
  } else if (is_get && strcmp(path, "/api/network") == 0) {
    handle_network_get(client);
  } else if (is_post && strcmp(path, "/api/network") == 0) {
    handle_network_set(body, client);
  } else if (is_get && strcmp(path, "/api/freed") == 0) {
    handle_freed_get(client);
  } else if (is_post && strcmp(path, "/api/freed") == 0) {
    handle_freed_set(body, client);
  } else if (is_post && strcmp(path, "/api/stream/start") == 0) {
    handle_stream_start(client);
  } else if (is_post && strcmp(path, "/api/stream/stop") == 0) {
    handle_stream_stop(client);
  } else if (is_post && strcmp(path, "/api/encoders/reset") == 0) {
    handle_encoders_reset(ctx, client);
  } else if (is_get && strcmp(path, "/api/fleet") == 0) {
    handle_fleet(client);
  } else if (is_post && strcmp(path, "/api/system/reboot") == 0) {
    handle_reboot(client);
  } else {
    send_json(client, 404, "Not Found", "{\"error\":\"not found\"}");
  }
}

bool web_is_streaming() {
  return g_streaming;
}

int web_interface_init() {
  g_server.begin();
  Serial.printf("Web: listening on port %d\n", WEB_SERVER_PORT);
  return 0;
}

static void process_slot(WebContext *ctx, ClientSlot &slot) {
  int avail = slot.client.available();
  if (avail > 0) {
    size_t space = sizeof(slot.buffer) - 1 - slot.buffer_len;
    size_t to_read = (size_t)avail < space ? (size_t)avail : space;
    slot.buffer_len += slot.client.read((uint8_t *)slot.buffer + slot.buffer_len, to_read);
    slot.buffer[slot.buffer_len] = '\0';
  }

  int headers_end = find_headers_end(slot.buffer, slot.buffer_len);
  if (headers_end < 0) {
    if (!slot.client.connected() || slot.buffer_len >= sizeof(slot.buffer) - 1) {
      slot.client.stop();
      slot.active = false;
    }
    return;
  }

  int content_length = get_content_length(slot.buffer);
  size_t total_needed = (size_t)headers_end + (size_t)content_length;
  if (slot.buffer_len < total_needed && slot.buffer_len < sizeof(slot.buffer) - 1) {
    return;
  }

  char method[8] = {0};
  char path[256] = {0};
  sscanf(slot.buffer, "%7s %255s", method, path);

  const char *body = slot.buffer + headers_end;

  dispatch(ctx, method, path, body, slot.client);

  slot.client.stop();
  slot.active = false;
}

void web_interface_process(WebContext *ctx) {
  EthernetClient new_client = g_server.accept();
  if (new_client) {
    bool placed = false;
    for (int i = 0; i < WEB_MAX_CLIENTS; i++) {
      if (!g_slots[i].active) {
        g_slots[i].client = new_client;
        g_slots[i].buffer_len = 0;
        g_slots[i].active = true;
        placed = true;
        break;
      }
    }
    if (!placed) new_client.stop();
  }

  for (int i = 0; i < WEB_MAX_CLIENTS; i++) {
    if (g_slots[i].active) process_slot(ctx, g_slots[i]);
  }
}
