#pragma once

#include <Arduino.h>
#include "math_engine.h"

enum CalibrationState {
  CAL_STATE_IDLE = 0,
  CAL_STATE_SELECT_LENS,
  CAL_STATE_DETECT_ENCODERS,
  CAL_STATE_FOCUS_SET_MIN,
  CAL_STATE_FOCUS_SET_MID,
  CAL_STATE_FOCUS_SET_MAX,
  CAL_STATE_IRIS_SET_MIN,
  CAL_STATE_IRIS_SET_MID,
  CAL_STATE_IRIS_SET_MAX,
  CAL_STATE_ZOOM_SET_MIN,
  CAL_STATE_ZOOM_SET_MID,
  CAL_STATE_ZOOM_SET_MAX,
  CAL_STATE_VALIDATE,
  CAL_STATE_COMPLETE,
  CAL_STATE_ERROR,
};

enum CalibrationError {
  CAL_OK = 0,
  CAL_ERROR_INVALID_PARAM = -1,
  CAL_ERROR_ENCODER_FAULT = -2,
  CAL_ERROR_STORAGE_FAIL = -3,
};

struct LensProfile {
  char name[64];
  char manufacturer[32];
  char model[32];
  char serial_number[32];

  float focus_min_mm;
  float focus_max_mm;
  bool focus_enabled;

  float iris_min_fstop;
  float iris_max_fstop;
  bool iris_enabled;

  float zoom_min_mm;
  float zoom_max_mm;
  bool zoom_enabled;

  uint16_t focus_rotation_deg;
  uint16_t iris_rotation_deg;
  uint16_t zoom_rotation_deg;

  uint32_t created_timestamp;
  uint32_t modified_timestamp;
  uint32_t use_count;
};

struct EncoderCalibration {
  uint16_t min_angle;
  uint16_t mid_angle;
  uint16_t max_angle;

  uint8_t direction;
  uint8_t enabled;
  uint8_t calibrated;
  uint8_t encoder_id;

  uint16_t total_range;
  float scale_factor;

  uint16_t noise_floor;
  uint16_t repeatability;
  uint8_t quality_score;

  uint32_t sample_count;
  uint32_t calibration_time;
};

struct CalibrationData {
  LensProfile lens;
  EncoderCalibration encoders[3];

  uint32_t timestamp;
  uint32_t version;
  uint8_t valid;
  uint16_t total_score;

  char user_name[32];
  char notes[128];
};

struct CalibrationSession {
  CalibrationState state;
  CalibrationState previous_state;

  uint8_t current_encoder;
  uint8_t steps_completed;
  uint8_t total_steps;
  uint8_t progress_percent;

  LensProfile *active_lens;
  EncoderCalibration temp_encoders[3];

  uint16_t *sample_buffer;
  size_t sample_capacity;
  size_t sample_count;

  CalibrationData *result;

  char error_message[256];
  int error_code;

  uint32_t start_time;
  uint32_t step_start_time;
  uint32_t timeout_ms;
};

struct LensPreset {
  const char *name;
  uint16_t focus_min_mm;
  uint16_t focus_max_mm;
  uint8_t iris_min_fstop_x10;
  uint8_t iris_max_fstop_x10;
  uint16_t zoom_min_mm;
  uint16_t zoom_max_mm;
};

const LensPreset *calibration_get_lens_presets();
int calibration_get_preset_count();

int calibration_start_session(CalibrationSession *session, CalibrationData *result);
int calibration_end_session(CalibrationSession *session, bool save);
void calibration_abort_session(CalibrationSession *session);

int calibration_select_preset(CalibrationSession *session, int preset_index, LensProfile *profile);
int calibration_set_lens_profile(CalibrationSession *session, const LensProfile *profile);
int calibration_detect_encoders(CalibrationSession *session, uint8_t *detected_mask);
int calibration_set_point(CalibrationSession *session, uint8_t encoder_id, uint16_t angle);
int calibration_collect_samples(CalibrationSession *session, uint8_t encoder_id,
                                 const uint16_t *samples, size_t count);

int calibration_validate(const CalibrationData *cal, char *error_report, size_t report_size);
int calibration_test_accuracy(const CalibrationData *cal, uint8_t encoder_id, uint8_t *score);

float calibration_angle_to_mm(const CalibrationData *cal, uint8_t encoder_id, uint16_t angle);
float calibration_angle_to_fstop(const CalibrationData *cal, uint16_t angle);
uint16_t calibration_mm_to_angle(const CalibrationData *cal, uint8_t encoder_id, float mm);

int calibration_export_json(const CalibrationData *cal, char *buffer, size_t size);
int calibration_import_json(CalibrationData *cal, const char *json);

const char *calibration_get_state_name(CalibrationState state);
const char *calibration_get_instructions(CalibrationState state);
uint8_t calibration_get_progress(const CalibrationSession *session);

void apply_calibration(EncoderState *encoder, const EncoderCalibration *cal);

// Provided by the encoder driver module, ported separately.
bool encoder_read_angle(uint8_t encoder_id, uint16_t *angle);

// Provided by the storage module, ported separately.
int storage_save_calibration(const CalibrationData *cal);
