#pragma once

#include <Arduino.h>
#include "calibration.h"
#include "lens_profiles.h"

#define STORAGE_FS_SIZE (1 * 1024 * 1024)

int storage_init();
int storage_load_calibration(CalibrationData *cal);
int storage_save_calibration(const CalibrationData *cal);
int storage_load_lens_profile(int slot, StoredLensProfile *out);
int storage_save_lens_profile(const StoredLensProfile *profile, int slot);
int storage_delete_lens_profile(int slot);
int storage_factory_reset(uint32_t confirm);
