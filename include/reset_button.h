#pragma once

#include <Arduino.h>

#define RESET_BUTTON_PIN 6
#define RESET_BUTTON_HOLD_MS 5000

void reset_button_init();
void reset_button_process();
