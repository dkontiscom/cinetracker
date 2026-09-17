#include "calibration.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

// numbers off datasheets/spec sheets some are guesses
static const LensPreset LENS_PRESETS[] = {
    {"Zeiss CP.3 25mm T2.1", 250, 65535, 21, 220, 25, 25},
    {"Canon CN-E 50mm T1.3", 450, 65535, 13, 220, 50, 50},
    {"Cooke S4/i 18mm T2.0", 350, 65535, 20, 220, 18, 18},
    {"Arri Signature Prime 35mm T1.8", 320, 65535, 18, 220, 35, 35},
    {"Canon CN-E 17-120mm T2.95", 950, 65535, 29, 220, 17, 120},
    {"Fujinon MK 18-55mm T2.9", 850, 65535, 29, 220, 18, 55},
    {"Zeiss LWZ.3 21-100mm T2.9-3.9", 800, 65535, 29, 220, 21, 100},
    {"Angenieux Optimo 24-290mm T2.8", 1200, 65535, 28, 220, 24, 290},
    {"Cooke Anamorphic/i 50mm T2.3", 750, 65535, 23, 220, 50, 50},
    {"Arri Master Anamorphic 75mm T1.9", 900, 65535, 19, 220, 75, 75},
    {"Generic Manual Lens", 300, 65535, 14, 160, 50, 50},
    {"Custom Lens Profile", 500, 65535, 20, 220, 35, 35},
};

static const int NUM_LENS_PRESETS = sizeof(LENS_PRESETS) / sizeof(LensPreset);

static const char *STATE_NAMES[] = {
    "Idle", "Select Lens", "Detect Encoders",
    "Focus Minimum", "Focus Midpoint", "Focus Maximum",
    "Iris Minimum", "Iris Midpoint", "Iris Maximum",
    "Zoom Minimum", "Zoom Midpoint", "Zoom Maximum",
    "Validation", "Complete", "Error",
};

static const char *STATE_INSTRUCTIONS[] = {
    "Ready to start calibration",
    "Select lens profile or create new",
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

#define SAMPLE_BUFFER_SIZE 1024
static uint16_t g_sample_buffer[SAMPLE_BUFFER_SIZE];

static uint16_t calculate_angle_range(uint16_t min_angle, uint16_t max_angle, uint8_t direction) {
  if (direction == 0) {
    if (max_angle >= min_angle) return max_angle - min_angle;
    return (AS5048_MAX_ANGLE - min_angle) + max_angle;
  } else {
    if (min_angle >= max_angle) return min_angle - max_angle;
    return (AS5048_MAX_ANGLE - max_angle) + min_angle;
  }
}

static float calc_scale_factor_range(uint16_t angle_range, float min_physical, float max_physical) {
  if (angle_range == 0) return 0.0f;
  return (max_physical - min_physical) / angle_range;
}

const LensPreset *calibration_get_lens_presets() {
  return LENS_PRESETS;
}

int calibration_get_preset_count() {
  return NUM_LENS_PRESETS;
}

int calibration_start_session(CalibrationSession *session, CalibrationData *result) {
  if (!session || !result) return CAL_ERROR_INVALID_PARAM;

  Serial.println("Calibration: Starting new session");

  memset(session, 0, sizeof(CalibrationSession));
  memset(result, 0, sizeof(CalibrationData));

  session->state = CAL_STATE_SELECT_LENS;
  session->previous_state = CAL_STATE_IDLE;
  session->result = result;
  session->sample_buffer = g_sample_buffer;
  session->sample_capacity = SAMPLE_BUFFER_SIZE;
  session->start_time = millis();
  session->timeout_ms = 30000;

  result->version = 1;
  result->timestamp = millis() / 1000;

  return CAL_OK;
}

int calibration_end_session(CalibrationSession *session, bool save) {
  if (!session || !session->result) return CAL_ERROR_INVALID_PARAM;

  Serial.printf("Calibration: Ending session (save=%d)\n", save);

  if (save && session->state == CAL_STATE_COMPLETE) {
    memcpy(&session->result->lens, session->active_lens, sizeof(LensProfile));
    memcpy(session->result->encoders, session->temp_encoders, sizeof(session->temp_encoders));

    session->result->valid = 1;
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

int calibration_select_preset(CalibrationSession *session, int preset_index, LensProfile *profile) {
  if (!session || !profile || preset_index < 0 || preset_index >= NUM_LENS_PRESETS) {
    return CAL_ERROR_INVALID_PARAM;
  }

  const LensPreset *preset = &LENS_PRESETS[preset_index];

  memset(profile, 0, sizeof(LensProfile));
  strncpy(profile->name, preset->name, sizeof(profile->name) - 1);

  profile->focus_min_mm = preset->focus_min_mm;
  profile->focus_max_mm = preset->focus_max_mm;
  profile->focus_enabled = true;
  profile->focus_rotation_deg = 300;

  profile->iris_min_fstop = preset->iris_min_fstop_x10 / 10.0f;
  profile->iris_max_fstop = preset->iris_max_fstop_x10 / 10.0f;
  profile->iris_enabled = true;
  profile->iris_rotation_deg = 90;

  if (preset->zoom_min_mm != preset->zoom_max_mm) {
    profile->zoom_min_mm = preset->zoom_min_mm;
    profile->zoom_max_mm = preset->zoom_max_mm;
    profile->zoom_enabled = true;
    profile->zoom_rotation_deg = 120;
  } else {
    profile->zoom_enabled = false;
  }

  profile->created_timestamp = millis() / 1000;
  profile->modified_timestamp = profile->created_timestamp;

  Serial.printf("Calibration: Selected preset '%s'\n", preset->name);

  return calibration_set_lens_profile(session, profile);
}

int calibration_set_lens_profile(CalibrationSession *session, const LensProfile *profile) {
  if (!session || !profile) return CAL_ERROR_INVALID_PARAM;

  session->active_lens = (LensProfile *)profile;

  session->total_steps = 1;
  if (profile->focus_enabled) session->total_steps += 3;
  if (profile->iris_enabled) session->total_steps += 3;
  if (profile->zoom_enabled) session->total_steps += 3;
  session->total_steps += 1;

  Serial.printf("Calibration: Lens profile set, %d total steps\n", session->total_steps);

  session->state = CAL_STATE_DETECT_ENCODERS;
  return CAL_OK;
}

int calibration_detect_encoders(CalibrationSession *session, uint8_t *detected_mask) {
  if (!session || !detected_mask) return CAL_ERROR_INVALID_PARAM;

  Serial.println("Calibration: Detecting encoders...");

  *detected_mask = 0;
  int count = 0;
  uint16_t test_angle;

  if (session->active_lens->focus_enabled && encoder_read_angle(0, &test_angle)) {
    *detected_mask |= (1 << 0);
    session->temp_encoders[0].enabled = 1;
    session->temp_encoders[0].encoder_id = 0;
    count++;
    Serial.println("  Focus encoder detected");
  }

  if (session->active_lens->iris_enabled && encoder_read_angle(1, &test_angle)) {
    *detected_mask |= (1 << 1);
    session->temp_encoders[1].enabled = 1;
    session->temp_encoders[1].encoder_id = 1;
    count++;
    Serial.println("  Iris encoder detected");
  }

  if (session->active_lens->zoom_enabled && encoder_read_angle(2, &test_angle)) {
    *detected_mask |= (1 << 2);
    session->temp_encoders[2].enabled = 1;
    session->temp_encoders[2].encoder_id = 2;
    count++;
    Serial.println("  Zoom encoder detected");
  }

  if (count == 0) {
    session->state = CAL_STATE_ERROR;
    strcpy(session->error_message, "No encoders detected");
    return CAL_ERROR_ENCODER_FAULT;
  }

  if (session->temp_encoders[0].enabled) {
    session->state = CAL_STATE_FOCUS_SET_MIN;
    session->current_encoder = 0;
  } else if (session->temp_encoders[1].enabled) {
    session->state = CAL_STATE_IRIS_SET_MIN;
    session->current_encoder = 1;
  } else if (session->temp_encoders[2].enabled) {
    session->state = CAL_STATE_ZOOM_SET_MIN;
    session->current_encoder = 2;
  }

  session->step_start_time = millis();
  return count;
}

int calibration_set_point(CalibrationSession *session, uint8_t encoder_id, uint16_t angle) {
  if (!session || encoder_id >= 3) return CAL_ERROR_INVALID_PARAM;

  EncoderCalibration *enc = &session->temp_encoders[encoder_id];
  int16_t delta = 0;

  switch (session->state) {
    case CAL_STATE_FOCUS_SET_MIN:
      if (encoder_id != 0) return CAL_ERROR_INVALID_PARAM;
      enc->min_angle = angle;
      Serial.printf("Calibration: Focus MIN = %u\n", angle);
      session->state = CAL_STATE_FOCUS_SET_MID;
      session->steps_completed++;
      break;

    case CAL_STATE_FOCUS_SET_MID:
      if (encoder_id != 0) return CAL_ERROR_INVALID_PARAM;
      enc->mid_angle = angle;
      delta = angle_distance(enc->min_angle, angle);
      enc->direction = (delta > 0) ? 0 : 1;
      Serial.printf("Calibration: Focus MID = %u (direction=%s)\n", angle, enc->direction ? "CCW" : "CW");
      session->state = CAL_STATE_FOCUS_SET_MAX;
      session->steps_completed++;
      break;

    case CAL_STATE_FOCUS_SET_MAX:
      if (encoder_id != 0) return CAL_ERROR_INVALID_PARAM;
      enc->max_angle = angle;
      enc->total_range = calculate_angle_range(enc->min_angle, enc->max_angle, enc->direction);
      enc->scale_factor = calc_scale_factor_range(enc->total_range,
                                                   session->active_lens->focus_min_mm,
                                                   session->active_lens->focus_max_mm);
      enc->calibrated = 1;
      enc->quality_score = 85;
      Serial.printf("Calibration: Focus MAX = %u (range=%u)\n", angle, enc->total_range);

      if (session->temp_encoders[1].enabled) {
        session->state = CAL_STATE_IRIS_SET_MIN;
        session->current_encoder = 1;
      } else if (session->temp_encoders[2].enabled) {
        session->state = CAL_STATE_ZOOM_SET_MIN;
        session->current_encoder = 2;
      } else {
        session->state = CAL_STATE_VALIDATE;
      }
      session->steps_completed++;
      break;

    case CAL_STATE_IRIS_SET_MIN:
      if (encoder_id != 1) return CAL_ERROR_INVALID_PARAM;
      enc->min_angle = angle;
      Serial.printf("Calibration: Iris MIN = %u\n", angle);
      session->state = CAL_STATE_IRIS_SET_MID;
      session->steps_completed++;
      break;

    case CAL_STATE_IRIS_SET_MID:
      if (encoder_id != 1) return CAL_ERROR_INVALID_PARAM;
      enc->mid_angle = angle;
      delta = angle_distance(enc->min_angle, angle);
      enc->direction = (delta > 0) ? 0 : 1;
      Serial.printf("Calibration: Iris MID = %u (direction=%s)\n", angle, enc->direction ? "CCW" : "CW");
      session->state = CAL_STATE_IRIS_SET_MAX;
      session->steps_completed++;
      break;

    case CAL_STATE_IRIS_SET_MAX:
      if (encoder_id != 1) return CAL_ERROR_INVALID_PARAM;
      enc->max_angle = angle;
      enc->total_range = calculate_angle_range(enc->min_angle, enc->max_angle, enc->direction);
      enc->scale_factor = calc_scale_factor_range(enc->total_range,
                                                   session->active_lens->iris_min_fstop,
                                                   session->active_lens->iris_max_fstop);
      enc->calibrated = 1;
      enc->quality_score = 85;
      Serial.printf("Calibration: Iris MAX = %u (range=%u)\n", angle, enc->total_range);

      if (session->temp_encoders[2].enabled) {
        session->state = CAL_STATE_ZOOM_SET_MIN;
        session->current_encoder = 2;
      } else {
        session->state = CAL_STATE_VALIDATE;
      }
      session->steps_completed++;
      break;

    case CAL_STATE_ZOOM_SET_MIN:
      if (encoder_id != 2) return CAL_ERROR_INVALID_PARAM;
      enc->min_angle = angle;
      Serial.printf("Calibration: Zoom MIN = %u\n", angle);
      session->state = CAL_STATE_ZOOM_SET_MID;
      session->steps_completed++;
      break;

    case CAL_STATE_ZOOM_SET_MID:
      if (encoder_id != 2) return CAL_ERROR_INVALID_PARAM;
      enc->mid_angle = angle;
      delta = angle_distance(enc->min_angle, angle);
      enc->direction = (delta > 0) ? 0 : 1;
      Serial.printf("Calibration: Zoom MID = %u (direction=%s)\n", angle, enc->direction ? "CCW" : "CW");
      session->state = CAL_STATE_ZOOM_SET_MAX;
      session->steps_completed++;
      break;

    case CAL_STATE_ZOOM_SET_MAX:
      if (encoder_id != 2) return CAL_ERROR_INVALID_PARAM;
      enc->max_angle = angle;
      enc->total_range = calculate_angle_range(enc->min_angle, enc->max_angle, enc->direction);
      enc->scale_factor = calc_scale_factor_range(enc->total_range,
                                                   session->active_lens->zoom_min_mm,
                                                   session->active_lens->zoom_max_mm);
      enc->calibrated = 1;
      enc->quality_score = 85;
      Serial.printf("Calibration: Zoom MAX = %u (range=%u)\n", angle, enc->total_range);

      session->state = CAL_STATE_VALIDATE;
      session->steps_completed++;
      break;

    default:
      return CAL_ERROR_INVALID_PARAM;
  }

  session->progress_percent = (session->steps_completed * 100) / session->total_steps;
  session->step_start_time = millis();
  return CAL_OK;
}

int calibration_collect_samples(CalibrationSession *session, uint8_t encoder_id,
                                 const uint16_t *samples, size_t count) {
  if (!session || !samples || encoder_id >= 3) return CAL_ERROR_INVALID_PARAM;

  EncoderCalibration *enc = &session->temp_encoders[encoder_id];

  size_t copy_count = (count < session->sample_capacity) ? count : session->sample_capacity;
  memcpy(session->sample_buffer, samples, copy_count * sizeof(uint16_t));
  session->sample_count = copy_count;

  // used to calc repeatability here too was literaly the same number deleted it
  enc->noise_floor = calculate_noise_floor(samples, count);
  enc->sample_count = count;

  Serial.printf("Calibration: Collected %u samples, noise=%u\n", (unsigned)count, enc->noise_floor);

  return CAL_OK;
}

int calibration_validate(const CalibrationData *cal, char *error_report, size_t report_size) {
  int errors = 0;
  char temp[256];

  if (error_report && report_size > 0) error_report[0] = '\0';

  Serial.println("Calibration: Validating calibration data...");

  for (int i = 0; i < 3; i++) {
    const EncoderCalibration *enc = &cal->encoders[i];
    if (!enc->enabled) continue;

    if (!enc->calibrated) {
      snprintf(temp, sizeof(temp), "Encoder %d enabled but not calibrated\n", i);
      if (error_report) strncat(error_report, temp, report_size - strlen(error_report) - 1);
      errors++;
      continue;
    }

    if (enc->total_range < 100) {
      snprintf(temp, sizeof(temp), "Encoder %d range too small (%u)\n", i, enc->total_range);
      if (error_report) strncat(error_report, temp, report_size - strlen(error_report) - 1);
      errors++;
    }

    if (enc->noise_floor > 50) {
      snprintf(temp, sizeof(temp), "Encoder %d has high noise (%u)\n", i, enc->noise_floor);
      if (error_report) strncat(error_report, temp, report_size - strlen(error_report) - 1);
    }

    Serial.printf("  Encoder %d: Range=%u, Noise=%u, Quality=%u%%\n",
                  i, enc->total_range, enc->noise_floor, enc->quality_score);
  }

  if (cal->lens.name[0] == '\0') {
    snprintf(temp, sizeof(temp), "Lens profile has no name\n");
    if (error_report) strncat(error_report, temp, report_size - strlen(error_report) - 1);
    errors++;
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

float calibration_angle_to_mm(const CalibrationData *cal, uint8_t encoder_id, uint16_t angle) {
  if (!cal || encoder_id >= 3) return 0.0f;

  const EncoderCalibration *enc = &cal->encoders[encoder_id];
  if (!enc->calibrated) return 0.0f;

  int32_t normalized;
  if (enc->direction == 0) {
    normalized = angle - enc->min_angle;
    if (normalized < 0) normalized += AS5048_MAX_ANGLE;
  } else {
    normalized = enc->min_angle - angle;
    if (normalized < 0) normalized += AS5048_MAX_ANGLE;
  }

  if (normalized < 0) normalized = 0;
  if (normalized > enc->total_range) normalized = enc->total_range;

  float scaled = normalized * enc->scale_factor;

  float physical_min;
  switch (encoder_id) {
    case 0: physical_min = cal->lens.focus_min_mm; break;
    case 1: physical_min = cal->lens.iris_min_fstop; break;
    case 2: physical_min = cal->lens.zoom_min_mm; break;
    default: physical_min = 0;
  }

  return physical_min + scaled;
}

float calibration_angle_to_fstop(const CalibrationData *cal, uint16_t angle) {
  return calibration_angle_to_mm(cal, 1, angle);
}

uint16_t calibration_mm_to_angle(const CalibrationData *cal, uint8_t encoder_id, float mm) {
  if (!cal || encoder_id >= 3) return 0;

  const EncoderCalibration *enc = &cal->encoders[encoder_id];
  if (!enc->calibrated || enc->scale_factor == 0.0f) return 0;

  float physical_min, physical_max;
  switch (encoder_id) {
    case 0: physical_min = cal->lens.focus_min_mm; physical_max = cal->lens.focus_max_mm; break;
    case 1: physical_min = cal->lens.iris_min_fstop; physical_max = cal->lens.iris_max_fstop; break;
    case 2: physical_min = cal->lens.zoom_min_mm; physical_max = cal->lens.zoom_max_mm; break;
    default: return 0;
  }

  if (mm < physical_min) mm = physical_min;
  if (mm > physical_max) mm = physical_max;

  float angle_offset = (mm - physical_min) / enc->scale_factor;

  uint16_t result;
  if (enc->direction == 0) {
    result = ((uint16_t)(enc->min_angle + angle_offset)) & 0x3FFF;
  } else {
    result = ((uint16_t)(enc->min_angle - angle_offset)) & 0x3FFF;
  }

  return result;
}

int calibration_export_json(const CalibrationData *cal, char *buffer, size_t size) {
  if (!cal || !buffer || size < 256) return CAL_ERROR_INVALID_PARAM;

  int len = snprintf(buffer, size,
      "{\n"
      "  \"lens\": {\n"
      "    \"name\": \"%s\",\n"
      "    \"focus_min\": %.2f,\n"
      "    \"focus_max\": %.2f,\n"
      "    \"iris_min\": %.2f,\n"
      "    \"iris_max\": %.2f,\n"
      "    \"zoom_min\": %.2f,\n"
      "    \"zoom_max\": %.2f\n"
      "  },\n"
      "  \"encoders\": [\n",
      cal->lens.name,
      cal->lens.focus_min_mm, cal->lens.focus_max_mm,
      cal->lens.iris_min_fstop, cal->lens.iris_max_fstop,
      cal->lens.zoom_min_mm, cal->lens.zoom_max_mm);

  for (int i = 0; i < 3; i++) {
    const EncoderCalibration *enc = &cal->encoders[i];
    len += snprintf(buffer + len, size - len,
        "    {\n"
        "      \"id\": %d,\n"
        "      \"calibrated\": %s,\n"
        "      \"min_angle\": %u,\n"
        "      \"max_angle\": %u,\n"
        "      \"direction\": \"%s\",\n"
        "      \"range\": %u,\n"
        "      \"noise\": %u\n"
        "    }%s\n",
        i, enc->calibrated ? "true" : "false",
        enc->min_angle, enc->max_angle,
        enc->direction ? "CCW" : "CW",
        enc->total_range, enc->noise_floor,
        (i < 2) ? "," : "");
  }

  len += snprintf(buffer + len, size - len,
      "  ],\n"
      "  \"timestamp\": %lu,\n"
      "  \"version\": %lu\n"
      "}\n",
      (unsigned long)cal->timestamp, (unsigned long)cal->version);

  return len;
}

int calibration_import_json(CalibrationData *cal, const char *json) {
  (void)cal;
  (void)json;
  Serial.println("Calibration: JSON import not yet implemented");
  return CAL_ERROR_INVALID_PARAM;
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
  encoder->mm_scale = cal->scale_factor;

  encoder->original_cal_min = encoder->cal_min;
  encoder->original_cal_max = encoder->cal_max;

  encoder->encoder_id = cal->encoder_id;
  encoder->enabled = 1;
  encoder->calibrated = 1;
  encoder->noise_floor = cal->noise_floor;

  Serial.printf("  Range: %u-%u (%u counts)\n", encoder->cal_min, encoder->cal_max, encoder->cal_range);
  Serial.printf("  Direction: %s\n", encoder->direction ? "CCW" : "CW");
  Serial.printf("  Scale: %f\n", encoder->mm_scale);
}
