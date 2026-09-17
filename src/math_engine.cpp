#include "math_engine.h"

#include <math.h>
#include <string.h>

uint16_t normalize_angle_14bit(uint16_t angle) {
  return angle & 0x3FFF;
}

int16_t angle_distance(uint16_t from, uint16_t to) {
  int32_t diff = (int32_t)to - (int32_t)from;
  if (diff > 8192) diff -= 16384;
  else if (diff < -8192) diff += 16384;
  return (int16_t)diff;
}

uint16_t apply_deadband(uint16_t current, uint16_t previous, uint16_t threshold) {
  int32_t diff = (int32_t)current - (int32_t)previous;
  if (diff < 0) diff = -diff;
  return (diff > threshold) ? current : previous;
}

int32_t ema_filter(EncoderState *state, int32_t input) {
  const float alpha = 1.0f / (1 << EMA_ALPHA_SHIFT);

  if (!state->ema_initialized) {
    // first read just seeds it no real filtering yet
    state->ema_state = (float)input;
    state->ema_initialized = true;
    return input;
  }

  state->ema_state += alpha * ((float)input - state->ema_state);
  return (int32_t)state->ema_state;
}

int32_t median_filter(EncoderState *state, int32_t input) {
  state->median_buffer[state->median_index] = input;
  state->median_index = (state->median_index + 1) % MEDIAN_FILTER_SIZE;

  int32_t sorted[MEDIAN_FILTER_SIZE];
  memcpy(sorted, state->median_buffer, sizeof(sorted));

#define SWAP(a, b) if (sorted[a] > sorted[b]) { int32_t tmp = sorted[a]; sorted[a] = sorted[b]; sorted[b] = tmp; }
  SWAP(0, 1); SWAP(3, 4);
  SWAP(0, 2); SWAP(1, 2);
  SWAP(0, 3); SWAP(2, 4);
  SWAP(1, 3); SWAP(2, 3);
  SWAP(1, 2);
#undef SWAP

  return sorted[2];
}

int32_t kalman_filter(EncoderState *state, int32_t measurement) {
  if (!state->kalman_initialized) {
    state->kalman_x = (float)measurement;
    state->kalman_p = KALMAN_P_INITIAL;
    state->kalman_initialized = true;
    return measurement;
  }

  state->kalman_p += state->kalman_q;

  float k = state->kalman_p / (state->kalman_p + state->kalman_r);
  state->kalman_x += k * ((float)measurement - state->kalman_x);
  state->kalman_p *= (1.0f - k);

  return (int32_t)state->kalman_x;
}

// cal_min/cal_max arent numerically ordered direction decides which one is
// the physical low end so the window has to move on whichever side got
// passed not just whichever field happens to hold the bigger number
void handle_boundary_overshoot(EncoderState *state, int32_t current_angle) {
  int32_t lo = (state->cal_min < state->cal_max) ? state->cal_min : state->cal_max;
  int32_t hi = (state->cal_min < state->cal_max) ? state->cal_max : state->cal_min;

  if (current_angle > hi) {
    int32_t overshoot = current_angle - hi;
    state->cal_max += overshoot;
    state->cal_min += overshoot;
    state->overshoot_detected = true;
    return;
  }

  if (current_angle < lo) {
    int32_t overshoot = lo - current_angle;
    state->cal_min -= overshoot;
    state->cal_max -= overshoot;
    state->overshoot_detected = true;
    return;
  }
}

void math_process_angle(EncoderState *state, uint16_t raw_angle) {
  raw_angle = apply_deadband(raw_angle, state->raw_angle, ENCODER_DEADBAND);

  // this is the actual unwrap step delta rides along the real turns the
  // gear can do so a multi turn gear ratio never loses track of position
  int16_t delta = angle_distance(state->raw_angle, raw_angle);
  state->unwrapped_angle += delta;
  state->raw_angle = normalize_angle_14bit(raw_angle);

  switch (state->filter_type) {
    case FILTER_EMA:
      state->filtered_angle = ema_filter(state, state->unwrapped_angle);
      break;
    case FILTER_MEDIAN:
      state->filtered_angle = median_filter(state, state->unwrapped_angle);
      break;
    case FILTER_KALMAN:
      state->filtered_angle = kalman_filter(state, state->unwrapped_angle);
      break;
    default:
      state->filtered_angle = state->unwrapped_angle;
      break;
  }

  uint32_t now = micros();
  if (state->update_timestamp > 0) {
    uint32_t dt_us = now - state->update_timestamp;
    if (dt_us > 0) {
      state->velocity = (int16_t)(((int32_t)delta * 1000) / (int32_t)dt_us);
      uint16_t abs_velocity = (state->velocity < 0) ? -state->velocity : state->velocity;
      if (abs_velocity > state->max_velocity) state->max_velocity = abs_velocity;
    }
  }
  state->update_timestamp = now;

  handle_boundary_overshoot(state, state->filtered_angle);
  state->normalized_angle = state->filtered_angle;
  state->sample_count++;
}

float process_encoder_sample(EncoderState *state, uint16_t raw_angle) {
  state->last_mm_value = state->mm_value;

  math_process_angle(state, raw_angle);

  if (state->cal_range > 0) {
    // direction says which of cal_min/cal_max is the physical low end not
    // which one is numerically smaller same reasoning as the overshoot fix
    float angle_offset = (state->direction == 0)
        ? (float)(state->normalized_angle - state->cal_min)
        : (float)(state->cal_min - state->normalized_angle);
    state->mm_value = state->mm_min + angle_offset * state->mm_scale;

    if (state->mm_value < state->mm_min) state->mm_value = state->mm_min;
    else if (state->mm_value > state->mm_max) state->mm_value = state->mm_max;
  }

  return state->mm_value;
}

uint8_t auto_detect_direction(EncoderState *state, const uint16_t *samples, size_t count) {
  if (count < 10) return 0;

  int32_t sum_derivatives = 0;
  int32_t positive_count = 0;
  int32_t negative_count = 0;

  for (size_t i = 1; i < count; i++) {
    int16_t derivative = angle_distance(samples[i - 1], samples[i]);
    sum_derivatives += derivative;
    if (derivative > 10) positive_count++;
    else if (derivative < -10) negative_count++;
  }

  if (positive_count > negative_count) {
    state->direction = 0;
  } else if (negative_count > positive_count) {
    state->direction = 1;
  } else {
    state->direction = (sum_derivatives >= 0) ? 0 : 1;
  }

  return state->direction;
}

uint16_t calculate_noise_floor(const uint16_t *samples, size_t count) {
  if (count < 2) return 0;

  uint32_t sum = 0;
  for (size_t i = 0; i < count; i++) sum += samples[i];
  float mean = (float)sum / count;

  float sum_sq_diff = 0;
  for (size_t i = 0; i < count; i++) {
    float diff = samples[i] - mean;
    sum_sq_diff += diff * diff;
  }

  return (uint16_t)sqrtf(sum_sq_diff / count);
}

float calculate_scale_factor(int32_t angle_range, float mm_range) {
  if (angle_range == 0) return 0.0f;
  return mm_range / angle_range;
}

void math_engine_init_state(EncoderState *state, uint8_t encoder_id) {
  memset(state, 0, sizeof(EncoderState));

  state->encoder_id = encoder_id;
  state->filter_type = FILTER_EMA;

  state->cal_min = 0;
  state->cal_max = AS5048_MAX_ANGLE;
  state->cal_range = AS5048_MAX_ANGLE;

  state->mm_min = 0.0f;
  state->mm_max = 100.0f;
  state->mm_scale = calculate_scale_factor(state->cal_range, state->mm_max - state->mm_min);

  state->kalman_q = KALMAN_Q;
  state->kalman_r = KALMAN_R;
}

void math_engine_reset_state(EncoderState *state) {
  uint8_t encoder_id = state->encoder_id;
  uint8_t filter_type = state->filter_type;
  int32_t cal_min = state->cal_min;
  int32_t cal_max = state->cal_max;
  float mm_min = state->mm_min;
  float mm_max = state->mm_max;
  uint8_t direction = state->direction;

  math_engine_init_state(state, encoder_id);

  state->filter_type = filter_type;
  math_engine_apply_calibration(state, cal_min, cal_max, mm_min, mm_max, direction);
}

void math_engine_set_filter(EncoderState *state, uint8_t filter_type) {
  state->filter_type = filter_type;
  state->ema_initialized = false;
  state->kalman_initialized = false;
  state->median_index = 0;
  memset(state->median_buffer, 0, sizeof(state->median_buffer));
}

void math_engine_configure_kalman(EncoderState *state, float process_noise, float measurement_noise) {
  state->kalman_q = process_noise;
  state->kalman_r = measurement_noise;
  state->kalman_initialized = false;
}

bool math_engine_validate_calibration(const EncoderState *state) {
  // cal_max < cal_min is the normal case for the reverse direction not an
  // error only a zero range (nothing moved between min and max) is bad
  if (state->cal_max == state->cal_min) return false;
  if (state->mm_max <= state->mm_min) return false;
  return true;
}

void math_engine_apply_calibration(EncoderState *state, int32_t cal_min, int32_t cal_max,
                                    float mm_min, float mm_max, uint8_t direction) {
  state->cal_min = cal_min;
  state->cal_max = cal_max;
  state->cal_range = (cal_max > cal_min) ? (cal_max - cal_min) : (cal_min - cal_max);
  state->mm_min = mm_min;
  state->mm_max = mm_max;
  state->direction = direction;
  state->mm_scale = calculate_scale_factor(state->cal_range, mm_max - mm_min);
  state->calibrated = 1;
}

float math_engine_get_position_mm(const EncoderState *state) {
  return state->mm_value;
}

float math_engine_get_velocity_mm_per_sec(const EncoderState *state) {
  return state->velocity * state->mm_scale * 1000.0f;
}
