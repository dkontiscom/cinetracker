#include <Arduino.h>
#include <string.h>

#include "math_engine.h"
#include "calibration.h"
#include "encoder_driver.h"
#include "storage.h"
#include "network_stack.h"
#include "web_interface.h"
#include "fleet.h"
#include "ptp_client.h"
#include "genlock.h"
#include "aes67.h"
#include "status_leds.h"
#include "reset_button.h"

static EncoderState g_encoders[3];
static CalibrationSession g_cal_session;
static CalibrationData g_cal_data;
static WebContext g_web_ctx;
static bool g_unexpected_reboot = false;

// dirty unless a previous boot proved otherwise a missing flag file (very
// first boot ever) doesnt count as a crash theres nothing to compare to
static void check_clean_shutdown() {
  bool clean = false;
  if (storage_load_clean_shutdown(&clean) == 0 && !clean) {
    g_unexpected_reboot = true;
    Serial.println("Main: previous shutdown was not clean flagging it");
  }
  storage_set_clean_shutdown(false);
}

static void load_saved_calibration() {
  if (storage_load_calibration(&g_cal_data) != 0) return;

  for (int i = 0; i < 3; i++) {
    if (g_cal_data.encoders[i].calibrated) {
      apply_calibration(&g_encoders[i], &g_cal_data.encoders[i]);
    }
  }

  Serial.println("Main: restored calibration from storage");
}

// math_engine_init_state already defaults every encoder to FILTER_EMA, this
// only needs to act when a saved setting actually turned it off
static void load_saved_filter_setting() {
  bool enabled = true;
  if (storage_load_filter_enabled(&enabled) != 0 || enabled) return;

  for (int i = 0; i < 3; i++) {
    math_engine_set_filter(&g_encoders[i], FILTER_NONE);
  }
  Serial.println("Main: filtering disabled by saved setting");
}

void setup() {
  Serial.begin(115200);
  // 2s cap not infinite wait so it still boots with no usb cable plugged in
  uint32_t wait_start = millis();
  while (!Serial && millis() - wait_start < 2000) {
  }

  Serial.println("CineTracker starting");

  storage_init();
  check_clean_shutdown();

  for (int i = 0; i < 3; i++) {
    math_engine_init_state(&g_encoders[i], i);
  }

  encoder_driver_init();
  uint8_t detected_mask = encoder_detect_presence();

  memset(&g_cal_session, 0, sizeof(g_cal_session));
  memset(&g_cal_data, 0, sizeof(g_cal_data));
  load_saved_calibration();
  load_saved_filter_setting();

  encoder_start_sampling(detected_mask, 1000);

  NetworkConfig net_config;
  if (storage_load_network_config(&net_config) != 0) {
    network_get_default_config(&net_config);
  }
  network_stack_init(&net_config);

  uint8_t genlock_fps;
  if (storage_load_genlock_fps(&genlock_fps) != 0) {
    genlock_fps = GENLOCK_DEFAULT_FPS;
  }
  genlock_init(genlock_fps);

  web_interface_init();

  Aes67Config aes67_config = {};
  aes67_config.dest_ip[0] = 239;
  aes67_config.dest_ip[1] = 69;
  aes67_config.dest_ip[2] = 0;
  aes67_config.dest_ip[3] = 1;
  aes67_config.dest_port = 5004;
  strncpy(aes67_config.stream_name, "cinetracker-ltc", sizeof(aes67_config.stream_name) - 1);
  aes67_init(&aes67_config);

  Aes67Settings aes67_settings;
  if (storage_load_aes67_settings(&aes67_settings) == 0) {
    aes67_set_destination(aes67_settings.dest_ip, aes67_settings.dest_port);
    aes67_set_direction((Aes67Direction)aes67_settings.direction);
  }

  char device_name[32];
  snprintf(device_name, sizeof(device_name), "cinetracker-%u", network_get_camera_id());
  fleet_init(device_name, network_get_camera_id());

  g_web_ctx.encoders = g_encoders;
  g_web_ctx.cal_session = &g_cal_session;
  g_web_ctx.cal_data = &g_cal_data;
  g_web_ctx.unexpected_reboot = &g_unexpected_reboot;

  status_leds_init();
  reset_button_init();

  Serial.println("CineTracker ready");
}

void loop() {
  for (int i = 0; i < 3; i++) {
    uint16_t angle;
    if (encoder_get_latest_sample(i, &angle, nullptr)) {
      process_encoder_sample(&g_encoders[i], angle);
    }
  }

  if (web_is_streaming()) {
    network_process_frame(g_encoders);
  }

  web_interface_process(&g_web_ctx);
  encoder_process();
  fleet_process();
  ptp_client_process();
  aes67_process();
  status_leds_process();
  reset_button_process();
}
