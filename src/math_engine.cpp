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

void math_process_angle(EncoderState *state, uint16_t raw_angle, int32_t unwrapped_angle) {
  // the unwrap itself happens in encoder_driver at the sample rate, see
  // accumulate() there for why it cant live here at loop rate. delta is
  // only for velocity now
  int32_t delta = unwrapped_angle - state->unwrapped_angle;
  state->unwrapped_angle = unwrapped_angle;
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

void process_encoder_sample(EncoderState *state, uint16_t raw_angle, int32_t unwrapped_angle) {
  math_process_angle(state, raw_angle, unwrapped_angle);
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

void math_engine_init_state(EncoderState *state, uint8_t encoder_id) {
  memset(state, 0, sizeof(EncoderState));

  state->encoder_id = encoder_id;
  state->filter_type = FILTER_EMA;

  state->cal_min = 0;
  state->cal_max = AS5048_MAX_ANGLE;
  state->cal_range = AS5048_MAX_ANGLE;

  state->kalman_q = KALMAN_Q;
  state->kalman_r = KALMAN_R;
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

