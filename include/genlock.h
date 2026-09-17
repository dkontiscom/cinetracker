#pragma once

#include <Arduino.h>

// 24 25 30 or 60 tested, all compile, change this per production not at runtime
#define GENLOCK_FPS 25
#define GENLOCK_MHZ 600

#define GENLOCK_PIN_BIT0 2
#define GENLOCK_PIN_BIT1 3
#define GENLOCK_INTERRUPT_CHANNEL 2

int genlock_init();
bool genlock_locked();
