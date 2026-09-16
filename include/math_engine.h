#pragma once

#include <Arduino.h>

#define AS5048_MAX_ANGLE 16383
#define MEDIAN_FILTER_SIZE 5
#define EMA_ALPHA_SHIFT 3
#define ENCODER_DEADBAND 2
#define KALMAN_Q 0.001f
#define KALMAN_R 0.1f
#define KALMAN_P_INITIAL 1.0f

enum FilterType {
  FILTER_NONE = 0,
  FILTER_EMA = 1,
  FILTER_MEDIAN = 2,
  FILTER_KALMAN = 3,
};

struct EncoderState {
  uint16_t raw_angle;
  uint16_t filtered_angle;
  uint16_t normalized_angle;
  int16_t velocity;

  float mm_value;
  float last_mm_value;

  uint16_t cal_min;
  uint16_t cal_max;
  uint16_t cal_range;

  float mm_min;
  float mm_max;
  float mm_scale;

  bool overshoot_detected;
  uint8_t direction;

  bool ema_initialized;
  float ema_state;

  uint16_t median_buffer[MEDIAN_FILTER_SIZE];
  uint8_t median_index;

  bool kalman_initialized;
  float kalman_x;
  float kalman_p;
  float kalman_q;
  float kalman_r;

  uint8_t filter_type;

  uint32_t sample_count;
  int32_t total_rotations;
  uint16_t max_velocity;
  uint32_t update_timestamp;

  uint8_t encoder_id;
  uint8_t enabled;
  uint8_t calibrated;
  uint8_t connected;
};

uint16_t normalize_angle_14bit(uint16_t angle);
int16_t angle_distance(uint16_t from, uint16_t to);
uint16_t apply_deadband(uint16_t current, uint16_t previous, uint16_t threshold);

uint16_t ema_filter(EncoderState *state, uint16_t input);
uint16_t median_filter(EncoderState *state, uint16_t input);
uint16_t kalman_filter(EncoderState *state, uint16_t measurement);

void handle_boundary_overshoot(EncoderState *state, uint16_t angle);
void math_process_angle(EncoderState *state, uint16_t raw_angle);
float process_encoder_sample(EncoderState *state, uint16_t raw_angle);

uint8_t auto_detect_direction(EncoderState *state, const uint16_t *samples, size_t count);
uint16_t calculate_noise_floor(const uint16_t *samples, size_t count);
float calculate_scale_factor(uint16_t angle_range, float mm_range);

void math_engine_init_state(EncoderState *state, uint8_t encoder_id);
void math_engine_reset_state(EncoderState *state);
void math_engine_set_filter(EncoderState *state, uint8_t filter_type);
void math_engine_configure_kalman(EncoderState *state, float process_noise, float measurement_noise);

bool math_engine_validate_calibration(const EncoderState *state);
void math_engine_apply_calibration(EncoderState *state, uint16_t cal_min, uint16_t cal_max,
                                    float mm_min, float mm_max, uint8_t direction);

float math_engine_get_position_mm(const EncoderState *state);
float math_engine_get_velocity_mm_per_sec(const EncoderState *state);
