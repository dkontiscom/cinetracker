Basic architecture, Teensy 4.1 build. this will get more detailed as we
actually build each part, this is just the starting point.

Priorities, in order: genlock, timecode (LTC over AES67), FIZ encoder data
and its FreeD output. same latency and framerate targets as the old RP2040
version (sub ms encoder latency, up to 240fps FreeD). everything else
(fleet UI, diagnostics, RTC, status LEDs) is secondary and shouldnt get in
the way of those three.

Main design rule: anything time critical runs off a hardware timer, DMA or
interrupt, not off the main loop(). Teensy 4.1 only has one core (RT1062,
Cortex-M7, 600MHz, has an FPU unlike the old RP2040's M0+ cores), so loop()
is reserved for the web server, JSON building, storage writes, things that
can tolerate being delayed by a few hundred microseconds. interrupts always
preempt loop() on this chip, so as long as the ISRs stay short, genlock/PTP/
audio/encoders keep their timing even while the web server is busy.

Subsystems and what drives them:

- Ethernet + PTP: native teensy 4.1 ethernet mac via QNEthernet, hardware
  timestamping in the mac itself not software timed. no pins needed,
  official ethernet kit connects thru its own dedicated pads, checked and
  its not in the standard numbered pin set.

  PTP client is t41-ptp (IMS-AS-LUH), one shared client feeding both
  genlock and aes67 timing, not two separate clients. checked this
  properly by cloning both t41-ptp and AES67Teensy and diffing the actual
  source - AES67Teensy's vendored copy still has unresolved git merge
  conflict markers committed straight into ptp-base.h/cpp, l3ptp.h and
  t41-ptp.h, wouldnt even compile as is, so thats not usable.

  turns out theres a second problem we only found once we actually wrote
  the genlock code and it failed to compile: the standalone IMS-AS-LUH/
  t41-ptp repo (what we first vendored via platformio.ini) is STALE
  compared to the copy bundled inside t41-tri-sync-ptp itself. the bundled
  one has an extended PTPBase::begin() (frameDuration, jumpThreshold, rate
  limit, update rate, two callbacks, skipAtStart) specifically so PTP
  clock corrections dont land in the middle of a genlock pulse and glitch
  the signal - the standalone repos plain begin() cant do this at all.
  so we now vendor the newer bundled copy directly into our own tree at
  lib/t41-ptp (and lib/t41-tri-sync for the pulse generator), commit
  4a121127f63f826a1db01534c0b7c4614c448026, instead of depending on the
  standalone repo through platformio.ini - removed that line. MIT
  license confirmed on both, checked directly in the repo.

  needs a specific QNEthernet fork per t41-ptp's own readme:
  HedgeHawk/QNEthernet branch ieee1588-2-fix. switched platformio.ini to
  it and rebuilt everything - network_stack.cpp, fleet.cpp and
  web_interface.cpp all compiled unchanged against it, real confirmation
  not just an assumption. found one real bug doing this: the HedgeHawk
  fork itself is missing #include <math.h> in lwip_t41.c (round() used
  without it, hard compile error not a warning). fixed with
  "-include math.h" in build_flags rather than patching the vendored
  copy directly, since .pio/libdeps gets freshly redownloaded on every
  machine and a local patch wouldnt survive that.
- Genlock: built, see src/genlock.cpp. pins are NOT hardcoded in
  t41-tri-sync-ptp, checked the source for this - begin() takes the two
  sync pins as parameters, driven off the ethernet ptp hardware timer
  through digitalWriteFast(), not a dedicated pwm peripheral, so no real
  pin constraint here. using pins 2 and 3, interrupt channel 2. a real
  SMPTE ST0274-2008 tri-level sync generator, cites the actual spec
  sections in its own source, not guesswork.

  fps: originally only 30 or 60 compiled (#error for anything else).
  checked a real broadcast timing reference (skyworks AN377, pulled the
  actual clock rate table, not guessed) - 24, 25 and 30fps 1080p all use
  the same 74.25MHz sample clock, the vendored code already had that
  constant right for 30fps, the author just never added the other two
  cases to the same branch. added TRISYNC_FPS==25 and ==24 to that
  branch in lib/t41-tri-sync, tested all of 24/25/30/60 actually compile
  not just assumed. GENLOCK_FPS in genlock.h picks which at compile time,
  still not runtime switchable - see open items below.

  still not handled: true fractional rates (23.976, 29.97, 59.94), which
  us cinema/tv work uses constantly and are NOT the same as 24/30/60
  exactly, would need restructuring TRISYNC_FPS from an integer #define
  match to something that can represent the /1.001 factor. flagged, not
  fixed, since it wasnt what was asked for this pass.
- AES67 / LTC: i2s/sai with dma, per the AES67Teensy reference. only one
  direction active at a time, user picks in the web ui - either ltc in
  (local device -> streamed to network) or ltc out (network -> local
  device), never both at once, not a general audio router. needs a codec
  with both adc and dac, switched in firmware, not true full duplex.
  AES67Teensy itself is 16bit/48khz today, checked their RTPAudio.h directly
  and its because they built it on the stock AudioRecordQueue/AudioPlayQueue
  classes which are hardcoded int16, not because the codec or aes67 itself
  needs 16bit. we checked the actual sgtl5000 datasheet too and the chip is
  24bit capable at the hardware level.

  hardware to buy: PJRC "Audio board for use with Teensy 4.0-4.1", currently
  Rev D2 (same as the Rev D everyone references, just a jan 2023 refresh,
  still sgtl5000). confirmed against the real pjrc product page, this is the
  right part, no alternate codec board needed.

  for actually driving it at 24bit: using OpenAudio_ArduinoLibrary by
  chip audette. checked its real teensy 4.x sai register config directly
  (WNW/W0W set to 32bit word slots, genuine wide i2s transfer not just
  internal float math) so we know it actually works, not just taking the
  readme's word for it. github's api said license: null on that repo at
  first look which worried us, but that only checks for a standalone
  LICENSE file - the actual source files carry a full MIT header, checked
  directly. so this is a real dependency, not just a reference.

  one real gotcha found in the sgtl5000 datasheet: its 24bit register mode
  only works at a specific clock setting (SCLKFREQ=0), worth remembering
  when we actually build this. pins still not finalized, the shield's i2s
  pinout is fixed by the board itself so dont want to guess at it, will
  check properly when we get there.
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
- interrupt priority order across genlock/ptp/audio/encoders/ethernet still
  needs setting once all the pieces actually exist - genlock should win.
- audio shield i2s/i2c pin numbers, havent checked these against source yet.
- genlock fps (24/25/30/60) is a compile time #define, switching for a
  different production means a reflash right now, not a web ui setting.
  true fractional rates (23.976/29.97/59.94) not supported at all yet.

See CREDITS.md for what this is built on.
