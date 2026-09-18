#pragma once

#include <Arduino.h>
#include "calibration.h"
#include "network_stack.h"
#include "aes67.h"

#define STORAGE_FS_SIZE (1 * 1024 * 1024)

int storage_init();
int storage_load_calibration(CalibrationData *cal);
int storage_save_calibration(const CalibrationData *cal);
int storage_factory_reset(uint32_t confirm);

int storage_load_network_config(NetworkConfig *config);
int storage_save_network_config(const NetworkConfig *config);

int storage_load_aes67_settings(Aes67Settings *settings);
int storage_save_aes67_settings(const Aes67Settings *settings);

int storage_load_genlock_fps(uint8_t *fps);
int storage_save_genlock_fps(uint8_t fps);

// clean_shutdown is written false at the start of every boot and true only
// right before a deliberate reset so the next boot can tell a crash apart
// from a normal restart see main cpp and reset_button cpp
int storage_load_clean_shutdown(bool *clean);
int storage_set_clean_shutdown(bool clean);
