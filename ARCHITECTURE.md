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
- AES67 / LTC: built, see src/aes67.cpp. mono only (its carrying LTC not
  program audio), only one direction active at a time - AES67_LTC_IN
  (local audio in -> RTP out to network) or AES67_LTC_OUT (RTP in from
  network -> local audio out), never both, gated in aes67_process()
  by whatever aes67_set_direction() was last called with. direction and
  destination ip/port are now real web ui settings, GET/POST /api/aes67
  in web_interface.cpp, card in the settings tab in web_content.cpp.
  boots at AES67_OFF with the main.cpp default dest until changed.

  hardware: dropped the PJRC audio shield (sgtl5000 chip). it forced
  16bit unless you went into its internals to override it, and it has
  its own onboard sd card slot wired to the exact same pins (10/11/12/13)
  as the Focus encoders spi bus - a real conflict, not a hypothetical
  one, found by checking the real pjrc pin table not by guessing.

  using a PCM5102A (dac, ltc out) + PCM1808 (adc, ltc in) pair instead.
  both are bare chips with no i2c and no control registers at all - bit
  depth/format is set by tying a couple pins to gnd/3v3 on the board
  itself, so 24bit is just how the chip behaves, nothing to enable or
  override in software. neither board has an sd card or anything else
  on it. confirmed real teensy wiring for this pair from someone whos
  actually built it: bck 21, lrclk 20, mclk 23, dac data 7, adc data 8 -
  same i2s pin group the sgtl5000 shield used, still doesnt touch
  10/11/12/13, so the Focus encoder pin conflict is gone by hardware
  choice not by moving the encoder off its pins.

  24bit: using OpenAudio_ArduinoLibrary (AudioInputI2S_F32/
  AudioOutputI2S_F32/AudioRecordQueue_F32/AudioPlayQueue_F32), a real
  dependency in platformio.ini, MIT confirmed in the source file headers.
  these classes just drive the raw i2s peripheral, they dont know or
  care what chip is on the other end, so no codec control class is
  needed at all with this pair - aes67_init() does AudioMemory_F32()
  and starts the record queue, thats it.

  RTP: plain L24 mono over RTP (RFC 3190 style), 48 samples/packet at
  48khz (1ms packet time, matches the AES67Teensy reference), dynamic
  payload type 98 - not IANA fixed for L24, just needs to match what we
  advertise. announces itself via SAP (RFC 2974, the real well known
  239.255.255.255:9875 multicast address, matches AES67Teensy) every
  30s with a real SDP description, not a stub.

  jitter buffer on rx: 4 packet ring, primes on the first 2 packets then
  plays out on its own micros() pace independent of when packets actually
  arrive. this is what actually keeps the output steady - without it
  playout timing would follow network arrival jitter directly, which is
  exactly what you cant have on a sync signal. the spec calls holding a
  small buffer "link offset" and expects a receiver to do this, its not
  in tension with aes67s latency numbers since those numbers already
  assume a buffer is there.
  on underrun (packet actually missing) it plays silence for that one 1ms
  slot instead of stalling. professional ltc readers implement "freewheel"
  for exactly this case - hold the last valid time through a brief gap
  rather than lose lock - but how many ms a specific downstream reader
  will actually freewheel through is not verified yet, depends on
  whatever ends up reading this on the unreal/livelink side. open item,
  not assumed fine.

  i2s/i2c pins: no i2c on this board pair at all (see above). i2s pins
  are fixed by the imxrt sai1 peripheral itself, checked by reading
  output_i2s_f32.cpp directly, nothing we choose or could get wrong.
- FIZ encoders: originally planned as 3x as5048a each on its own spi bus
  (focus/iris/zoom on spi/spi1/spi2). spi2 turned out to be unusable -
  its only two pin options on the teensy 4.1 are the same physical pads
  as the onboard sd card and the psram/flash memory footprint, confirmed
  against the real teensy SPI.h source (comment: "T4.1 has SPI2 pins on
  memory connectors as well as SDCard") and independently against PJRC's
  own forum thread titled "SPI2 Pins on Teensy 4.1?". those pads are also
  reported as physically fragile (~1mm pitch, pads known to pull off the
  board if the wire moves), so not something to build a camera-mounted
  device around even setting the sd card sharing aside.

  fix: zoom stays an as5048a but moves onto the spi1 bus (shares the
  clock/data lines with... nothing else now, see below, distinguished by
  its own cs pin, thats how spi bus sharing is designed to work). iris
  switches to an as5048b instead, same sensor family, i2c interface
  instead of spi, confirmed identical 14bit resolution to the spi part
  from the real ams datasheet, so no precision lost by switching that
  one axis. as5048b register addresses (0xfe/0xff for angle, split high
  byte + low 6 bits) checked against the real datasheet and independently
  against the sosandroid/AMS_AS5048B arduino library, both agree.

  iris is deliberately NOT read inside the same hardware timer interrupt
  as focus/zoom. real reported failure on the pjrc forum: IntervalTimer
  interrupting an in progress Wire transfer wedges the i2c bus (scl stuck
  high, sda stuck low). checked the actual teensy 4.x Wire source and
  confirmed its blocking calls poll hardware registers directly rather
  than waiting on their own interrupt to fire, so it wont deadlock, but
  the community consensus (multiple independent threads) is still to
  keep i2c out of a hardware isr entirely rather than rely on that. iris
  gets its own timer in encoder_process(), called from loop() same as
  aes67_process() and friends, not from the interrupt.

  sampled at 1khz, focus/zoom via IntervalTimer, iris via elapsedMicros
  in the main loop. pin 13 is also the teensy's built in led btw (classic
  arduino spi/led pin overlap thing) - doesnt matter for us since we're
  using external status leds on 4 and 5 not the builtin one.

  multi turn: the as5048a/b are single turn absolute encoders, they only
  ever report where the shaft is within one 360 degree revolution with
  zero memory of how many full turns happened. cinetracker mounts the
  encoder to the lens ring through a swappable gear pair the user picks
  themselves for whatever mounting spot and speed a given shot needs, so
  the gear ratio between lens ring and encoder shaft isnt fixed and isnt
  ours to control - which means a focus pull can genuinely spin the
  encoder more than 360 degrees even though the lens ring itself never
  does. below 360 degrees this only shows up as a filter glitch for a few
  samples crossing the raw 0/16383 point. past 360 degrees its worse than
  a glitch - the same raw angle now maps to more than one real physical
  position and no amount of filtering can tell them apart, the
  information just isnt in a single raw reading.

  fix is EncoderState.unwrapped_angle in math_engine.cpp, a plain int32
  running total. every sample adds angle_distance(prev_raw, new_raw) onto
  it instead of letting the wrapped 14bit value reset every revolution -
  same accumulate-the-delta structure as numpy.unwrap uses for phase data
  (confirmed against numpy's own docs), chosen over filtering the sin/cos
  decomposition (the alternative real approach, confirmed against VESC
  firmware's own encoder docs) because a lens ring is mechanically
  bounded, never spins forever, so the accumulator cant run away the way
  a continuously spinning motor shaft's would. the filters (ema/median/
  kalman) now run on this unwrapped value instead of the raw wrapped one,
  which also kills the sub-360-degree glitch as a side effect - one fix
  covers both cases.

  calibration has to capture unwrapped positions too, not just live
  tracking. handle_cal_point() in web_interface.cpp reads the live
  ctx->encoders[...].unwrapped_angle (continuously running since boot,
  same array process_encoder_sample updates every loop iteration) instead
  of taking a fresh one-shot encoder_read_angle() at each SET button
  press - otherwise any turns the user makes physically rotating the lens
  ring between two button presses would be invisible and the calibrated
  range would be wrong. EncoderCalibration.min_angle/mid_angle/max_angle
  in calibration.h are int32 unwrapped positions now not raw 14bit values,
  and calculate_angle_range()/calibration_angle_to_mm() dropped their old
  modulo wraparound math entirely since theres nothing to wrap anymore in
  continuous space.

  this only stays valid from calibration start through however long the
  device stays powered - a single turn absolute encoder has no way to
  know about turns that happened while it was off, thats a hard limit of
  the sensor not a bug. not a new problem for this project though: a
  fresh 3 point calibration is already required every time a lens gets
  mounted (the physical min/max mm are lens specific anyway), same as
  real lens control systems already require a "set marks" touch per lens.
  turn count is just never trusted past that boundary, nothing more.

  found and fixed two related bugs while building this: network_stack.cpp
  and math_engine_apply_calibration() both assumed cal_max is always
  numerically bigger than cal_min, which is only true for one rotation
  direction - for a lens calibrated the other way FreeD output was
  already silently broken (cal_max - cal_min going negative into an
  unsigned range, producing garbage normalized values). direction was
  always the right flag to check here, it just wasnt being checked
  consistently everywhere cal_min/cal_max got used. fixed in
  math_engine.cpp (handle_boundary_overshoot, process_encoder_sample,
  math_engine_apply_calibration, math_engine_validate_calibration) and
  network_stack.cpp (all three freed normalization blocks).
- Status LEDs: built, see src/status_leds.cpp. pin 4 genlock/ptp lock, pin 5
  freed streaming active - not two copies of the same signal, each pin is
  its own real status now.
- Reset button: built, see src/reset_button.cpp. pin 6, active low, has to
  be held 5s not tapped so bumping it on set cant wipe a device mid shoot,
  reuses the same SCB_AIRCR reboot handle_reboot() already used.

  unexpected reboot flag: a single byte file (bootflag.bin) written false
  at the start of every boot and true only right before a deliberate
  reset (web ui reboot or the reset button). if a boot finds it still
  false that means the previous session never got to mark itself clean -
  crash or power loss not a normal restart - and the web ui shows a
  banner for it under System until someone clicks Acknowledge. no
  watchdog silently recovers from this on purpose, see roadmap for why,
  this is the visible version of the same idea. acknowledging only
  clears the in memory flag for the current session, a genuinely new
  unexpected reboot re-flags it fresh from the persisted bit regardless
  of whether the last one was ever acknowledged.
- Web UI / fleet discovery: runs in loop(), non blocking. read only fleet
  view is the baseline, per device control if it turns out to be easy.

Full pin map, all interfaces, checked for overlaps against each other -
none found:
    Focus encoder (as5048a)  SPI   sck 13  mosi 11  miso 12  cs 10
    Zoom encoder  (as5048a)  SPI1  sck 27  mosi 26  miso 1   cs 0
    Iris encoder  (as5048b)  I2C   sda 18  scl 19
    Genlock output                bit0 2  bit1 3
    Status LEDs                   genlock/ptp lock 4, freed streaming 5
    Audio (i2s)                   mclk 23  bclk 21  lrclk 20  dac data 7  adc data 8
    Factory reset button          pin 6 hold 5s to confirm
    Ethernet                      native teensy 4.1 rj45, fixed by the board itself
                                  not something we assign, ptp uses it directly
so pin 13 doubles as the teensy onboard led (fine, external leds used
instead) and thats the only overlap on the whole board, everything else
is a clean 1:1 assignment with nothing shared, and nothing sits on the
sdcard/memory pads at all anymore. reset button is a plain gpio to gnd
with INPUT_PULLUP, active low, hold to confirm not a single tap so a
bump on set cant trigger it by accident.

Open items, still not resolved:
- interrupt priority order across genlock/ptp/audio/encoders/ethernet still
  needs setting once all the pieces actually exist - genlock should win.
- genlock fps (24/25/30/60) is a compile time #define, switching for a
  different production means a reflash right now, not a web ui setting.
  true fractional rates (23.976/29.97/59.94) not supported at all yet.
- ltc dropout tolerance on the receiving end (freewheel) not verified
  against any real downstream reader yet, only the theory checks out.
- none of this tested on real hardware yet, pin map and both the spi and
  the new i2c encoder path only verified against source and by compiling
  not on a bench - the isr/i2c interrupt concern specifically was checked
  by reading the actual Wire driver source but the fix (poll iris from
  loop() not the hw timer) hasnt been proven on a real bus yet either.
- bom changed: 2x as5048a + 1x as5048b instead of 3x as5048a, and
  pcm5102a + pcm1808 instead of the pjrc audio shield, not ordered yet.
- multi turn unwrap math verified by compiling and by hand tracing the
  arithmetic, not tested against a real gear pair on a bench yet. the
  unwrapped_angle accumulator is a plain int32 with no overflow guard -
  fine for any realistic single powered-on session, not addressed beyond
  that since a fresh calibration already resets the trusted reference
  point anyway.

See CREDITS.md for what this is built on.
