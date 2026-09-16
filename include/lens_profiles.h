#pragma once

#include <Arduino.h>
#include "calibration.h"

#define PROFILE_MAX_SLOTS 14
#define PROFILE_NAME_MAX_LEN 64
#define PROFILE_JSON_MAX_SIZE 4096

#define PROFILE_VALID_NAME 0x01
#define PROFILE_VALID_FOCUS 0x04
#define PROFILE_VALID_IRIS 0x08
#define PROFILE_VALID_ZOOM 0x10
#define PROFILE_VALID_ALL 0x1D

struct ProfileSummary {
  uint8_t slot;
  bool valid;
  bool in_use;
  char name[PROFILE_NAME_MAX_LEN];
  char manufacturer[32];
  char model[32];
  uint32_t created_time;
  uint32_t last_used_time;
  uint32_t use_count;
  uint8_t validation_flags;
  uint8_t quality_score;
};

struct ProfileFilter {
  const char *manufacturer;
  const char *name_contains;
  bool only_calibrated;
  uint32_t min_quality_score;
};

// On-disk record: a lens profile plus its per-encoder calibration and usage
// metadata. Provided (serialized to flash) by the storage module, ported
// separately.
struct StoredLensProfile {
  LensProfile profile;
  EncoderCalibration encoder_cal[3];
  uint32_t created_time;
  uint32_t last_used_time;
  uint32_t use_count;
};

int storage_load_lens_profile(int slot, StoredLensProfile *out);
int storage_save_lens_profile(const StoredLensProfile *profile, int slot);
int storage_delete_lens_profile(int slot);

int profiles_init();
int profiles_list(ProfileSummary *summaries, int max_count, const ProfileFilter *filter);
int profiles_get_summary(int slot, ProfileSummary *summary);

int profiles_create(const LensProfile *profile, int slot);
int profiles_load(int slot, LensProfile *profile);
int profiles_save(int slot, const LensProfile *profile);
int profiles_delete(int slot);

int profiles_quick_load(int slot);
int profiles_get_active();

int profiles_export_json(int slot, char *json_buffer, size_t buffer_size, bool pretty);
int profiles_import_json(const char *json_string, int slot);

int profiles_validate(int slot, uint8_t *validation_flags, uint8_t *quality_score);
int profiles_clone(int source_slot, int dest_slot, const char *new_name);
int profiles_find_by_name(const char *name);
int profiles_get_statistics(int *total_profiles, int *free_slots, int *active_slot);
int profiles_factory_reset(uint32_t confirm);

int profiles_format_name(const LensProfile *profile, char *buffer, size_t buffer_size);
int profiles_format_specs(const LensProfile *profile, char *buffer, size_t buffer_size);
