#pragma once

#include <Arduino.h>
#include "calibration.h"
#include "lens_profiles.h"

#define STORAGE_FS_SIZE (1 * 1024 * 1024)

int storage_init();
int storage_load_calibration(CalibrationData *cal);
int storage_factory_reset(uint32_t confirm);
