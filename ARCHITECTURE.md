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

- Ethernet + PTP: native Teensy 4.1 Ethernet MAC via QNEthernet (PTP fork,
  see below for which one), hardware timestamping in the MAC itself, not
  software-timed.
- Genlock: hardware timer / FlexPWM output, GPIO pins per the
  t41-tri-sync-ptp reference (their example uses pins 10 and 12). PTP-locked.
- AES67 / LTC: I2S/SAI with DMA, per the AES67Teensy reference. One audio in
  (LTC from a local device -> streamed to network), one audio out (LTC from
  network -> local device). Not a general audio router.
- FIZ encoders: 3x AS5048A, each on its own SPI bus (Teensy 4.1 has three:
  SPI, SPI1, SPI2), sampled via IntervalTimer at 1kHz. Avoid using pin 10 for
  an encoder chip-select since genlock uses it for sync output.
- Web UI / fleet discovery: runs in loop(), non-blocking. Read-only fleet
  view is the baseline, per-device control if it turns out easy.

Open items, not resolved yet:
- Exact SPI1/SPI2 pin numbers need to be checked against the official Teensy
  4.1 pinout before any wiring happens - don't trust a pin number here until
  it's been cross-checked.
- t41-tri-sync-ptp and AES67Teensy depend on two different QNEthernet PTP
  forks (HedgeHawk vs ssilverman, different branches). Need to check if
  they're actually compatible before combining genlock+PTP and AES67 in one
  firmware.
- Interrupt priority order across genlock/PTP/audio/encoders/ethernet still
  needs to be set once all the pieces exist - genlock should win.

See CREDITS.md for what this is built on.
