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

static EncoderState g_encoders[3];
static CalibrationSession g_cal_session;
static CalibrationData g_cal_data;
static WebContext g_web_ctx;

static void load_saved_calibration() {
  if (storage_load_calibration(&g_cal_data) != 0) return;

  for (int i = 0; i < 3; i++) {
    if (g_cal_data.encoders[i].calibrated) {
      apply_calibration(&g_encoders[i], &g_cal_data.encoders[i]);
    }
  }

  Serial.println("Main: restored calibration from storage");
}

void setup() {
  Serial.begin(115200);
  // 2s cap not infinite wait so it still boots with no usb cable plugged in
  uint32_t wait_start = millis();
  while (!Serial && millis() - wait_start < 2000) {
  }

  Serial.println("CineTracker starting");

  storage_init();

  for (int i = 0; i < 3; i++) {
    math_engine_init_state(&g_encoders[i], i);
  }

  encoder_driver_init();
  uint8_t detected_mask = encoder_detect_presence();

  memset(&g_cal_session, 0, sizeof(g_cal_session));
  memset(&g_cal_data, 0, sizeof(g_cal_data));
  load_saved_calibration();

  encoder_start_sampling(detected_mask, 1000);

  NetworkConfig net_config;
  network_get_default_config(&net_config);
  network_stack_init(&net_config);
  genlock_init();

  web_interface_init();

  Aes67Config aes67_config = {};
  aes67_config.dest_ip[0] = 239;
  aes67_config.dest_ip[1] = 69;
  aes67_config.dest_ip[2] = 0;
  aes67_config.dest_ip[3] = 1;
  aes67_config.dest_port = 5004;
  strncpy(aes67_config.stream_name, "cinetracker-ltc", sizeof(aes67_config.stream_name) - 1);
  aes67_init(&aes67_config);

  char device_name[32];
  snprintf(device_name, sizeof(device_name), "cinetracker-%u", network_get_camera_id());
  fleet_init(device_name, network_get_camera_id());

  g_web_ctx.encoders = g_encoders;
  g_web_ctx.cal_session = &g_cal_session;
  g_web_ctx.cal_data = &g_cal_data;

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
}
