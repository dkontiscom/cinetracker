#pragma once

#include <Arduino.h>

// 24 25 30 50 60 all real broadcast tri level sync rates all compiled
// into the firmware at once now see genlock cpp for the per rate
// template dispatch changing rate is a web ui setting plus a reboot
// not a reflash anymore
#define GENLOCK_DEFAULT_FPS 25
#define GENLOCK_MHZ 600

#define GENLOCK_PIN_BIT0 2
#define GENLOCK_PIN_BIT1 3
#define GENLOCK_INTERRUPT_CHANNEL 2

bool genlock_fps_supported(uint8_t fps);
int genlock_init(uint8_t fps);
bool genlock_locked();
uint8_t genlock_get_fps();
