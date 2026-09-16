#pragma once

#include <Arduino.h>
#include <SPI.h>

#define AS5048_CMD_NOP 0x0000
#define AS5048_CMD_ANGLE 0x3FFF
#define AS5048_CMD_MAGNITUDE 0x3FFE
#define AS5048_CMD_DIAGNOSTICS 0x3FFD
#define AS5048_READ_FLAG 0x4000
#define AS5048_PARITY_BIT 0x8000
#define AS5048_EF_ERROR 0x4000
#define AS5048_RESOLUTION 16384

#define ENCODER_SPI_BAUDRATE 8000000

#define ENCODER_CS_FOCUS 2
#define ENCODER_CS_IRIS 3
#define ENCODER_CS_ZOOM 4

uint16_t as5048_calculate_parity(uint16_t value);

int encoder_driver_init();
uint8_t encoder_detect_presence();

int encoder_start_sampling(uint8_t encoder_mask, uint32_t sample_rate_hz);
void encoder_stop_sampling();

bool encoder_read_angle(uint8_t encoder_id, uint16_t *angle);
bool encoder_get_latest_sample(uint8_t encoder_id, uint16_t *angle, uint32_t *sample_count);

int encoder_get_diagnostics(uint8_t encoder_id, uint16_t *agc, uint16_t *magnitude);
int encoder_collect_calibration_samples(uint8_t encoder_id, uint16_t *samples, size_t count);
