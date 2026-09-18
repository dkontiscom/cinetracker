#include "calibration.h"
#include "storage.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

static const char *STATE_NAMES[] = {
    "Idle", "Detect Encoders",
    "Focus Minimum", "Focus Midpoint", "Focus Maximum",
    "Iris Minimum", "Iris Midpoint", "Iris Maximum",
    "Zoom Minimum", "Zoom Midpoint", "Zoom Maximum",
    "Validation", "Complete", "Error",
};

static const char *STATE_INSTRUCTIONS[] = {
    "Ready to start calibration",
    "Detecting connected encoders...",
    "Rotate focus to MINIMUM (closest focus) and press SET",
    "Rotate focus to approximate MIDDLE position and press SET",
    "Rotate focus to MAXIMUM (infinity) and press SET",
    "Open iris to MINIMUM f-stop (wide open) and press SET",
    "Set iris to approximate MIDDLE position and press SET",
    "Close iris to MAXIMUM f-stop and press SET",
    "Set zoom to MINIMUM (widest angle) and press SET",
    "Set zoom to approximate MIDDLE position and press SET",
    "Set zoom to MAXIMUM (telephoto) and press SET",
    "Testing calibration accuracy...",
    "Calibration complete! Press SAVE to store.",
    "Calibration failed. Check error message.",
};

// encoder_id order used whenever we walk "whats next", 0=focus 1=iris
// 2=zoom, matches the bit order of encoder_wanted_mask too
static const uint8_t ENCODER_ORDER[3] = {0, 1, 2};
static const CalibrationState ENCODER_MIN_STATE[3] = {
    CAL_STATE_FOCUS_SET_MIN, CAL_STATE_IRIS_SET_MIN, CAL_STATE_ZOOM_SET_MIN,
};

#define SAMPLE_BUFFER_SIZE 1024
static uint16_t g_sample_buffer[SAMPLE_BUFFER_SIZE];

// min/mid/max are continuous unwrapped positions now not raw wrapped 14bit
// values no modulo math needed the direction just says which came out
// numerically bigger a plain magnitude is the real total range either way
static int32_t calculate_angle_range(int32_t min_angle, int32_t max_angle) {
  return (max_angle > min_angle) ? (max_angle - min_angle) : (min_angle - max_angle);
}

int calibration_start_session(CalibrationSession *session, CalibrationData *result, uint8_t encoder_wanted_mask) {
  if (!session || !result || (encoder_wanted_mask & ~0x07) || encoder_wanted_mask == 0) {
    return CAL_ERROR_INVALID_PARAM;
  }

  Serial.println("Calibration: Starting new session");

  // result (ctx->cal_data) is the live/saved calibration the status page
  // reads from - it used to get memset here immediately on Start, wiping
  // the display until reboot if the operator aborted instead of saving.
  // only the session's own temp_encoders need clearing, result is left
  // untouched until calibration_end_session actually saves into it
  memset(session, 0, sizeof(CalibrationSession));

  session->state = CAL_STATE_DETECT_ENCODERS;
  session->previous_state = CAL_STATE_IDLE;
  session->result = result;
  session->sample_buffer = g_sample_buffer;
  session->sample_capacity = SAMPLE_BUFFER_SIZE;
  session->encoder_wanted_mask = encoder_wanted_mask;

  session->total_steps = 0;
  for (int i = 0; i < 3; i++) {
    if (encoder_wanted_mask & (1 << ENCODER_ORDER[i])) session->total_steps += 3;
  }

  return CAL_OK;
}

int calibration_end_session(CalibrationSession *session, bool save) {
  if (!session || !session->result) return CAL_ERROR_INVALID_PARAM;

  Serial.printf("Calibration: Ending session (save=%d)\n", save);

  if (save && session->state == CAL_STATE_COMPLETE) {
    memcpy(session->result->encoders, session->temp_encoders, sizeof(session->temp_encoders));

    session->result->valid = 1;
    session->result->version = 1;
    session->result->timestamp = millis() / 1000;

    session->result->total_score = 0;
    for (int i = 0; i < 3; i++) {
      if (session->temp_encoders[i].calibrated) {
        session->result->total_score += session->temp_encoders[i].quality_score;
      }
    }

    if (storage_save_calibration(session->result) != 0) {
      Serial.println("Calibration: Failed to save to storage");
      return CAL_ERROR_STORAGE_FAIL;
    }

    Serial.println("Calibration: Saved successfully");
  }

  session->state = CAL_STATE_IDLE;
  return CAL_OK;
}

void calibration_abort_session(CalibrationSession *session) {
  if (!session) return;
  Serial.println("Calibration: Session aborted");
  session->state = CAL_STATE_IDLE;
  session->error_code = -1;
  strcpy(session->error_message, "Calibration aborted by user");
}

int calibration_detect_encoders(CalibrationSession *session, uint8_t *detected_mask) {
  if (!session || !detected_mask) return CAL_ERROR_INVALID_PARAM;

  Serial.println("Calibration: Detecting encoders...");

  // real hardware presence check, but only a fault warning not a gate -
  // the operator's wanted mask decides what actually gets calibrated, spi
  // absence detection (focus/zoom) isnt reliable enough to gate on alone
  *detected_mask = 0;
  uint16_t test_angle;
  for (int i = 0; i < 3; i++) {
    if (encoder_read_angle(i, &test_angle)) *detected_mask |= (1 << i);
  }

  for (int i = 0; i < 3; i++) {
    uint8_t id = ENCODER_ORDER[i];
    bool wanted = session->encoder_wanted_mask & (1 << id);
    session->temp_encoders[id].enabled = wanted ? 1 : 0;
    session->temp_encoders[id].encoder_id = id;
    if (wanted && !(*detected_mask & (1 << id))) {
      Serial.printf("  Encoder %d wanted but not responding on the bus\n", id);
    }
  }

  for (int i = 0; i < 3; i++) {
    uint8_t id = ENCODER_ORDER[i];
    if (session->encoder_wanted_mask & (1 << id)) {
      session->state = ENCODER_MIN_STATE[id];
      session->current_encoder = id;
      return CAL_OK;
    }
  }

  session->state = CAL_STATE_ERROR;
  strcpy(session->error_message, "No encoders selected");
  return CAL_ERROR_ENCODER_FAULT;
}

// after the last point of an encoder's range is set, either move to the
// next wanted encoder's minimum or, if that was the last one, run
// validation right now instead of leaving the session stuck in VALIDATE
// forever with nothing to ever move it to COMPLETE
static void advance_after_max(CalibrationSession *session, uint8_t just_finished_id) {
  for (int i = 0; i < 3; i++) {
    uint8_t id = ENCODER_ORDER[i];
    if (id <= just_finished_id) continue;
    if (session->encoder_wanted_mask & (1 << id)) {
      session->state = ENCODER_MIN_STATE[id];
      session->current_encoder = id;
      return;
    }
  }

  session->state = CAL_STATE_VALIDATE;
  char report[256];
  int errors = calibration_validate(session->temp_encoders, report, sizeof(report));
  if (errors < 0) {
    session->state = CAL_STATE_ERROR;
    strncpy(session->error_message, report, sizeof(session->error_message) - 1);
    session->error_message[sizeof(session->error_message) - 1] = '\0';
    session->error_code = errors;
  } else {
    session->state = CAL_STATE_COMPLETE;
  }
}

int calibration_set_point(CalibrationSession *session, uint8_t encoder_id, int32_t angle,
                           bool has_phys_value, float phys_value) {
  if (!session || encoder_id >= 3) return CAL_ERROR_INVALID_PARAM;

  EncoderCalibration *enc = &session->temp_encoders[encoder_id];
  int32_t delta = 0;
  CalibrationState min_state, mid_state, max_state;

  switch (encoder_id) {
    case 0: min_state = CAL_STATE_FOCUS_SET_MIN; mid_state = CAL_STATE_FOCUS_SET_MID; max_state = CAL_STATE_FOCUS_SET_MAX; break;
    case 1: min_state = CAL_STATE_IRIS_SET_MIN; mid_state = CAL_STATE_IRIS_SET_MID; max_state = CAL_STATE_IRIS_SET_MAX; break;
    case 2: min_state = CAL_STATE_ZOOM_SET_MIN; mid_state = CAL_STATE_ZOOM_SET_MID; max_state = CAL_STATE_ZOOM_SET_MAX; break;
    default: return CAL_ERROR_INVALID_PARAM;
  }

  if (session->state == min_state) {
    enc->min_angle = angle;
    enc->has_phys_min = has_phys_value;
    enc->phys_min = phys_value;
    Serial.printf("Calibration: encoder %d MIN = %ld\n", encoder_id, (long)angle);
    session->state = mid_state;
    session->steps_completed++;

  } else if (session->state == mid_state) {
    enc->mid_angle = angle;
    enc->has_phys_mid = has_phys_value;
    enc->phys_mid = phys_value;
    // continuous unwrapped position now plain subtraction not angle_distance
    delta = angle - enc->min_angle;
    enc->direction = (delta > 0) ? 0 : 1;
    Serial.printf("Calibration: encoder %d MID = %ld (direction=%s)\n", encoder_id, (long)angle,
                  enc->direction ? "CCW" : "CW");
    session->state = max_state;
    session->steps_completed++;

  } else if (session->state == max_state) {
    enc->max_angle = angle;
    // focus max is always infinity by construction, never a typed value
    enc->has_phys_max = (encoder_id == 0) ? false : has_phys_value;
    enc->phys_max = (encoder_id == 0) ? 0.0f : phys_value;

    enc->total_range = calculate_angle_range(enc->min_angle, enc->max_angle);

    int32_t seg_lo_range = (enc->direction == 0) ? (enc->mid_angle - enc->min_angle) : (enc->min_angle - enc->mid_angle);
    enc->scale_lo = (enc->has_phys_min && enc->has_phys_mid && seg_lo_range != 0)
        ? (enc->phys_mid - enc->phys_min) / seg_lo_range : 0.0f;

    int32_t seg_hi_range = (enc->direction == 0) ? (enc->max_angle - enc->mid_angle) : (enc->mid_angle - enc->max_angle);
    enc->scale_hi = (enc->has_phys_mid && enc->has_phys_max && seg_hi_range != 0)
        ? (enc->phys_max - enc->phys_mid) / seg_hi_range : 0.0f;

    enc->calibrated = 1;
    enc->quality_score = 85;
    Serial.printf("Calibration: encoder %d MAX = %ld (range=%ld)\n", encoder_id, (long)angle, (long)enc->total_range);

    session->steps_completed++;
    advance_after_max(session, encoder_id);

  } else {
    return CAL_ERROR_INVALID_PARAM;
  }

  session->progress_percent = (session->total_steps > 0) ? (session->steps_completed * 100) / session->total_steps : 0;
  return CAL_OK;
}

int calibration_collect_samples(CalibrationSession *session, uint8_t encoder_id,
                                 const uint16_t *samples, size_t count) {
  if (!session || !samples || encoder_id >= 3) return CAL_ERROR_INVALID_PARAM;

  EncoderCalibration *enc = &session->temp_encoders[encoder_id];

  size_t copy_count = (count < session->sample_capacity) ? count : session->sample_capacity;
  memcpy(session->sample_buffer, samples, copy_count * sizeof(uint16_t));
  session->sample_count = copy_count;

  enc->noise_floor = calculate_noise_floor(samples, count);
  enc->sample_count = count;

  Serial.printf("Calibration: Collected %u samples, noise=%u\n", (unsigned)count, enc->noise_floor);

  return CAL_OK;
}

int calibration_validate(const EncoderCalibration encoders[3], char *error_report, size_t report_size) {
  int errors = 0;
  char temp[256];

  if (error_report && report_size > 0) error_report[0] = '\0';

  Serial.println("Calibration: Validating calibration data...");

  for (int i = 0; i < 3; i++) {
    const EncoderCalibration *enc = &encoders[i];
    if (!enc->enabled) continue;

    if (!enc->calibrated) {
      snprintf(temp, sizeof(temp), "Encoder %d enabled but not calibrated\n", i);
      if (error_report) strncat(error_report, temp, report_size - strlen(error_report) - 1);
      errors++;
      continue;
    }

    if (enc->total_range < 100) {
      snprintf(temp, sizeof(temp), "Encoder %d range too small (%ld)\n", i, (long)enc->total_range);
      if (error_report) strncat(error_report, temp, report_size - strlen(error_report) - 1);
      errors++;
    }

    if (enc->noise_floor > 50) {
      snprintf(temp, sizeof(temp), "Encoder %d has high noise (%u)\n", i, enc->noise_floor);
      if (error_report) strncat(error_report, temp, report_size - strlen(error_report) - 1);
    }

    Serial.printf("  Encoder %d: Range=%ld, Noise=%u, Quality=%u%%\n",
                  i, (long)enc->total_range, enc->noise_floor, enc->quality_score);
  }

  Serial.println(errors > 0 ? "Calibration: Validation FAILED" : "Calibration: Validation PASSED");
  return errors > 0 ? -errors : 0;
}

int calibration_test_accuracy(const CalibrationData *cal, uint8_t encoder_id, uint8_t *score) {
  if (!cal || encoder_id >= 3 || !score) return CAL_ERROR_INVALID_PARAM;

  const EncoderCalibration *enc = &cal->encoders[encoder_id];
  if (!enc->calibrated) {
    *score = 0;
    return CAL_ERROR_INVALID_PARAM;
  }

  int32_t total_score = 100;

  if (enc->noise_floor > 10) total_score -= min(20, enc->noise_floor / 5);
  if (enc->total_range < 1000) total_score -= 10;

  *score = (uint8_t)total_score;
  Serial.printf("Calibration: Encoder %d accuracy score: %u%%\n", encoder_id, *score);
  return CAL_OK;
}

bool calibration_angle_to_phys(const CalibrationData *cal, uint8_t encoder_id, int32_t angle,
                                float *out_value, bool *out_is_infinite) {
  if (!cal || encoder_id >= 3 || !out_value || !out_is_infinite) return false;

  const EncoderCalibration *enc = &cal->encoders[encoder_id];
  if (!enc->calibrated) return false;

  bool past_mid = (enc->direction == 0) ? (angle >= enc->mid_angle) : (angle <= enc->mid_angle);

  if (encoder_id == 0 && past_mid) {
    *out_is_infinite = true;
    *out_value = 0.0f;
    return true;
  }
  *out_is_infinite = false;

  if (!past_mid) {
    if (!enc->has_phys_min || !enc->has_phys_mid) return false;
    float offset = (enc->direction == 0) ? (float)(angle - enc->min_angle) : (float)(enc->min_angle - angle);
    *out_value = enc->phys_min + offset * enc->scale_lo;
  } else {
    if (!enc->has_phys_mid || !enc->has_phys_max) return false;
    float offset = (enc->direction == 0) ? (float)(angle - enc->mid_angle) : (float)(enc->mid_angle - angle);
    *out_value = enc->phys_mid + offset * enc->scale_hi;
  }

  return true;
}

const char *calibration_get_state_name(CalibrationState state) {
  if ((size_t)state < sizeof(STATE_NAMES) / sizeof(STATE_NAMES[0])) return STATE_NAMES[state];
  return "Unknown";
}

const char *calibration_get_instructions(CalibrationState state) {
  if ((size_t)state < sizeof(STATE_INSTRUCTIONS) / sizeof(STATE_INSTRUCTIONS[0])) return STATE_INSTRUCTIONS[state];
  return "Unknown state";
}

uint8_t calibration_get_progress(const CalibrationSession *session) {
  if (!session) return 0;
  return session->progress_percent;
}

void apply_calibration(EncoderState *encoder, const EncoderCalibration *cal) {
  if (!encoder || !cal || !cal->calibrated) return;

  Serial.printf("Calibration: Applying calibration to encoder %d\n", cal->encoder_id);

  encoder->cal_min = cal->min_angle;
  encoder->cal_max = cal->max_angle;
  encoder->cal_range = cal->total_range;
  encoder->direction = cal->direction;

  encoder->original_cal_min = encoder->cal_min;
  encoder->original_cal_max = encoder->cal_max;

  encoder->encoder_id = cal->encoder_id;
  encoder->enabled = 1;
  encoder->calibrated = 1;
  encoder->noise_floor = cal->noise_floor;

  Serial.printf("  Range: %ld-%ld (%ld counts)\n", (long)encoder->cal_min, (long)encoder->cal_max,
                (long)encoder->cal_range);
  Serial.printf("  Direction: %s\n", encoder->direction ? "CCW" : "CW");
}
