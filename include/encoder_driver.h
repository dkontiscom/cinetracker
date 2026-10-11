#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#define AS5048_CMD_NOP 0x0000
#define AS5048_CMD_ANGLE 0x3FFF
#define AS5048_CMD_MAGNITUDE 0x3FFE
#define AS5048_CMD_DIAGNOSTICS 0x3FFD
#define AS5048_CMD_CLEAR_ERROR 0x0001
#define AS5048_READ_FLAG 0x4000
#define AS5048_PARITY_BIT 0x8000
#define AS5048_EF_ERROR 0x4000
#define AS5048_RESOLUTION 16384

#define ENCODER_SPI_BAUDRATE 8000000

// focus as5048a spi zoom as5048a spi1 iris as5048b i2c see architecture md
// spi2 pins arent usable theyre the sdcard pads not a normal header
#define ENCODER_CS_FOCUS 10
#define ENCODER_CS_ZOOM 0

// as5048b register map same 14bit values split high/low as the spi version
// just addressed differently confirmed against the real datasheet and the
// ams_as5048b arduino library not guessed
#define AS5048B_I2C_ADDRESS 0x40
#define AS5048B_REG_AGC 0xFA
#define AS5048B_REG_DIAG 0xFB
#define AS5048B_REG_MAGNITUDE_MSB 0xFC
#define AS5048B_REG_MAGNITUDE_LSB 0xFD
#define AS5048B_REG_ANGLE_MSB 0xFE
#define AS5048B_REG_ANGLE_LSB 0xFF

uint16_t as5048_calculate_parity(uint16_t value);

int encoder_driver_init();
uint8_t encoder_detect_presence();

int encoder_start_sampling(uint8_t encoder_mask, uint32_t sample_rate_hz);
void encoder_stop_sampling();

// call every loop iteration polls iris over i2c on its own timer instead
// of inside the spi hw timer isr real reports of intervaltimer wedging
// the i2c bus if wire is called from inside another interrupt
void encoder_process();

// read every active encoder right now, through the same unwrap as the
// timer samples. call it right before building a FreeD packet so the
// packet carries a reading a couple of hundred us old at most, instead of
// the last 1 kHz timer sample which can be up to 1 ms old
void encoder_refresh_now();

bool encoder_read_angle(uint8_t encoder_id, uint16_t *angle);
// unwrapped is the multi turn position accumulated at the sample rate, the
// raw 14 bit value wraps every revolution and is only kept for display
bool encoder_get_latest_sample(uint8_t encoder_id, uint16_t *angle, int32_t *unwrapped, uint32_t *sample_count);

int encoder_get_diagnostics(uint8_t encoder_id, uint16_t *agc, uint16_t *magnitude);
int encoder_collect_calibration_samples(uint8_t encoder_id, uint16_t *samples, size_t count);
