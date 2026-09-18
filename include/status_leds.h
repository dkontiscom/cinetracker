#pragma once

#include <Arduino.h>

#define STATUS_LED_SYNC_PIN 4
#define STATUS_LED_STREAM_PIN 5

void status_leds_init();
void status_leds_process();
