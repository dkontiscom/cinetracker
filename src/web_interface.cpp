#include "web_interface.h"

#include <QNEthernet.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "encoder_driver.h"
#include "network_stack.h"
#include "web_content.h"
#include "fleet.h"
#include "aes67.h"
#include "genlock.h"
#include "storage.h"

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

static bool json_get_float(const char *body, const char *key, float *out) {
  char pattern[40];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char *p = strstr(body, pattern);
  if (!p) return false;
  p = strchr(p + strlen(pattern), ':');
  if (!p) return false;
  p++;
  while (*p == ' ') p++;
  *out = strtof(p, nullptr);
  return true;
}

static bool json_get_bool(const char *body, const char *key, bool *out) {
  char pattern[40];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char *p = strstr(body, pattern);
  if (!p) return false;
  p = strchr(p + strlen(pattern), ':');
  if (!p) return false;
  p++;
  while (*p == ' ') p++;
  *out = (strncmp(p, "true", 4) == 0);
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

// rough physical estimate for display only, see calibration_angle_to_phys
// not every encoder has enough typed reference points to produce one, and
// focus reports infinite instead of a value past its mid point
static void get_phys_display(const CalibrationData *cal, uint8_t encoder_id, int32_t angle,
                              float *value, bool *valid, bool *is_infinite) {
  *is_infinite = false;
  *value = 0.0f;
  *valid = calibration_angle_to_phys(cal, encoder_id, angle, value, is_infinite);
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
  bool unexpected_reboot = ctx->unexpected_reboot && *ctx->unexpected_reboot;

  float focus_mm, iris_fstop, zoom_mm;
  bool focus_valid, focus_inf, iris_valid, iris_inf, zoom_valid, zoom_inf;
  get_phys_display(ctx->cal_data, 0, encoders[0].filtered_angle, &focus_mm, &focus_valid, &focus_inf);
  get_phys_display(ctx->cal_data, 1, encoders[1].filtered_angle, &iris_fstop, &iris_valid, &iris_inf);
  get_phys_display(ctx->cal_data, 2, encoders[2].filtered_angle, &zoom_mm, &zoom_valid, &zoom_inf);

  char body[700];
  snprintf(body, sizeof(body),
      "{\"mode\":\"%s\",\"streaming\":%s,\"connected\":%s,\"synced\":%s,"
      "\"unexpected_reboot\":%s,"
      "\"focus_mm\":%.2f,\"focus_mm_valid\":%s,\"focus_mm_infinite\":%s,"
      "\"iris_fstop\":%.2f,\"iris_fstop_valid\":%s,"
      "\"zoom_mm\":%.2f,\"zoom_mm_valid\":%s,"
      "\"focus_angle\":%ld,\"iris_angle\":%ld,\"zoom_angle\":%ld,"
      "\"frames_sent\":%lu,\"errors\":%lu,\"uptime\":%lu,\"fps\":%.1f}",
      mode, g_streaming ? "true" : "false", connected ? "true" : "false", genlock_locked() ? "true" : "false",
      unexpected_reboot ? "true" : "false",
      focus_mm, focus_valid ? "true" : "false", focus_inf ? "true" : "false",
      iris_fstop, iris_valid ? "true" : "false",
      zoom_mm, zoom_valid ? "true" : "false",
      (long)encoders[0].filtered_angle, (long)encoders[1].filtered_angle, (long)encoders[2].filtered_angle,
      (unsigned long)stats.packets_sent, (unsigned long)stats.packets_dropped,
      (unsigned long)network_get_uptime_seconds(), network_get_actual_fps());

  send_json(client, 200, "OK", body);
}

static void handle_encoders(WebContext *ctx, EthernetClient &client) {
  EncoderState *e = ctx->encoders;

  float phys[3];
  bool valid[3], inf[3];
  for (int i = 0; i < 3; i++) {
    get_phys_display(ctx->cal_data, i, e[i].filtered_angle, &phys[i], &valid[i], &inf[i]);
  }

  char body[500];
  snprintf(body, sizeof(body),
      "{\"encoders\":["
      "{\"id\":0,\"name\":\"focus\",\"angle\":%ld,\"mm\":%.2f,\"mm_valid\":%s,\"mm_infinite\":%s,\"enabled\":%s},"
      "{\"id\":1,\"name\":\"iris\",\"angle\":%ld,\"fstop\":%.2f,\"fstop_valid\":%s,\"enabled\":%s},"
      "{\"id\":2,\"name\":\"zoom\",\"angle\":%ld,\"mm\":%.2f,\"mm_valid\":%s,\"enabled\":%s}"
      "]}",
      (long)e[0].filtered_angle, phys[0], valid[0] ? "true" : "false", inf[0] ? "true" : "false",
      e[0].enabled ? "true" : "false",
      (long)e[1].filtered_angle, phys[1], valid[1] ? "true" : "false", e[1].enabled ? "true" : "false",
      (long)e[2].filtered_angle, phys[2], valid[2] ? "true" : "false", e[2].enabled ? "true" : "false");
  send_json(client, 200, "OK", body);
}

static void handle_cal_status(WebContext *ctx, EthernetClient &client) {
  CalibrationSession *s = ctx->cal_session;

  // calibration_validate can join multiple lines with real newlines, which
  // isnt valid inside a json string, flatten to spaces before sending
  char error_flat[256];
  strncpy(error_flat, s->state == CAL_STATE_ERROR ? s->error_message : "", sizeof(error_flat) - 1);
  error_flat[sizeof(error_flat) - 1] = '\0';
  for (char *p = error_flat; *p; p++) {
    if (*p == '\n' || *p == '"') *p = ' ';
  }

  char body[600];
  snprintf(body, sizeof(body),
      "{\"state\":\"%s\",\"instructions\":\"%s\",\"progress\":%u,\"current_encoder\":%u,"
      "\"calibrated\":%s,\"focus_calibrated\":%s,\"iris_calibrated\":%s,\"zoom_calibrated\":%s,"
      "\"focus_wanted\":%s,\"iris_wanted\":%s,\"zoom_wanted\":%s,\"error\":\"%s\"}",
      calibration_get_state_name(s->state), calibration_get_instructions(s->state), s->progress_percent,
      s->current_encoder, ctx->cal_data->valid ? "true" : "false",
      s->temp_encoders[0].calibrated ? "true" : "false", s->temp_encoders[1].calibrated ? "true" : "false",
      s->temp_encoders[2].calibrated ? "true" : "false", (s->encoder_wanted_mask & 1) ? "true" : "false",
      (s->encoder_wanted_mask & 2) ? "true" : "false", (s->encoder_wanted_mask & 4) ? "true" : "false",
      error_flat);
  send_json(client, 200, "OK", body);
}

// which rings to calibrate is the operator's manual choice, not hardware
// auto detection - a prime lens genuinely has no zoom ring to detect, and
// spi absence detection (focus/zoom) isnt reliable enough to gate on
// anyway, only i2c (iris) can tell "not connected" from "connected" for sure
static void handle_cal_start(WebContext *ctx, const char *body, EthernetClient &client) {
  bool want_focus = true, want_iris = true, want_zoom = true;
  json_get_bool(body, "focus", &want_focus);
  json_get_bool(body, "iris", &want_iris);
  json_get_bool(body, "zoom", &want_zoom);

  uint8_t mask = (want_focus ? 1 : 0) | (want_iris ? 2 : 0) | (want_zoom ? 4 : 0);
  if (mask == 0) {
    send_json(client, 200, "OK", "{\"success\":false,\"message\":\"select at least one encoder\"}");
    return;
  }

  calibration_start_session(ctx->cal_session, ctx->cal_data, mask);
  uint8_t detected = 0;
  calibration_detect_encoders(ctx->cal_session, &detected);

  send_json(client, 200, "OK", "{\"success\":true}");
}

// reads the live continuously unwrapped position not a fresh raw read so
// a multi turn gear ratio between calibration points is never lost see
// architecture md for why this cant be a one shot encoder_read_angle
static void handle_cal_point(WebContext *ctx, const char *body, EthernetClient &client) {
  CalibrationSession *s = ctx->cal_session;
  EncoderState *enc = &ctx->encoders[s->current_encoder];
  if (enc->sample_count == 0) {
    send_json(client, 200, "OK", "{\"success\":false,\"message\":\"encoder read failed\"}");
    return;
  }

  const char *completed_name = calibration_get_state_name(s->state);
  float phys_value = 0.0f;
  bool has_phys = json_get_float(body, "phys_value", &phys_value);

  int result = calibration_set_point(s, s->current_encoder, enc->unwrapped_angle, has_phys, phys_value);

  char out[350];
  snprintf(out, sizeof(out),
      "{\"success\":%s,\"completed\":\"%s\",\"state\":\"%s\",\"instructions\":\"%s\",\"progress\":%u}",
      result == CAL_OK ? "true" : "false", completed_name, calibration_get_state_name(s->state),
      calibration_get_instructions(s->state), s->progress_percent);
  send_json(client, 200, "OK", out);
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

static void handle_network_get(EthernetClient &client) {
  IPAddress ip = Ethernet.localIP();
  IPAddress subnet = Ethernet.subnetMask();
  IPAddress gateway = Ethernet.gatewayIP();

  // multicast/port are the freed stream's destination (NetworkConfig
  // dest_ip/dest_port), handle_network_set already accepts and saves
  // them together with ip - this is just the matching read back, the web
  // ui had nothing to load these two from before
  NetworkConfig config;
  network_get_current_config(&config);

  char body[300];
  snprintf(body, sizeof(body),
      "{\"ip\":\"%u.%u.%u.%u\",\"subnet\":\"%u.%u.%u.%u\",\"gateway\":\"%u.%u.%u.%u\","
      "\"multicast\":\"%u.%u.%u.%u\",\"port\":%u}",
      ip[0], ip[1], ip[2], ip[3], subnet[0], subnet[1], subnet[2], subnet[3], gateway[0], gateway[1],
      gateway[2], gateway[3], config.dest_ip[0], config.dest_ip[1], config.dest_ip[2], config.dest_ip[3],
      config.dest_port);
  send_json(client, 200, "OK", body);
}

static void handle_network_set(const char *body, EthernetClient &client) {
  // starts from the live config not defaults otherwise a field left out
  // of this request would silently reset to its default instead of
  // staying whatever it was already set to
  NetworkConfig config;
  network_get_current_config(&config);
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
  if (result == 0) storage_save_network_config(&config);

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
  bool camera_id_changed = json_get_int(body, "camera_id", &camera_id);
  bool frame_rate_changed = json_get_int(body, "frame_rate", &frame_rate);
  if (camera_id_changed) {
    network_set_camera_id((uint8_t)camera_id);
    // fleet.cpp caches this at boot for its announce broadcast and
    // conflict check, both go stale unless told about a live change too
    fleet_set_camera_id((uint8_t)camera_id);
  }
  if (frame_rate_changed) network_set_frame_rate((uint8_t)frame_rate);

  // camera id doubles as the device id fleet conflict detection depends
  // on it and frame_rate is freed metadata both live in NetworkConfig so
  // either change needs the same save
  if (camera_id_changed || frame_rate_changed) {
    NetworkConfig config;
    network_get_current_config(&config);
    storage_save_network_config(&config);
  }

  send_json(client, 200, "OK", "{\"success\":true,\"message\":\"freed settings updated\"}");
}

static const char *aes67_direction_name(Aes67Direction dir) {
  if (dir == AES67_LTC_IN) return "ltc_in";
  if (dir == AES67_LTC_OUT) return "ltc_out";
  return "off";
}

static Aes67Direction aes67_direction_from_name(const char *name) {
  if (strcmp(name, "ltc_in") == 0) return AES67_LTC_IN;
  if (strcmp(name, "ltc_out") == 0) return AES67_LTC_OUT;
  return AES67_OFF;
}

static void handle_aes67_get(EthernetClient &client) {
  uint8_t ip[4];
  uint16_t port;
  aes67_get_destination(ip, &port);

  char body[350];
  snprintf(body, sizeof(body),
           "{\"direction\":\"%s\",\"dest_ip\":\"%u.%u.%u.%u\",\"dest_port\":%u,"
           "\"packets_sent\":%lu,\"packets_received\":%lu,\"packets_lost\":%lu,\"underruns\":%lu,"
           "\"jitter_overwrites\":%lu}",
           aes67_direction_name(aes67_get_direction()), ip[0], ip[1], ip[2], ip[3], port,
           (unsigned long)aes67_packets_sent(), (unsigned long)aes67_packets_received(),
           (unsigned long)aes67_packets_lost(), (unsigned long)aes67_underruns(),
           (unsigned long)aes67_jitter_overwrites());
  send_json(client, 200, "OK", body);
}

static void handle_aes67_set(const char *body, EthernetClient &client) {
  char direction[16];
  if (json_get_string(body, "direction", direction, sizeof(direction))) {
    aes67_set_direction(aes67_direction_from_name(direction));
  }

  char ip_str[32];
  uint8_t ip[4];
  uint16_t port;
  aes67_get_destination(ip, &port);

  bool have_ip = json_get_string(body, "dest_ip", ip_str, sizeof(ip_str));
  if (have_ip) parse_ip(ip_str, ip);

  int port_val;
  bool have_port = json_get_int(body, "dest_port", &port_val);
  if (have_port) port = (uint16_t)port_val;

  if (have_ip || have_port) {
    aes67_set_destination(ip, port);
  }

  Aes67Settings settings;
  settings.direction = (uint8_t)aes67_get_direction();
  aes67_get_destination(settings.dest_ip, &settings.dest_port);
  storage_save_aes67_settings(&settings);

  send_json(client, 200, "OK", "{\"success\":true}");
}

static void handle_genlock_get(EthernetClient &client) {
  char body[64];
  snprintf(body, sizeof(body), "{\"fps\":%u}", genlock_get_fps());
  send_json(client, 200, "OK", body);
}

// fps only takes effect after a reboot the ptp client and the tri sync
// interrupt handler are both set up once at boot not designed to be
// safely reconfigured while genlock is actively running see
// architecture md for why this wasnt made a live hot swap
static void handle_genlock_set(const char *body, EthernetClient &client) {
  int fps;
  if (!json_get_int(body, "fps", &fps) || !genlock_fps_supported((uint8_t)fps)) {
    send_json(client, 200, "OK", "{\"success\":false,\"message\":\"unsupported fps\"}");
    return;
  }

  storage_save_genlock_fps((uint8_t)fps);
  storage_set_clean_shutdown(true);
  send_json(client, 200, "OK", "{\"success\":true,\"message\":\"rebooting to apply\"}");
  client.flush();
  delay(100);
  SCB_AIRCR = 0x05FA0004;
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

// one shared on/off switch for all 3 encoders, same as it ever was - never
// exposed a choice of which filter, just ema or none, matching the old
// on/off semantics rather than inventing a new per-filter-type picker
static void handle_filter_get(WebContext *ctx, EthernetClient &client) {
  bool enabled = ctx->encoders[0].filter_type != FILTER_NONE;
  char body[40];
  snprintf(body, sizeof(body), "{\"enabled\":%s}", enabled ? "true" : "false");
  send_json(client, 200, "OK", body);
}

static void handle_filter_set(WebContext *ctx, const char *body, EthernetClient &client) {
  bool enabled = true;
  if (!json_get_bool(body, "enabled", &enabled)) {
    send_json(client, 200, "OK", "{\"success\":false,\"message\":\"missing enabled\"}");
    return;
  }

  for (int i = 0; i < 3; i++) {
    math_engine_set_filter(&ctx->encoders[i], enabled ? FILTER_EMA : FILTER_NONE);
  }
  storage_save_filter_enabled(enabled);

  send_json(client, 200, "OK", "{\"success\":true}");
}

// peer names come straight off an unauthenticated lan broadcast (fleet.cpp),
// unescaped a crafted name containing a quote or backslash could break or
// inject fields into this response - strip/escape before it ever reaches
// a %s. out_size must be at least 2x the longest input this is called
// with plus one, so a name that is entirely quotes still fits
static void json_escape(const char *in, char *out, size_t out_size) {
  size_t o = 0;
  for (size_t i = 0; in[i] != '\0' && o + 2 < out_size; i++) {
    unsigned char c = (unsigned char)in[i];
    if (c == '"' || c == '\\') {
      out[o++] = '\\';
      out[o++] = (char)c;
    } else if (c < 0x20) {
      out[o++] = ' ';
    } else {
      out[o++] = (char)c;
    }
  }
  out[o] = '\0';
}

static void handle_fleet(EthernetClient &client) {
  FleetPeer peers[FLEET_MAX_PEERS];
  int count = fleet_get_peers(peers, FLEET_MAX_PEERS);

  // formats each entry into its own small buffer first and only copies it
  // into body if it actually fits, rather than accumulating snprintf's
  // would-have-written return value against sizeof(body) - len, which
  // keeps growing past the real buffer even once its silently truncating
  // and wraps size_t - int into a huge number once it overtakes it
  char body[2000];
  size_t len = 0;

  int n = snprintf(body, sizeof(body), "{\"conflict\":%s,\"peers\":[", fleet_camera_id_conflict() ? "true" : "false");
  len += (n > 0) ? (size_t)n : 0;

  for (int i = 0; i < count; i++) {
    char safe_name[70];
    json_escape(peers[i].name, safe_name, sizeof(safe_name));

    char entry[150];
    int entry_len = snprintf(entry, sizeof(entry), "%s{\"ip\":\"%u.%u.%u.%u\",\"name\":\"%s\",\"camera_id\":%u}",
                              i > 0 ? "," : "", peers[i].ip[0], peers[i].ip[1], peers[i].ip[2], peers[i].ip[3],
                              safe_name, peers[i].camera_id);
    // stop before overflowing, always leaving room for the closing "]}"
    if (entry_len <= 0 || len + (size_t)entry_len + 3 > sizeof(body)) break;
    memcpy(body + len, entry, (size_t)entry_len);
    len += (size_t)entry_len;
  }

  n = snprintf(body + len, sizeof(body) - len, "]}");
  len += (n > 0) ? (size_t)n : 0;

  send_json(client, 200, "OK", body);
}

static void handle_reboot(EthernetClient &client) {
  storage_set_clean_shutdown(true);
  send_json(client, 200, "OK", "{\"success\":true}");
  client.flush();
  delay(100);
  // magic reset word for cortex m7 not teensy specific
  SCB_AIRCR = 0x05FA0004;
}

static void handle_ack_reboot(WebContext *ctx, EthernetClient &client) {
  if (ctx->unexpected_reboot) *ctx->unexpected_reboot = false;
  send_json(client, 200, "OK", "{\"success\":true}");
}

static void dispatch(WebContext *ctx, const char *method, char *path, const char *body, EthernetClient &client) {
  char *query = strchr(path, '?');
  if (query) *query = '\0';

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
    handle_cal_start(ctx, body, client);
  } else if (is_post && strcmp(path, "/api/calibration/set") == 0) {
    handle_cal_point(ctx, body, client);
  } else if (is_post && strcmp(path, "/api/calibration/save") == 0) {
    handle_cal_save(ctx, client);
  } else if (is_post && strcmp(path, "/api/calibration/stop") == 0) {
    handle_cal_stop(ctx, client);
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
  } else if (is_get && strcmp(path, "/api/filter") == 0) {
    handle_filter_get(ctx, client);
  } else if (is_post && strcmp(path, "/api/filter") == 0) {
    handle_filter_set(ctx, body, client);
  } else if (is_get && strcmp(path, "/api/fleet") == 0) {
    handle_fleet(client);
  } else if (is_get && strcmp(path, "/api/aes67") == 0) {
    handle_aes67_get(client);
  } else if (is_post && strcmp(path, "/api/aes67") == 0) {
    handle_aes67_set(body, client);
  } else if (is_get && strcmp(path, "/api/genlock") == 0) {
    handle_genlock_get(client);
  } else if (is_post && strcmp(path, "/api/genlock") == 0) {
    handle_genlock_set(body, client);
  } else if (is_post && strcmp(path, "/api/system/reboot") == 0) {
    handle_reboot(client);
  } else if (is_post && strcmp(path, "/api/system/ack_reboot") == 0) {
    handle_ack_reboot(ctx, client);
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
