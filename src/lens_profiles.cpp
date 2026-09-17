#include "lens_profiles.h"

#include <string.h>
#include <strings.h>
#include <stdio.h>

static int g_active_profile_slot = -1;

static void build_summary(int slot, const StoredLensProfile &stored, ProfileSummary *summary) {
  summary->slot = slot;
  summary->valid = true;
  summary->in_use = (slot == g_active_profile_slot);

  strncpy(summary->name, stored.profile.name, PROFILE_NAME_MAX_LEN - 1);
  summary->name[PROFILE_NAME_MAX_LEN - 1] = '\0';
  strncpy(summary->manufacturer, stored.profile.manufacturer, 31);
  summary->manufacturer[31] = '\0';
  strncpy(summary->model, stored.profile.model, 31);
  summary->model[31] = '\0';

  summary->created_time = stored.created_time;
  summary->last_used_time = stored.last_used_time;
  summary->use_count = stored.use_count;

  summary->validation_flags = PROFILE_VALID_NAME;
  if (stored.profile.focus_enabled && stored.encoder_cal[0].calibrated) {
    summary->validation_flags |= PROFILE_VALID_FOCUS;
  }
  if (stored.profile.iris_enabled && stored.encoder_cal[1].calibrated) {
    summary->validation_flags |= PROFILE_VALID_IRIS;
  }
  if (stored.profile.zoom_enabled && stored.encoder_cal[2].calibrated) {
    summary->validation_flags |= PROFILE_VALID_ZOOM;
  }

  uint32_t total_quality = 0;
  int encoder_count = 0;
  for (int i = 0; i < 3; i++) {
    if (stored.encoder_cal[i].enabled) {
      total_quality += stored.encoder_cal[i].quality_score;
      encoder_count++;
    }
  }
  summary->quality_score = encoder_count > 0 ? (total_quality / encoder_count) : 0;
}

static bool passes_filter(const ProfileSummary &summary, const ProfileFilter *filter) {
  if (!filter) return true;
  if (filter->manufacturer && strstr(summary.manufacturer, filter->manufacturer) == NULL) return false;
  if (filter->name_contains && strstr(summary.name, filter->name_contains) == NULL) return false;
  if (filter->only_calibrated && summary.validation_flags != PROFILE_VALID_ALL) return false;
  if (summary.quality_score < filter->min_quality_score) return false;
  return true;
}

int profiles_init() {
  Serial.println("Profiles: Initializing profile management");
  g_active_profile_slot = -1;
  return 0;
}

int profiles_list(ProfileSummary *summaries, int max_count, const ProfileFilter *filter) {
  if (!summaries || max_count <= 0) return 0;

  int count = 0;
  for (int slot = 0; slot < PROFILE_MAX_SLOTS && count < max_count; slot++) {
    StoredLensProfile stored;
    if (storage_load_lens_profile(slot, &stored) != 0) continue;

    ProfileSummary summary;
    build_summary(slot, stored, &summary);
    if (!passes_filter(summary, filter)) continue;

    summaries[count] = summary;
    count++;
  }

  Serial.printf("Profiles: Listed %d profiles\n", count);
  return count;
}

int profiles_get_summary(int slot, ProfileSummary *summary) {
  if (slot < 0 || slot >= PROFILE_MAX_SLOTS || !summary) return -1;

  StoredLensProfile stored;
  if (storage_load_lens_profile(slot, &stored) != 0) return -1;

  build_summary(slot, stored, summary);
  return 0;
}

int profiles_create(const LensProfile *profile, int slot) {
  if (!profile) return -1;

  if (slot < 0) {
    for (slot = 0; slot < PROFILE_MAX_SLOTS; slot++) {
      StoredLensProfile temp;
      if (storage_load_lens_profile(slot, &temp) != 0) break;
    }
    if (slot >= PROFILE_MAX_SLOTS) {
      Serial.println("Profiles: No free slots available");
      return -2;
    }
  }

  if (slot < 0 || slot >= PROFILE_MAX_SLOTS) return -1;

  StoredLensProfile stored = {};
  stored.profile = *profile;
  stored.created_time = millis() / 1000;
  stored.last_used_time = 0;
  stored.use_count = 0;

  if (storage_save_lens_profile(&stored, slot) < 0) {
    Serial.printf("Profiles: Failed to save profile to slot %d\n", slot);
    return -3;
  }

  Serial.printf("Profiles: Created profile '%s' in slot %d\n", profile->name, slot);
  return slot;
}

int profiles_load(int slot, LensProfile *profile) {
  if (slot < 0 || slot >= PROFILE_MAX_SLOTS || !profile) return -1;

  StoredLensProfile stored;
  if (storage_load_lens_profile(slot, &stored) != 0) return -1;

  *profile = stored.profile;
  return 0;
}

int profiles_save(int slot, const LensProfile *profile) {
  if (slot < 0 || slot >= PROFILE_MAX_SLOTS || !profile) return -1;

  StoredLensProfile stored;
  if (storage_load_lens_profile(slot, &stored) != 0) {
    return profiles_create(profile, slot);
  }

  stored.profile = *profile;
  return storage_save_lens_profile(&stored, slot);
}

int profiles_delete(int slot) {
  if (slot < 0 || slot >= PROFILE_MAX_SLOTS) return -1;

  if (slot == g_active_profile_slot) g_active_profile_slot = -1;

  int result = storage_delete_lens_profile(slot);
  if (result == 0) Serial.printf("Profiles: Deleted profile in slot %d\n", slot);
  return result;
}

int profiles_quick_load(int slot) {
  if (slot < 0 || slot >= PROFILE_MAX_SLOTS) return -1;

  Serial.printf("Profiles: Quick-loading profile from slot %d\n", slot);

  StoredLensProfile stored;
  if (storage_load_lens_profile(slot, &stored) != 0) {
    Serial.printf("Profiles: Failed to load profile from slot %d\n", slot);
    return -1;
  }

  // doesnt push cal to the encoders yet needs the driver hooked up
  g_active_profile_slot = slot;

  Serial.printf("Profiles: Loaded profile '%s'\n", stored.profile.name);
  return 0;
}

int profiles_get_active() {
  return g_active_profile_slot;
}

int profiles_export_json(int slot, char *json_buffer, size_t buffer_size, bool pretty) {
  if (slot < 0 || slot >= PROFILE_MAX_SLOTS || !json_buffer || buffer_size == 0) return -1;

  StoredLensProfile stored;
  if (storage_load_lens_profile(slot, &stored) != 0) return -1;

  const LensProfile *profile = &stored.profile;
  const char *indent = pretty ? "  " : "";
  const char *newline = pretty ? "\n" : "";
  int len = 0;

  len += snprintf(json_buffer + len, buffer_size - len, "{%s", newline);
  len += snprintf(json_buffer + len, buffer_size - len, "%s\"name\": \"%s\",%s", indent, profile->name, newline);
  len += snprintf(json_buffer + len, buffer_size - len, "%s\"manufacturer\": \"%s\",%s", indent, profile->manufacturer, newline);
  len += snprintf(json_buffer + len, buffer_size - len, "%s\"model\": \"%s\",%s", indent, profile->model, newline);
  len += snprintf(json_buffer + len, buffer_size - len, "%s\"serial_number\": \"%s\",%s", indent, profile->serial_number, newline);

  if (profile->focus_enabled) {
    len += snprintf(json_buffer + len, buffer_size - len, "%s\"focus\": {%s", indent, newline);
    len += snprintf(json_buffer + len, buffer_size - len, "%s%s\"enabled\": true,%s", indent, indent, newline);
    len += snprintf(json_buffer + len, buffer_size - len, "%s%s\"min_mm\": %.2f,%s", indent, indent, profile->focus_min_mm, newline);
    len += snprintf(json_buffer + len, buffer_size - len, "%s%s\"max_mm\": %.2f%s", indent, indent, profile->focus_max_mm, newline);
    len += snprintf(json_buffer + len, buffer_size - len, "%s},%s", indent, newline);
  }

  if (profile->iris_enabled) {
    len += snprintf(json_buffer + len, buffer_size - len, "%s\"iris\": {%s", indent, newline);
    len += snprintf(json_buffer + len, buffer_size - len, "%s%s\"enabled\": true,%s", indent, indent, newline);
    len += snprintf(json_buffer + len, buffer_size - len, "%s%s\"min_fstop\": %.2f,%s", indent, indent, profile->iris_min_fstop, newline);
    len += snprintf(json_buffer + len, buffer_size - len, "%s%s\"max_fstop\": %.2f%s", indent, indent, profile->iris_max_fstop, newline);
    len += snprintf(json_buffer + len, buffer_size - len, "%s},%s", indent, newline);
  }

  if (profile->zoom_enabled) {
    len += snprintf(json_buffer + len, buffer_size - len, "%s\"zoom\": {%s", indent, newline);
    len += snprintf(json_buffer + len, buffer_size - len, "%s%s\"enabled\": true,%s", indent, indent, newline);
    len += snprintf(json_buffer + len, buffer_size - len, "%s%s\"min_mm\": %.2f,%s", indent, indent, profile->zoom_min_mm, newline);
    len += snprintf(json_buffer + len, buffer_size - len, "%s%s\"max_mm\": %.2f%s", indent, indent, profile->zoom_max_mm, newline);
    len += snprintf(json_buffer + len, buffer_size - len, "%s}%s", indent, newline);
  }

  len += snprintf(json_buffer + len, buffer_size - len, "}%s", newline);

  if ((size_t)len >= buffer_size) {
    Serial.println("Profiles: JSON buffer too small");
    return -2;
  }

  return len;
}

int profiles_import_json(const char *json_string, int slot) {
  (void)json_string;
  (void)slot;
  Serial.println("Profiles: JSON import not yet implemented");
  return -1;
}

int profiles_validate(int slot, uint8_t *validation_flags, uint8_t *quality_score) {
  ProfileSummary summary;
  if (profiles_get_summary(slot, &summary) != 0) return -1;

  if (validation_flags) *validation_flags = summary.validation_flags;
  if (quality_score) *quality_score = summary.quality_score;

  return (summary.validation_flags == PROFILE_VALID_ALL) ? 0 : -1;
}

int profiles_clone(int source_slot, int dest_slot, const char *new_name) {
  if (source_slot < 0 || source_slot >= PROFILE_MAX_SLOTS) return -1;

  LensProfile profile;
  if (profiles_load(source_slot, &profile) != 0) return -1;

  if (new_name) {
    strncpy(profile.name, new_name, sizeof(profile.name) - 1);
    profile.name[sizeof(profile.name) - 1] = '\0';
  }

  return profiles_create(&profile, dest_slot);
}

int profiles_find_by_name(const char *name) {
  if (!name) return -1;

  ProfileSummary summaries[PROFILE_MAX_SLOTS];
  int count = profiles_list(summaries, PROFILE_MAX_SLOTS, NULL);

  for (int i = 0; i < count; i++) {
    if (strcasecmp(summaries[i].name, name) == 0) return summaries[i].slot;
  }

  return -1;
}

int profiles_get_statistics(int *total_profiles, int *free_slots, int *active_slot) {
  ProfileSummary summaries[PROFILE_MAX_SLOTS];
  int count = profiles_list(summaries, PROFILE_MAX_SLOTS, NULL);

  if (total_profiles) *total_profiles = count;
  if (free_slots) *free_slots = PROFILE_MAX_SLOTS - count;
  if (active_slot) *active_slot = g_active_profile_slot;

  return 0;
}

int profiles_factory_reset(uint32_t confirm) {
  if (confirm != 0xDEADBEEF) return -1;

  Serial.println("Profiles: Factory reset - deleting all profiles");
  for (int slot = 0; slot < PROFILE_MAX_SLOTS; slot++) {
    storage_delete_lens_profile(slot);
  }
  g_active_profile_slot = -1;
  return 0;
}

int profiles_format_name(const LensProfile *profile, char *buffer, size_t buffer_size) {
  if (!profile || !buffer || buffer_size == 0) return 0;
  return snprintf(buffer, buffer_size, "%s %s %s", profile->manufacturer, profile->model, profile->name);
}

int profiles_format_specs(const LensProfile *profile, char *buffer, size_t buffer_size) {
  if (!profile || !buffer || buffer_size == 0) return 0;

  int len = 0;
  if (profile->focus_enabled) {
    len += snprintf(buffer + len, buffer_size - len, "Focus: %.0fmm-%.0fmm  ",
                     profile->focus_min_mm, profile->focus_max_mm);
  }
  if (profile->iris_enabled) {
    len += snprintf(buffer + len, buffer_size - len, "Iris: T%.1f-T%.1f  ",
                     profile->iris_min_fstop, profile->iris_max_fstop);
  }
  if (profile->zoom_enabled) {
    len += snprintf(buffer + len, buffer_size - len, "Zoom: %.0fmm-%.0fmm",
                     profile->zoom_min_mm, profile->zoom_max_mm);
  }
  return len;
}
