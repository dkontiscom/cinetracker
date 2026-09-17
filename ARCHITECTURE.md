Basic architecture, Teensy 4.1 build. This will get more detailed as we actually
build each part, this is the starting point.

Priorities, in order: genlock, timecode (LTC over AES67), FIZ encoder data and
its FreeD output. Same latency and framerate targets as the old RP2040 version
(sub-ms encoder latency, up to 240fps FreeD). Everything else (fleet UI,
diagnostics, RTC, status LEDs) is secondary and must not get in the way of
those three.

Main design rule: anything time-critical runs off a hardware timer, DMA or
interrupt, not off the main loop(). Teensy 4.1 only has one core (RT1062,
Cortex-M7, 600MHz, has an FPU unlike the old RP2040's M0+ cores), so loop()
is reserved for the web server, JSON building, storage writes - things that
can tolerate being delayed by a few hundred microseconds. Interrupts always
preempt loop() on this chip, so as long as the ISRs stay short, genlock/PTP/
audio/encoders keep their timing even while the web server is busy.

Subsystems and what drives them:

- Ethernet + PTP: native teensy 4.1 ethernet mac via QNEthernet (ptp fork,
  see below which one), hardware timestamping in the mac itself not
  software timed. no pins needed, official ethernet kit connects thru its
  own dedicated pads, checked and its not in the standard numbered pin set.
- Genlock: pins are NOT hardcoded in t41-tri-sync-ptp, i actually went and
  checked the source for this one - begin() takes the two sync pins as
  parameters, driven off the ethernet ptp hardware timer through
  digitalWriteFast(), not a dedicated pwm peripheral, so no real pin
  constraint here. using pins 2 and 3 for now. ptp locked.
- AES67 / LTC: i2s/sai with dma, per the AES67Teensy reference. only one
  direction active at a time, user picks in the web ui - either ltc in
  (local device -> streamed to network) or ltc out (network -> local
  device), never both at once, not a general audio router. needs a codec
  with both adc and dac, switched in firmware, not true full duplex.
  AES67Teensy itself is 16bit/48khz today, 24bit (the actual broadcast std)
  still needs checking against a real codec datasheet before we pick
  hardware, not confirmed either way yet. pins also not finalized, the
  audio shield's i2s pinout is fixed by the shield board itself so dont
  want to guess at it, will check properly when we actually build this.
- FIZ encoders: 3x as5048a, each gets its own spi bus, cs pins checked
  against the real teensy 4.1 core source (SPI.cpp hardware tables) not
  guessed this time:
    - Focus: SPI  - sck 13, mosi 11, miso 12, cs 10
    - Iris:  SPI1 - sck 27, mosi 26, miso 1,  cs 0
    - Zoom:  SPI2 - sck 45, mosi 43, miso 42, cs 44
  sampled via IntervalTimer at 1khz. pin 13 is also the teensy's built in
  led btw (classic arduino spi/led pin overlap thing) - doesnt matter for
  us since we're using external status leds on 4 and 5 not the builtin one.
- Status LEDs (genlock lock / ptp sync): pins 4 and 5 reserved, not built yet.
- Web UI / fleet discovery: runs in loop(), non blocking. read only fleet
  view is the baseline, per device control if it turns out to be easy.

Open items, still not resolved:
- t41-tri-sync-ptp and AES67Teensy depend on two different QNEthernet ptp
  forks (HedgeHawk vs ssilverman, different branches). need to check if
  they're actually compatible before we try combining genlock+ptp and
  aes67 in the same firmware.
- interrupt priority order across genlock/ptp/audio/encoders/ethernet still
  needs setting once all the pieces actually exist - genlock should win.
- audio shield i2s/i2c pin numbers, havent checked these against source yet.

See CREDITS.md for what this is built on.
