#pragma once

#include <Arduino.h>
#include "calibration.h"

#define STORAGE_FS_SIZE (1 * 1024 * 1024)

int storage_init();
int storage_load_calibration(CalibrationData *cal);
int storage_save_calibration(const CalibrationData *cal);
int storage_factory_reset(uint32_t confirm);

// clean_shutdown is written false at the start of every boot and true only
// right before a deliberate reset so the next boot can tell a crash apart
// from a normal restart see main cpp and reset_button cpp
int storage_load_clean_shutdown(bool *clean);
int storage_set_clean_shutdown(bool clean);
