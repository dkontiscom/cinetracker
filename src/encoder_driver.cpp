#include "encoder_driver.h"

struct EncoderBus {
  SPIClass *spi;
  uint8_t cs_pin;
};

static EncoderBus g_buses[3] = {
    {&SPI, ENCODER_CS_FOCUS},
    {&SPI1, ENCODER_CS_IRIS},
    {&SPI2, ENCODER_CS_ZOOM},
};

static const SPISettings AS5048_SPI_SETTINGS(ENCODER_SPI_BAUDRATE, MSBFIRST, SPI_MODE1);

static IntervalTimer g_sampling_timer;
static volatile uint8_t g_active_mask = 0;
static volatile uint16_t g_latest_angle[3] = {0, 0, 0};
static volatile bool g_latest_valid[3] = {false, false, false};
static volatile uint32_t g_sample_count = 0;

uint16_t as5048_calculate_parity(uint16_t value) {
  uint16_t count = 0;
  for (int i = 0; i < 16; i++) {
    if (value & (1 << i)) count++;
  }
  if (count & 1) value |= AS5048_PARITY_BIT;
  return value;
}

static uint16_t as5048_transfer(const EncoderBus &bus, uint16_t tx) {
  bus.spi->beginTransaction(AS5048_SPI_SETTINGS);
  digitalWrite(bus.cs_pin, LOW);
  delayMicroseconds(10);
  uint16_t rx = bus.spi->transfer16(tx);
  digitalWrite(bus.cs_pin, HIGH);
  bus.spi->endTransaction();
  delayMicroseconds(10);
  return rx;
}

static uint16_t as5048_read_register(const EncoderBus &bus, uint16_t command) {
  as5048_transfer(bus, as5048_calculate_parity(command));
  return as5048_transfer(bus, AS5048_CMD_NOP);
}

static void sampling_isr() {
  for (uint8_t i = 0; i < 3; i++) {
    if (!(g_active_mask & (1 << i))) continue;

    uint16_t response = as5048_read_register(g_buses[i], AS5048_CMD_ANGLE | AS5048_READ_FLAG);
    if (!(response & AS5048_EF_ERROR)) {
      g_latest_angle[i] = response & 0x3FFF;
      g_latest_valid[i] = true;
    }
  }
  g_sample_count++;
}

int encoder_driver_init() {
  Serial.println("Encoder: Initializing driver");

  pinMode(ENCODER_CS_FOCUS, OUTPUT);
  pinMode(ENCODER_CS_IRIS, OUTPUT);
  pinMode(ENCODER_CS_ZOOM, OUTPUT);
  digitalWrite(ENCODER_CS_FOCUS, HIGH);
  digitalWrite(ENCODER_CS_IRIS, HIGH);
  digitalWrite(ENCODER_CS_ZOOM, HIGH);

  SPI.begin();
  SPI1.begin();
  SPI2.begin();

  Serial.println("Encoder: Driver initialized");
  return 0;
}

uint8_t encoder_detect_presence() {
  static const char *names[3] = {"Focus", "Iris", "Zoom"};
  uint8_t detected_mask = 0;

  Serial.println("Encoder: Detecting connected encoders...");

  for (uint8_t i = 0; i < 3; i++) {
    uint16_t response = as5048_read_register(g_buses[i], AS5048_CMD_ANGLE | AS5048_READ_FLAG);

    if (response == 0x0000 || response == 0xFFFF) {
      Serial.printf("  %s encoder not detected\n", names[i]);
      continue;
    }

    uint16_t angle = response & 0x3FFF;
    if (angle >= AS5048_RESOLUTION) continue;

    detected_mask |= (1 << i);

    uint16_t min_angle = angle;
    uint16_t max_angle = angle;
    for (int j = 0; j < 100; j++) {
      uint16_t sample = as5048_read_register(g_buses[i], AS5048_CMD_ANGLE | AS5048_READ_FLAG) & 0x3FFF;
      if (sample < min_angle) min_angle = sample;
      if (sample > max_angle) max_angle = sample;
      delayMicroseconds(100);
    }

    Serial.printf("  %s encoder detected: angle=%u, noise=%u\n", names[i], angle, (unsigned)(max_angle - min_angle));
  }

  return detected_mask;
}

int encoder_start_sampling(uint8_t encoder_mask, uint32_t sample_rate_hz) {
  Serial.printf("Encoder: Starting continuous sampling at %lu Hz\n", (unsigned long)sample_rate_hz);

  g_active_mask = encoder_mask;
  uint32_t period_us = 1000000 / sample_rate_hz;
  g_sampling_timer.begin(sampling_isr, period_us);

  return 0;
}

void encoder_stop_sampling() {
  g_sampling_timer.end();
  g_active_mask = 0;
}

bool encoder_read_angle(uint8_t encoder_id, uint16_t *angle) {
  if (encoder_id >= 3 || !angle) return false;

  uint8_t saved_mask = g_active_mask;
  g_active_mask = saved_mask & ~(1 << encoder_id);

  uint16_t response = as5048_read_register(g_buses[encoder_id], AS5048_CMD_ANGLE | AS5048_READ_FLAG);

  g_active_mask = saved_mask;

  if (response & AS5048_EF_ERROR) return false;
  *angle = response & 0x3FFF;
  return true;
}

bool encoder_get_latest_sample(uint8_t encoder_id, uint16_t *angle, uint32_t *sample_count) {
  if (encoder_id >= 3 || !angle) return false;

  noInterrupts();
  bool valid = g_latest_valid[encoder_id];
  uint16_t value = g_latest_angle[encoder_id];
  uint32_t count = g_sample_count;
  interrupts();

  if (!valid) return false;
  *angle = value;
  if (sample_count) *sample_count = count;
  return true;
}

int encoder_get_diagnostics(uint8_t encoder_id, uint16_t *agc, uint16_t *magnitude) {
  if (encoder_id >= 3 || !agc || !magnitude) return -1;
  const EncoderBus &bus = g_buses[encoder_id];

  uint16_t diag = as5048_read_register(bus, AS5048_CMD_DIAGNOSTICS | AS5048_READ_FLAG);
  if (diag & AS5048_EF_ERROR) return -1;
  *agc = (diag >> 8) & 0xFF;

  uint16_t mag = as5048_read_register(bus, AS5048_CMD_MAGNITUDE | AS5048_READ_FLAG);
  if (mag & AS5048_EF_ERROR) return -1;
  *magnitude = mag & 0x3FFF;

  return 0;
}

int encoder_collect_calibration_samples(uint8_t encoder_id, uint16_t *samples, size_t count) {
  if (encoder_id >= 3 || !samples) return -1;
  const EncoderBus &bus = g_buses[encoder_id];

  uint8_t saved_mask = g_active_mask;
  g_active_mask = saved_mask & ~(1 << encoder_id);

  for (size_t i = 0; i < count; i++) {
    uint16_t response = as5048_read_register(bus, AS5048_CMD_ANGLE | AS5048_READ_FLAG);
    samples[i] = response & 0x3FFF;
    delayMicroseconds(1000);
  }

  g_active_mask = saved_mask;
  return count;
}
