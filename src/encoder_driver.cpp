#include "encoder_driver.h"
#include "math_engine.h"

// public encoder_id stays 0 focus 1 iris 2 zoom everywhere else in the
// codebase this array only covers the two spi ones index by g_spi_slot
struct EncoderBus {
  SPIClass *spi;
  uint8_t cs_pin;
};

static EncoderBus g_spi_buses[2] = {
    {&SPI, ENCODER_CS_FOCUS},
    {&SPI1, ENCODER_CS_ZOOM},
};

// focus is g_spi_buses[0] zoom is g_spi_buses[1] iris has no entry its i2c
static uint8_t g_spi_slot(uint8_t encoder_id) {
  return encoder_id == 0 ? 0 : 1;
}

static const SPISettings AS5048_SPI_SETTINGS(ENCODER_SPI_BAUDRATE, MSBFIRST, SPI_MODE1);

static IntervalTimer g_sampling_timer;
static volatile uint8_t g_active_mask = 0;
static volatile uint16_t g_latest_angle[3] = {0, 0, 0};
static volatile int32_t g_latest_unwrapped[3] = {0, 0, 0};
static volatile bool g_latest_valid[3] = {false, false, false};
static volatile uint32_t g_sample_count = 0;
// previous post-deadband raw per encoder, only ever touched by that
// encoder's own writer (isr for focus/zoom, encoder_process for iris)
static uint16_t g_prev_raw[3] = {0, 0, 0};

// spi error flag state per encoder (index 1 unused, iris is i2c). written
// by the isr, the transition report is printed from the main loop
static volatile bool g_error_active[3] = {false, false, false};
static uint32_t g_error_clear_at[3] = {0, 0, 0};
static volatile uint32_t g_error_count[3] = {0, 0, 0};
static volatile uint8_t g_error_reg[3] = {0, 0, 0};
static bool g_error_reported[3] = {false, false, false};

static uint32_t g_iris_sample_count = 0;
static uint32_t g_iris_period_us = 1000;
static elapsedMicros g_iris_since_read = 0;

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

// chip give u last read not this one its always one behind
static uint16_t as5048_read_register(const EncoderBus &bus, uint16_t command) {
  as5048_transfer(bus, as5048_calculate_parity(command));
  return as5048_transfer(bus, AS5048_CMD_NOP);
}

// the error flag is latched. datasheet DS000298 v1-11 p19, verbatim: "if
// the error flag is set to high because of a communication problem the
// flag remains set until it will be cleared by the CLEAR ERROR FLAG
// command". every frame after one glitch (a connector wiggle, a parity
// hit, the encoder powering up a moment after the first isr frame) came
// back with EF set, got discarded, and that ring froze silently for
// good - nothing issued the clear. the clear is itself a read of 0x0001,
// its response carries the error register, and the read after that is
// clean again. rate limited so an unplugged encoder (floating miso reads
// as EF every tick) doesnt cost a clear every millisecond forever
static void as5048_clear_error(uint8_t bus_slot, uint8_t encoder_id) {
  if (g_error_active[encoder_id] && (g_sample_count - g_error_clear_at[encoder_id]) < 1000) return;
  uint16_t err = as5048_read_register(g_spi_buses[bus_slot], AS5048_CMD_CLEAR_ERROR | AS5048_READ_FLAG);
  g_error_clear_at[encoder_id] = g_sample_count;
  if (!g_error_active[encoder_id]) {
    g_error_active[encoder_id] = true;
    g_error_count[encoder_id]++;
    g_error_reg[encoder_id] = err & 0x07;
  }
}

// iris as5048b over i2c register pairs are high byte then low 6 bits same
// combine as the real ams_as5048b library not the spi 16bit framing
// return value is whether the i2c transaction actually succeeded callers
// must check it a disconnected chip must not look like a real angle of 0
static bool as5048b_read_reg8(uint8_t reg, uint8_t *out) {
  Wire.beginTransmission(AS5048B_I2C_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)AS5048B_I2C_ADDRESS, 1) < 1) return false;
  if (Wire.available() < 1) return false;
  *out = Wire.read();
  return true;
}

static bool as5048b_read_reg16(uint8_t reg_msb, uint16_t *out) {
  Wire.beginTransmission(AS5048B_I2C_ADDRESS);
  Wire.write(reg_msb);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)AS5048B_I2C_ADDRESS, 2) < 2) return false;
  if (Wire.available() < 2) return false;
  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();
  *out = (((uint16_t)msb) << 6) | (lsb & 0x3F);
  return true;
}

static bool as5048b_present() {
  Wire.beginTransmission(AS5048B_I2C_ADDRESS);
  return Wire.endTransmission() == 0;
}

// the multi turn unwrap lives here, at the real sample rate, not in the
// main loop. it used to run in math_process_angle at loop rate on
// whatever the isr had last latched, so any loop stall longer than the
// time to turn the shaft half a revolution (the 10s network reconfigure
// did it easily) aliased a whole turn, and handle_boundary_overshoot
// then quietly dragged the calibration window along to match. same
// deadband and angle_distance math, just moved to where every sample is
// actually seen
static void accumulate(uint8_t i, uint16_t raw) {
  raw = apply_deadband(raw, g_prev_raw[i], ENCODER_DEADBAND);
  g_latest_unwrapped[i] += angle_distance(g_prev_raw[i], raw);
  g_prev_raw[i] = raw;
  g_latest_angle[i] = raw;
  g_latest_valid[i] = true;
}

// only focus and zoom iris is polled from encoder_process() in the main
// loop not in here see the header comment on encoder_process for why
static void sampling_isr() {
  static const uint8_t spi_ids[2] = {0, 2};

  for (uint8_t k = 0; k < 2; k++) {
    uint8_t i = spi_ids[k];
    if (!(g_active_mask & (1 << i))) continue;

    uint16_t response = as5048_read_register(g_spi_buses[k], AS5048_CMD_ANGLE | AS5048_READ_FLAG);
    if (!(response & AS5048_EF_ERROR)) {
      accumulate(i, response & 0x3FFF);
      g_error_active[i] = false;
    } else {
      as5048_clear_error(k, i);
    }
  }
  g_sample_count++;
}

int encoder_driver_init() {
  Serial.println("Encoder: Initializing driver");

  pinMode(ENCODER_CS_FOCUS, OUTPUT);
  pinMode(ENCODER_CS_ZOOM, OUTPUT);
  digitalWrite(ENCODER_CS_FOCUS, HIGH);
  digitalWrite(ENCODER_CS_ZOOM, HIGH);

  SPI.begin();
  SPI1.begin();
  Wire.begin();
  Wire.setClock(400000);

  // whatever the chips latched during power up, start clean
  as5048_read_register(g_spi_buses[0], AS5048_CMD_CLEAR_ERROR | AS5048_READ_FLAG);
  as5048_read_register(g_spi_buses[1], AS5048_CMD_CLEAR_ERROR | AS5048_READ_FLAG);

  Serial.println("Encoder: Driver initialized");
  return 0;
}

uint8_t encoder_detect_presence() {
  static const char *names[3] = {"Focus", "Iris", "Zoom"};
  uint8_t detected_mask = 0;

  Serial.println("Encoder: Detecting connected encoders...");

  for (uint8_t i = 0; i < 3; i++) {
    uint16_t angle;
    uint16_t min_angle, max_angle;

    if (i == 1) {
      if (!as5048b_present() || !as5048b_read_reg16(AS5048B_REG_ANGLE_MSB, &angle)) {
        Serial.printf("  %s encoder not detected\n", names[i]);
        continue;
      }
      min_angle = angle;
      max_angle = angle;
      for (int j = 0; j < 100; j++) {
        uint16_t sample;
        if (as5048b_read_reg16(AS5048B_REG_ANGLE_MSB, &sample)) {
          if (sample < min_angle) min_angle = sample;
          if (sample > max_angle) max_angle = sample;
        }
        delayMicroseconds(100);
      }
    } else {
      const EncoderBus &bus = g_spi_buses[g_spi_slot(i)];
      uint16_t response = as5048_read_register(bus, AS5048_CMD_ANGLE | AS5048_READ_FLAG);

      if (response == 0x0000 || response == 0xFFFF) {
        Serial.printf("  %s encoder not detected\n", names[i]);
        continue;
      }

      angle = response & 0x3FFF;
      if (angle >= AS5048_RESOLUTION) continue;

      min_angle = angle;
      max_angle = angle;
      for (int j = 0; j < 100; j++) {
        uint16_t sample = as5048_read_register(bus, AS5048_CMD_ANGLE | AS5048_READ_FLAG) & 0x3FFF;
        if (sample < min_angle) min_angle = sample;
        if (sample > max_angle) max_angle = sample;
        delayMicroseconds(100);
      }
    }

    detected_mask |= (1 << i);
    Serial.printf("  %s encoder detected: angle=%u, noise=%u\n", names[i], angle, (unsigned)(max_angle - min_angle));
  }

  return detected_mask;
}

int encoder_start_sampling(uint8_t encoder_mask, uint32_t sample_rate_hz) {
  Serial.printf("Encoder: Starting continuous sampling at %lu Hz\n", (unsigned long)sample_rate_hz);

  g_active_mask = encoder_mask;
  uint32_t period_us = 1000000 / sample_rate_hz;
  g_iris_period_us = period_us;
  g_iris_since_read = 0;
  g_sampling_timer.begin(sampling_isr, period_us);

  return 0;
}

void encoder_stop_sampling() {
  g_sampling_timer.end();
  g_active_mask = 0;
}

void encoder_process() {
  // error/recovery transitions for the spi encoders, printed here not in
  // the isr. visible, not silent, and not a diagnostics ui either
  static const uint8_t spi_ids[2] = {0, 2};
  for (uint8_t k = 0; k < 2; k++) {
    uint8_t i = spi_ids[k];
    bool active = g_error_active[i];
    if (active && !g_error_reported[i]) {
      Serial.printf("Encoder: %s spi error flag set (error register 0x%X, count %lu), clearing\n",
                    i == 0 ? "focus" : "zoom", g_error_reg[i], (unsigned long)g_error_count[i]);
      g_error_reported[i] = true;
    } else if (!active && g_error_reported[i]) {
      Serial.printf("Encoder: %s spi reads clean again\n", i == 0 ? "focus" : "zoom");
      g_error_reported[i] = false;
    }
  }

  if (!(g_active_mask & (1 << 1))) return;
  if (g_iris_since_read < g_iris_period_us) return;
  g_iris_since_read = 0;

  // loop rate is iris's real sample rate (it's polled over i2c from here),
  // so unlike focus/zoom a stall still costs it - inherent to i2c from
  // the main loop, not something the isr move can fix
  uint16_t angle;
  if (as5048b_read_reg16(AS5048B_REG_ANGLE_MSB, &angle)) {
    accumulate(1, angle);
    g_iris_sample_count++;
  }
}

void encoder_refresh_now() {
  uint8_t mask = g_active_mask;

  // iris first, its i2c read is the slow one (~0.15 ms at 400 kHz), so the
  // fast moving focus and zoom are read last, closest to the packet
  if (mask & (1 << 1)) {
    uint16_t angle;
    if (as5048b_read_reg16(AS5048B_REG_ANGLE_MSB, &angle)) {
      accumulate(1, angle);
      g_iris_sample_count++;
      g_iris_since_read = 0;
    }
  }

  // take focus/zoom out of the isr mask while reading them here, same
  // pattern as encoder_read_angle: the isr preempts the main loop, never
  // the other way round, so once the bit is clear no isr touches that bus
  // or that encoder's state until it's put back
  static const uint8_t spi_ids[2] = {0, 2};
  uint8_t spi_mask = mask & ((1 << 0) | (1 << 2));
  g_active_mask = mask & ~spi_mask;
  for (uint8_t k = 0; k < 2; k++) {
    uint8_t i = spi_ids[k];
    if (!(spi_mask & (1 << i))) continue;
    uint16_t response = as5048_read_register(g_spi_buses[k], AS5048_CMD_ANGLE | AS5048_READ_FLAG);
    if (!(response & AS5048_EF_ERROR)) {
      accumulate(i, response & 0x3FFF);
      g_error_active[i] = false;
    } else {
      as5048_clear_error(k, i);
    }
  }
  g_active_mask = mask;
}

bool encoder_read_angle(uint8_t encoder_id, uint16_t *angle) {
  if (encoder_id >= 3 || !angle) return false;

  // pulls it out the isr mask so they dont fight the bus
  uint8_t saved_mask = g_active_mask;
  g_active_mask = saved_mask & ~(1 << encoder_id);

  bool ok;
  if (encoder_id == 1) {
    ok = as5048b_read_reg16(AS5048B_REG_ANGLE_MSB, angle);
  } else {
    uint16_t response = as5048_read_register(g_spi_buses[g_spi_slot(encoder_id)], AS5048_CMD_ANGLE | AS5048_READ_FLAG);
    ok = !(response & AS5048_EF_ERROR);
    if (ok) {
      *angle = response & 0x3FFF;
    } else {
      // same latch, same clear - so calibration detection self heals on
      // the next attempt instead of failing forever after one glitch
      as5048_read_register(g_spi_buses[g_spi_slot(encoder_id)], AS5048_CMD_CLEAR_ERROR | AS5048_READ_FLAG);
    }
  }

  g_active_mask = saved_mask;
  return ok;
}

bool encoder_get_latest_sample(uint8_t encoder_id, uint16_t *angle, int32_t *unwrapped, uint32_t *sample_count) {
  if (encoder_id >= 3 || !angle || !unwrapped) return false;

  noInterrupts();
  bool valid = g_latest_valid[encoder_id];
  uint16_t value = g_latest_angle[encoder_id];
  int32_t acc = g_latest_unwrapped[encoder_id];
  uint32_t count = (encoder_id == 1) ? g_iris_sample_count : g_sample_count;
  interrupts();

  if (!valid) return false;
  *angle = value;
  *unwrapped = acc;
  if (sample_count) *sample_count = count;
  return true;
}

int encoder_get_diagnostics(uint8_t encoder_id, uint16_t *agc, uint16_t *magnitude) {
  if (encoder_id >= 3 || !agc || !magnitude) return -1;

  if (encoder_id == 1) {
    uint8_t agc_val;
    if (!as5048b_read_reg8(AS5048B_REG_AGC, &agc_val)) return -1;
    if (!as5048b_read_reg16(AS5048B_REG_MAGNITUDE_MSB, magnitude)) return -1;
    *agc = agc_val;
    return 0;
  }

  const EncoderBus &bus = g_spi_buses[g_spi_slot(encoder_id)];

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

  uint8_t saved_mask = g_active_mask;
  g_active_mask = saved_mask & ~(1 << encoder_id);

  if (encoder_id == 1) {
    for (size_t i = 0; i < count; i++) {
      uint16_t sample = 0;
      as5048b_read_reg16(AS5048B_REG_ANGLE_MSB, &sample);
      samples[i] = sample;
      delayMicroseconds(1000);
    }
  } else {
    const EncoderBus &bus = g_spi_buses[g_spi_slot(encoder_id)];
    for (size_t i = 0; i < count; i++) {
      uint16_t response = as5048_read_register(bus, AS5048_CMD_ANGLE | AS5048_READ_FLAG);
      samples[i] = response & 0x3FFF;
      delayMicroseconds(1000);
    }
  }

  g_active_mask = saved_mask;
  return count;
}
