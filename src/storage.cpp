#include "storage.h"

#include <LittleFS.h>
#include <string.h>

// littlefs on spare program flash no sd card involved
static LittleFS_Program g_fs;
static bool g_fs_ready = false;

// every file is a small header then the raw payload. littlefs opens with
// no truncate so a file could only ever grow - a struct that shrank left a
// stale tail that read back as valid data. and nothing checked size or
// layout, a file written by a previous struct layout was read straight
// into the new one as garbage with no error. now: magic + layout version
// + payload size up front, the file is removed before every write so it
// cant carry a tail, and the payload is read into scratch and only copied
// out on a full match, so a caller's struct never gets a partial fill
struct FileHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
};

#define STORAGE_MAGIC 0x4B525443u
#define STORAGE_MAX_PAYLOAD 256

// bump the one whose struct changed shape, the old file is then ignored
// and the code falls back to defaults instead of reading the old layout
// as the new one. size is checked too but two layouts can share a size
// 2: dropped the never-written calibration_time field from EncoderCalibration
#define CALIBRATION_LAYOUT 2
#define NETWORK_LAYOUT 1
#define AES67_LAYOUT 1
#define GENLOCK_LAYOUT 1
#define FILTER_LAYOUT 1
#define BOOTFLAG_LAYOUT 1

static_assert(sizeof(CalibrationData) <= STORAGE_MAX_PAYLOAD, "grow STORAGE_MAX_PAYLOAD");
static_assert(sizeof(NetworkConfig) <= STORAGE_MAX_PAYLOAD, "grow STORAGE_MAX_PAYLOAD");
static_assert(sizeof(Aes67Settings) <= STORAGE_MAX_PAYLOAD, "grow STORAGE_MAX_PAYLOAD");

static bool write_file(const char *path, uint16_t version, const void *data, size_t size) {
  if (size > STORAGE_MAX_PAYLOAD) return false;
  g_fs.remove(path);
  File f = g_fs.open(path, FILE_WRITE_BEGIN);
  if (!f) return false;
  FileHeader h = {STORAGE_MAGIC, version, (uint16_t)size};
  bool ok = f.write((const uint8_t *)&h, sizeof(h)) == sizeof(h) &&
            f.write((const uint8_t *)data, size) == size;
  f.close();
  return ok;
}

// a missing file is normal (first boot, after a factory reset) and stays
// quiet, an existing file that doesnt match is worth hearing about
static bool read_file(const char *path, uint16_t version, void *data, size_t size) {
  if (size > STORAGE_MAX_PAYLOAD) return false;
  File f = g_fs.open(path, FILE_READ);
  if (!f) return false;

  FileHeader h;
  uint8_t scratch[STORAGE_MAX_PAYLOAD];
  bool ok = f.read((uint8_t *)&h, sizeof(h)) == sizeof(h) &&
            h.magic == STORAGE_MAGIC && h.version == version && h.size == size &&
            f.read(scratch, size) == size &&
            f.available() == 0;
  f.close();

  if (!ok) {
    Serial.printf("Storage: %s unreadable or stale layout, ignoring\n", path);
    return false;
  }
  memcpy(data, scratch, size);
  return true;
}

int storage_init() {
  Serial.println("Storage: Mounting filesystem");

  if (!g_fs.begin(STORAGE_FS_SIZE)) {
    Serial.println("Storage: Mount failed, formatting");
    if (!g_fs.quickFormat() || !g_fs.begin(STORAGE_FS_SIZE)) {
      Serial.println("Storage: Format failed");
      return -1;
    }
  }

  g_fs_ready = true;
  Serial.println("Storage: Ready");
  return 0;
}

int storage_load_calibration(CalibrationData *cal) {
  if (!g_fs_ready || !cal) return -1;
  if (!read_file("calibration.bin", CALIBRATION_LAYOUT, cal, sizeof(CalibrationData))) return -1;
  if (!cal->valid) return -1;
  return 0;
}

int storage_save_calibration(const CalibrationData *cal) {
  if (!g_fs_ready || !cal) return -1;
  return write_file("calibration.bin", CALIBRATION_LAYOUT, cal, sizeof(CalibrationData)) ? 0 : -1;
}

int storage_factory_reset(uint32_t confirm) {
  if (confirm != 0xDEADBEEF) return -1;

  Serial.println("Storage: Factory reset");

  if (!g_fs.quickFormat()) return -1;
  return storage_init();
}

int storage_load_network_config(NetworkConfig *config) {
  if (!g_fs_ready || !config) return -1;
  return read_file("network.bin", NETWORK_LAYOUT, config, sizeof(NetworkConfig)) ? 0 : -1;
}

int storage_save_network_config(const NetworkConfig *config) {
  if (!g_fs_ready || !config) return -1;
  return write_file("network.bin", NETWORK_LAYOUT, config, sizeof(NetworkConfig)) ? 0 : -1;
}

int storage_load_aes67_settings(Aes67Settings *settings) {
  if (!g_fs_ready || !settings) return -1;
  return read_file("aes67.bin", AES67_LAYOUT, settings, sizeof(Aes67Settings)) ? 0 : -1;
}

int storage_save_aes67_settings(const Aes67Settings *settings) {
  if (!g_fs_ready || !settings) return -1;
  return write_file("aes67.bin", AES67_LAYOUT, settings, sizeof(Aes67Settings)) ? 0 : -1;
}

int storage_load_genlock_fps(uint8_t *fps) {
  if (!g_fs_ready || !fps) return -1;
  return read_file("genlock.bin", GENLOCK_LAYOUT, fps, sizeof(*fps)) ? 0 : -1;
}

int storage_save_genlock_fps(uint8_t fps) {
  if (!g_fs_ready) return -1;
  return write_file("genlock.bin", GENLOCK_LAYOUT, &fps, sizeof(fps)) ? 0 : -1;
}

// through a byte, same as bootflag - reading flash straight into a bool
// is undefined for any value but 0 and 1
int storage_load_filter_enabled(bool *enabled) {
  if (!g_fs_ready || !enabled) return -1;
  uint8_t val = 0;
  if (!read_file("filter.bin", FILTER_LAYOUT, &val, sizeof(val))) return -1;
  *enabled = (val != 0);
  return 0;
}

int storage_save_filter_enabled(bool enabled) {
  if (!g_fs_ready) return -1;
  uint8_t val = enabled ? 1 : 0;
  return write_file("filter.bin", FILTER_LAYOUT, &val, sizeof(val)) ? 0 : -1;
}

int storage_load_clean_shutdown(bool *clean) {
  if (!g_fs_ready || !clean) return -1;
  uint8_t val = 0;
  if (!read_file("bootflag.bin", BOOTFLAG_LAYOUT, &val, sizeof(val))) return -1;
  *clean = (val != 0);
  return 0;
}

int storage_set_clean_shutdown(bool clean) {
  if (!g_fs_ready) return -1;
  uint8_t val = clean ? 1 : 0;
  return write_file("bootflag.bin", BOOTFLAG_LAYOUT, &val, sizeof(val)) ? 0 : -1;
}
