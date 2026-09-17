#pragma once

#include <Arduino.h>
#include "math_engine.h"
#include "calibration.h"

#define WEB_SERVER_PORT 80
#define WEB_MAX_CLIENTS 4
#define WEB_REQUEST_BUFFER_SIZE 1024

struct WebContext {
  EncoderState *encoders;
  CalibrationSession *cal_session;
  CalibrationData *cal_data;
};

int web_interface_init();
void web_interface_process(WebContext *ctx);
