#pragma once

#include <Arduino.h>
#include "math_engine.h"

enum CalibrationState {
  CAL_STATE_IDLE = 0,
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

struct EncoderCalibration {
  // unwrapped multi turn positions not raw 14bit values see math_engine h
  int32_t min_angle;
  int32_t mid_angle;
  int32_t max_angle;

  uint8_t direction;
  uint8_t enabled;
  uint8_t calibrated;
  uint8_t encoder_id;

  int32_t total_range;

  // operator typed reference values, mm for focus/zoom, fstop for iris
  // status page sanity check display only, never sent over freed. focus
  // has no phys_max, its physical max is always infinity by construction
  bool has_phys_min;
  bool has_phys_mid;
  bool has_phys_max;
  float phys_min;
  float phys_mid;
  float phys_max;

  // two segment scale, min-mid and mid-max, so a lens that isnt linear
  // across its whole throw (focus rings especially) still gets a rough
  // per segment estimate instead of one number averaged across the range
  float scale_lo;
  float scale_hi;

  uint16_t noise_floor;
  uint8_t quality_score;

  uint32_t sample_count;
};

struct CalibrationData {
  EncoderCalibration encoders[3];

  uint32_t timestamp;
  uint32_t version;
  uint8_t valid;
  uint16_t total_score;
};

struct CalibrationSession {
  CalibrationState state;
  CalibrationState previous_state;

  uint8_t current_encoder;
  uint8_t steps_completed;
  uint8_t total_steps;
  uint8_t progress_percent;

  // operator's manual choice at calibration_start_session, not hardware
  // auto detected, since a prime lens has no zoom ring to detect at all
  uint8_t encoder_wanted_mask;

  EncoderCalibration temp_encoders[3];

  uint16_t *sample_buffer;
  size_t sample_capacity;
  size_t sample_count;

  CalibrationData *result;

  char error_message[256];
  int error_code;
};

int calibration_start_session(CalibrationSession *session, CalibrationData *result, uint8_t encoder_wanted_mask);
int calibration_end_session(CalibrationSession *session, bool save);
void calibration_abort_session(CalibrationSession *session);

int calibration_detect_encoders(CalibrationSession *session, uint8_t *detected_mask);
int calibration_set_point(CalibrationSession *session, uint8_t encoder_id, int32_t angle,
                           bool has_phys_value, float phys_value);
int calibration_collect_samples(CalibrationSession *session, uint8_t encoder_id,
                                 const uint16_t *samples, size_t count);

int calibration_validate(const EncoderCalibration encoders[3], char *error_report, size_t report_size);
int calibration_test_accuracy(const CalibrationData *cal, uint8_t encoder_id, uint8_t *score);

// rough physical estimate for the status page sanity check display only
// never sent over freed, returns false if this encoder has no usable
// reference points for the angle's segment. focus reports out_is_infinite
// past its mid point instead of a value, its physical max is always
// infinity so no finite estimate is possible or meaningful there
bool calibration_angle_to_phys(const CalibrationData *cal, uint8_t encoder_id, int32_t angle,
                                float *out_value, bool *out_is_infinite);

const char *calibration_get_state_name(CalibrationState state);
const char *calibration_get_instructions(CalibrationState state);

void apply_calibration(EncoderState *encoder, const EncoderCalibration *cal);
// pushes a whole CalibrationData into the live encoder states, the one
// place both boot and a web ui save go through so they cant drift apart
void calibration_apply_all(EncoderState encoders[3], const CalibrationData *cal);

bool encoder_read_angle(uint8_t encoder_id, uint16_t *angle);
