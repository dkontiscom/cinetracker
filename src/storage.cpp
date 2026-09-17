#include "storage.h"

#include <LittleFS.h>

// littlefs on spare program flash no sd card involved
static LittleFS_Program g_fs;
static bool g_fs_ready = false;

// returns false if fs not mounted check it
static bool write_file(const char *path, const void *data, size_t size) {
  File f = g_fs.open(path, FILE_WRITE_BEGIN);
  if (!f) return false;
  size_t written = f.write((const uint8_t *)data, size);
  f.close();
  return written == size;
}

static bool read_file(const char *path, void *data, size_t size) {
  File f = g_fs.open(path, FILE_READ);
  if (!f) return false;
  size_t got = f.read((uint8_t *)data, size);
  f.close();
  return got == size;
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
  if (!read_file("calibration.bin", cal, sizeof(CalibrationData))) return -1;
  if (!cal->valid) return -1;
  return 0;
}

int storage_save_calibration(const CalibrationData *cal) {
  if (!g_fs_ready || !cal) return -1;
  return write_file("calibration.bin", cal, sizeof(CalibrationData)) ? 0 : -1;
}

int storage_factory_reset(uint32_t confirm) {
  if (confirm != 0xDEADBEEF) return -1;

  Serial.println("Storage: Factory reset");

  if (!g_fs.quickFormat()) return -1;
  return storage_init();
}
