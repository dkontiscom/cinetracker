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

  two things from the paper behind this library (riggers/schleusner/
  renke/blume, rtcsa 2024, "ptp-synchronized tri-level sync generation
  for networked multi-sensor systems") that are worth holding against
  what we actually do.

  clock servo rate limit - CHECK ON FIRST HARDWARE TEST. the servo is the
  feedback loop nudging the teensy clock into line with the ptp master,
  and because the sync pulse is generated off that same clock, every
  nudge shifts the signal the camera sees. nudge too hard and the camera
  drops genlock. their case study swept a real camera (dreamchip atomone)
  to find its tolerance and landed on 375 ns/s per adjustment at a 10 Hz
  update rate. genlock.cpp currently passes 800 ns/s at 4 Hz into
  ptp_client_init - bigger steps taken less often, wrong on both axes
  relative to their result, and the paper says plainly that the stable
  range gets narrower as the update rate drops (their fig 5 sweeps
  1/4/8/10/12 Hz specifically to show this). not changed blind, because
  their figures are for one particular camera and this wants a real
  camera and a picture to watch. but if genlock turns out flaky, 375 and
  10 is the first thing to try.

  settling time - genlock is not usable immediately at power on, and this
  is normal rather than a fault. the servo has to pull the local clock
  into alignment first, and their fig 7 plots that over a 0-400 second
  axis for the worst case start (half a frame of offset, 30 ppm initial
  drift). big offsets are corrected quickly by the coarse mode, which
  jumps the clock in whole frame steps so the pulse phase is preserved,
  but the final fine alignment inside half a frame is the slow part. so
  expect the lock led on pin 4 to stay dark for a while after boot, and
  dont roll camera until it lights. once settled, the companion paper
  measures 40.7 ns standard deviation against the master clock.

  fps: originally only 30 or 60 compiled (#error for anything else), and
  even after we added 24/25 it was still a single TRISYNC_FPS #define -
  one rate baked into the binary at compile time, changing it meant
  editing that line and reflashing over usb with the teensy loader.

  fixed properly now: lib/t41-tri-sync/src/t41-tri-sync.h was restructured
  from a namespace (one compiled instance) into a template<int FPS> class
  TriSync. every formula and function body is untouched from the original
  single rate version, just moved from #if branches to a template
  parameter, so the actual generated code for e.g. TriSync<25> is the
  same as the old TRISYNC_FPS==25 build would have produced - this was
  specifically chosen over rewriting the timing math to be runtime
  computed, to avoid touching the one subsystem where precision matters
  most. genlock.cpp instantiates TriSync<24/25/30/50/60> all in the same
  firmware and picks which ones pulse() gets attached to IRQ_ENET_TIMER
  based on a runtime fps value, loaded from storage at boot (genlock.bin,
  defaults to GENLOCK_DEFAULT_FPS if nothing saved yet).

  50fps was new this pass, checked its clock rate against the same real
  skyworks AN377 table rather than assumed by proximity to 25 or 60:
  1920x1080p at 50 and 60 both list 148.5MHz (they need double the pixel
  throughput of 24/25/30p at 1080 lines), not 74.25MHz like 24/25/30 -
  grouping 50 with 24/25 by "both sound european" would have been wrong.

  48fps was requested too but left out: checked a real commercial
  tri-level genlock product's supported rate list and it doesnt appear
  at all - where "48" shows up in real broadcast gear its actually
  47.952 (48000/1001), the ntsc-fractional family, not a plain integer
  rate. no legitimate SAMPLES_PER_MS value exists for exact 48.00fps to
  build from without guessing, so its not included.

  web ui: GET/POST /api/genlock, one field (fps). changing it saves to
  storage and reboots the device - not a live hot swap. the ptp client
  (ptp_client.cpp) only ever calls g_ptp.begin() once, on the first link
  up event, guarded by a g_started flag, and FRAMEDURATION/
  DEFAULTJUMPTHRESHOLD get baked into that one call - it was never built
  to be safely reconfigured while running, and this pass didnt add that
  capability either, on purpose. reboot to apply is a real behavior
  change from before but still fully removes the "need a reflash" pain,
  which was the actual complaint.

  still not handled: true fractional rates (23.976, 29.97, 59.94) -
  explicitly not wanted, not just deferred, they were checked and turn
  out to be mainly a US NTSC-legacy production convention still used
  internationally for film/streaming work, not something tied to any
  particular broadcaster today, but the decision to skip them stands
  regardless of that context.

  a real bug was found and fixed in the vendored math itself, upstream's
  own original code not something the template refactor introduced:
  CYCLES_PER_NS in t41-tri-sync.h was `TRISYNC_MHZ / 1000` with
  TRISYNC_MHZ a plain int macro (600) - integer division, evaluates to 0
  not 0.6. only used for the very first delayCycles() call at the top of
  pulse() (the "initial wait for a clean following pulse form"), so it
  was waiting ~0 cycles instead of the apparently intended ~120. fixed by
  changing the divisor to 1000.0 to force floating point division,
  nothing else touched. worth upstreaming to IMS-AS-LUH/t41-tri-sync-ptp,
  see credits.md.

  two more fixes in pulse() itself (2026-10-10), again upstream's own
  original behaviour. one: the trailing edge (+300mV -> 0mV, bits 10 ->
  01) was two digitalWriteFast calls, so the output passed through 00
  (-300mV) for the ~7 instructions between the two stores, which bends
  the edge once the carrier's analog edges are fast (v0.5c, ~35ns).
  begin() now checks both pins sit on the same gpio port (pins 2 and 3
  are GPIO9 bits 4/5) and the trailing edge becomes one store to that
  port's DR_TOGGLE register, two writes kept as the fallback. two: the
  vsync broad pulse was ended at the start of the next interrupt, which
  only lands on 2112T (st0274-2008 10.6 / table 7, bp = 2112T for every
  system) when the line is 2200 samples. at 24/25/50p it ran 440-550T too
  long. those rates now end it inside the same interrupt after 1980T, at
  the cost of ~30us (24/25p) / ~15us (50p) of priority-0 interrupt time
  on the 5 vsync lines per frame. neither checked on a scope yet. both
  worth upstreaming too.
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
  and math_engine.cpp both assumed cal_max is always numerically bigger
  than cal_min, which is only true for one rotation direction - for a
  lens calibrated the other way FreeD output was already silently broken
  (cal_max - cal_min going negative into an unsigned range, producing
  garbage normalized values). direction was always the right flag to
  check here, it just wasnt being checked consistently everywhere
  cal_min/cal_max got used. fixed in math_engine.cpp
  (handle_boundary_overshoot, process_encoder_sample) and
  network_stack.cpp (all three freed normalization blocks).

  full codebase re-audit (2026-09-18, multiple parallel review agents,
  read only pass then fixes only after explicit sign off) found the
  calibration wizard was completely non functional from the web ui - the
  buttons never called calibration/start so the server side session state
  machine never left idle, and separately CAL_STATE_VALIDATE had no code
  path that ever moved it to CAL_STATE_COMPLETE, so even a correctly
  wired wizard would never have let save succeed. root cause was a real
  design mismatch: calibration.cpp still had a full lens preset picker
  (LensProfile, LENS_PRESETS table, calibration_select_preset) that the
  web ui was never actually built against. resolved by dropping the
  preset system entirely (never used, nobody picks a lens from a
  dropdown when the real workflow is two people and a tape measure) and
  keeping the guided step by step wizard, now backed by a real state
  machine that actually works: operator manually toggles which of
  focus/iris/zoom this lens has (not auto detected - a prime has no zoom
  ring to detect, and spi absence detection for focus/zoom isnt reliable
  enough to gate on, only i2c/iris can really tell not-connected from
  connected), each min/mid/max point optionally takes a typed physical
  value (mm for focus/zoom, fstop for iris) purely for a status page
  sanity check display, never sent over freed which only ever gets the
  0-100% normalized position. two segments (min-mid, mid-max) each get
  their own scale factor instead of one averaged across the whole range,
  since focus rings especially arent linear across their throw. focus
  max has no physical value at all, its always infinity by construction,
  the status page shows the infinity symbol instead of a number past
  that point. this also deleted math_engine.cpp's separate, parallel,
  already broken copy of the same mm conversion (EncoderState.mm_min/
  mm_max were hardcoded 0/100 and never actually set from calibration -
  math_engine_apply_calibration(), the only thing that would have set
  them, was dead code nothing called), so /api/status's focus_mm/
  iris_fstop/zoom_mm fields now come from one real implementation
  (calibration_angle_to_phys) instead of two, one of which was silently
  wrong.

  same re-audit found a real stack overflow in handle_fleet() (/api/fleet,
  the peer list json builder): it accumulated snprintf's return value
  against sizeof(body) - len across a loop, but that return value is what
  snprintf would have written if unbounded, not what actually fit - so
  once enough peers pushed it past the buffer the subtraction (size_t -
  int) wrapped to a huge number and the next snprintf wrote a full,
  untruncated chunk starting from an already out of bounds pointer. peer
  names come straight off an unauthenticated lan broadcast (fleet.cpp),
  so this was reachable by anything else on the network, not just a
  theoretical edge case. fixed by formatting each peer into its own small
  fixed buffer first and only copying it into the real one if it actually
  fits, always leaving room for the closing "]}" - no more accumulating
  a length against shrinking remaining space at all.

  also found: fleet.cpp caches camera_id (and the device_name derived
  from it, "cinetracker-<id>") once in fleet_init() at boot, but camera
  id can change live from the web ui with no reboot. changing it kept
  broadcasting the old id to peers and checking fleet_camera_id_conflict()
  against the old one too, so conflict detection silently used a stale id
  until the next reboot. fixed with fleet_set_camera_id(), called from
  handle_freed_set() right alongside network_set_camera_id() so both
  update together, no reboot required, same as everything else that
  isnt genlock.

  last one from this pass: the settings page loaded its initial values
  from /api/config, which never existed server side - it 404d silently
  every load (the fetch never checked response status) so the network
  and freed settings cards always showed hardcoded placeholder defaults,
  never your actual saved config, even right after saving real values.
  handle_network_set (the write side) already accepted and saved ip,
  multicast and port together, it was only the read side missing -
  handle_network_get never returned multicast/port at all. fixed by
  adding those two to handle_network_get (sourced from the same
  NetworkConfig the write side already uses) and pointing the page at
  /api/network + /api/freed, the two endpoints that actually back these
  fields, instead of the one that never existed.

  four smaller leftovers from that same pass, mopped up after: aes67
  status now also exposes jitter_overwrites (was already tracked,
  just never sent out). t41-tri-sync.h's FRAMEDURATION had the same
  int-division truncation class as the CYCLES_PER_NS bug found earlier
  (NS_PER_S / FPS both integers before the double assignment) - fixed
  locally same as before, still not upstreaming it, same call as last
  time. t41-tri-sync-ptp.h's usage-example comment still showed the old
  pre-template trisync:: api and a stale update_interupt_counter typo,
  updated to match the real TriSync<FPS>:: template and the real
  spelling. and the settings page's filter/led toggles: checked the pico
  source, led_enabled was hardcoded true there too, never real in any
  generation, removed. filter_enabled genuinely reflected live state on
  the pico (filter_type > 0) and maps directly onto math_engine's
  already-built ema/median/kalman filtering here, which was already
  defaulting to ema for every encoder but had no way to turn off from
  the web ui in any generation, even the pico's own write side for it
  was a stub that did nothing. built for real this time: /api/filter
  get+post, one shared on/off switch across all 3 encoders (ema or
  none, matching the old semantics, never exposed a per-filter-type
  choice), persisted the same way genlock fps is (filter.bin), applied
  at boot before sampling starts and live via math_engine_set_filter().

  third audit pass (fresh agents, focused on everything touched by the
  last two passes) found one real bug: calibration_start_session()
  memset ctx->cal_data - the live/saved calibration the status page's
  focus_mm/iris_fstop/zoom_mm readouts and calibrated flags come from -
  immediately on pressing Start, not just on save. calibration_abort_
  session() never restored it. so starting a calibration then aborting
  instead of saving permanently wiped the status display until reboot,
  even though the actual encoder state used for freed streaming was
  untouched (that was set from storage once at boot, independent of
  cal_data). fixed by not touching cal_data at all until an actual save
  writes into it - the session only ever needed its own temp_encoders
  during the wizard, no reason to touch the live struct before its
  meant to change. version/timestamp stamping moved from start into the
  save path too, same reasoning.

  same pass also found handle_fleet() dropped peer names straight into
  the json response's "name":"%s" unescaped - fine for a normal name,
  but those names come straight off an unauthenticated lan broadcast
  (fleet.cpp), so a crafted one containing a quote or backslash could
  break or inject fields into the response for anything parsing
  /api/fleet. fixed with a small json_escape() helper (doubles quotes/
  backslashes, flattens control characters to spaces) run on the name
  before its ever formatted in. buffers grown to match the worst case
  of an all-quotes 31 character name doubling in length (entry 100->150,
  body 1400->2000), still comfortably safe.

  last one from this pass: saveFreeDSettings() showed the literal text
  "undefined" as its success toast, since handle_freed_set() only ever
  sent {"success":true} and the js reads data.message. checked the same
  pattern (showToast(data.message...)) at every other call site in
  web_content.cpp - the rest either already have a real message field
  on their handler's response or already guard with a || fallback, this
  was the one real gap. fixed by adding a message field, matching the
  same style handle_network_set already uses.

  two small cleanups from the open questions that pass raised: removed
  CalibrationSession.timeout_ms/start_time/step_start_time - set
  throughout the wizard but nothing ever read them back, dead fields
  suggesting a timeout feature that was never actually built. and
  removed /api/encoders (handle_encoders) entirely - zero frontend
  consumer, and /api/status already sends the same live per-encoder
  angle/mm/fstop data the monitor tab actually uses.

  third cleanup: the roadmap already claimed the fleet web ui was done,
  but there was never actually a frontend for it - /api/fleet had zero
  fetch() calls anywhere. built for real: a Fleet card on the Monitor
  tab, a conflict warning banner and a plain list of discovered peers
  (name/ip/camera id), polled every 3s. peer names come off an
  unauthenticated lan broadcast same as the json escaping fix above, so
  the row list is built with createElement/textContent, never innerHTML
  - a crafted name can't run as html/script in the page, same threat
  model, client side this time instead of server side.

  mopped up the remaining low severity items from that pass: handle_
  fleet()'s per-entry buffer now also checked against its own size not
  just against room left in body, closing the same bug class one layer
  down. resetOvershoot()'s toast and handle_filter_set()'s response now
  actually reflect success/failure instead of always claiming success -
  the filter one specifically distinguishes "applied live" (always
  works, no io) from "persisted" (can fail), since the setting still
  works right now even if it wont survive a reboot. the three settings-
  page init fetches now catch failures with a visible toast instead of
  silently leaving placeholder values with zero indication anything
  didnt load. and 3 real compiler warnings in vendored lib/t41-ptp
  fixed locally, not upstreamed, same call as t41-tri-sync's bugs: an
  unused bool suppressed, a bitwise-and-vs-precedence parenthesization
  that turned out NOT to clear its warning (the compiler wanted the
  comparison itself parenthesized, and the build check that "confirmed"
  it was defeated by platformio deciding what to recompile by content
  hash not timestamp, so the touched file never rebuilt - caught by the
  fourth audit and fixed then, as && which is what was meant all along,
  bit-identical for two bools but short-circuiting and warning free), and one real
  logic bug (slave && messageType==3 || messageType==9 parsing as
  (slave && messageType==3) || messageType==9, meaning it'd also match
  messageType==9 regardless of role) fixed to the clearly intended
  slave && (messageType==3 || messageType==9) - inert for this project
  today since ptp only ever runs in slave mode here, but a real bug for
  master mode.

  fourth audit pass (2026-09-19, five fresh agents, first deep look at
  the protocol and physics layer rather than module wiring) found that
  four core features had never actually worked. first one fixed: the
  freed packet's focus/zoom normalization in network_stack.cpp did
  offset * 0xFFFFFF in int32, which overflows for any offset past 127
  encoder counts - a lens throw is thousands, so every value past about
  a degree of travel wrapped negative on the wire, the device's core
  output was garbage from the first packet. and right next to it,
  freed_encode_24bit clamped to the signed 24 bit range (8388607) while
  the normalization targets the unsigned one (16777215, the constant
  the code itself defines), so even with the overflow gone the top half
  of the throw would have sat flat. both fixed in one place: a single
  freed_normalize() helper (64 bit multiply, clamps to the window so a
  position outside it gives an end stop not a wrapped value) replacing
  the three copy-pasted blocks, and the encoder made unsigned - freed
  zoom/focus are unsigned raw encoder values, and this encoder only ever
  serves those two fields since a lens only tracker never populates
  pan/tilt/xyz.

  roadmap item 10 done alongside it: send_sap_announce() now only runs
  when direction is LTC_IN (the transmit path). the 30s timer sits
  outside that gate on purpose so a device flipped to transmit announces
  at once rather than up to 30s later. no sap deletion packet is sent
  when flipping the other way (rfc 2974 has one) - receivers just age
  the session out, same as before. the announcement body itself still
  has the other problems the fourth audit found (double nul before the
  sdp, millis() as session id, missing a=ts-refclk/a=mediaclk), those
  are separate and still open.

  second of the four: a calibration saved from the web ui never reached
  the stream until reboot. apply_calibration() - the thing that pushes
  cal_min/cal_max/range into the live EncoderState freed reads from -
  was only ever called from main.cpp at boot. save wrote flash and the
  status page's cal_data, and the wire kept the old range (or the
  0..16383 default on a first ever calibration) until a power cycle. it
  was the flip side of what made the abort fix in the third pass safe:
  the live state being independent of cal_data cut both ways. fixed
  with one calibration_apply_all() that both boot and save go through so
  they cant drift - a ring not calibrated in the saved data goes
  disabled (freed streams 0 for it), which is exactly the state a
  reboot would have produced and what a lens with no iris or zoom ring
  is meant to do. two things had to move with it: calibration_end_
  session now refuses a save when the wizard isnt complete (it used to
  return ok, save nothing, and quietly drop the session to idle - with a
  live apply hanging off "ok" that would have applied stale data), and
  apply_calibration clears the overshoot flag so a fresh window starts
  clean.

  third of the four: ptp never started if the cable was plugged in at
  boot. ptp_client_init registers qnethernet's onLinkState callback to
  call g_ptp.begin() on link up, but that callback is change-only (the
  library just stores the pointer, checked in QNEthernet.h, no replay of
  the current state) and network_stack_init runs first and blocks in a
  10s dhcp wait that services the link - so the up edge fired during
  that wait, before anyone was listening, g_started stayed false, and
  genlock never locked. only an unplug/replug rescued it, and static ip
  was a race not a fix. now the begin logic is a named function that's
  both registered as the callback and called once by hand right after
  with Ethernet.linkState(), so an edge that already went by is caught.
  a link that drops and comes back after ptp is running still doesnt
  re-begin (begin is once per boot by design, same reason genlock fps
  needs a reboot) - what genlock_locked() reports across a link loss is
  a separate open item from the same audit.

  fourth of the four: aes67 receive could never hear a multicast stream.
  the rtp socket was bound with a plain begin(port), a unicast bind -
  lwip has igmp on and drops multicast for any group the interface hasnt
  joined (and the mac hash filter rejects it before lwip even sees it),
  so LTC_OUT mode received nothing, ever, at the default 239.x
  destination. now bind_rtp_socket() uses beginMulticast() (which does
  the joinGroup) when the destination is a multicast address and a plain
  bind otherwise, so a unicast destination still works. the join is by
  destination type not direction: direction can flip at runtime, and a
  socket sitting in a group costs nothing in transmit mode, so theres
  one bind path instead of two. one subtlety checked in the library
  source: stop() only removes the pcb, the group membership lives on the
  netif and is ref counted, so changing destination now explicitly
  leaveGroup()s the old address first - and an ip only change (same
  port) now rebinds too, it used to skip that, which was fine for
  unicast but is exactly the wrong thing when the group itself is what
  changed. still open from the same audit and adjacent to this: the
  socket's receive queue depth is 1, so any loop iteration over 1ms
  drops packets before the jitter buffer ever sees them.

  the sap/sdp announcement body, three problems fixed together. the
  payload-type field was written as "application/sdp" plus its nul (16
  bytes, correct) and then a second "\0", so the sdp itself started
  with a 0x00 byte instead of v=0 - any real parser rejects that. the
  o= line used millis() for both sess-id and sess-version, so every 30s
  announce was a brand new session to a receiver and stale ones
  lingered up to an hour: now sess-id is the mac as a 48 bit number
  (unique per device, stable across boots) and sess-version only moves
  when the description actually changes (destination, or the
  grandmaster appearing or changing), with the rfc 2974 msg-id-hash in
  the sap header filled in so receivers can tell a repeat from an
  update without parsing. and aes67 requires a=ts-refclk and
  a=mediaclk (rfc 7273), which were missing - dante controller may hide
  a stream without them. that needed the grandmaster identity, which
  the vendored ptp library never exposed: it doesnt parse Announce on
  the slave side at all (the servo only needs Sync/FollowUp/DelayResp),
  so a small local addition captures grandmasterIdentity (bytes 53-60)
  and domain from Announce and a getter hands them out. Announce
  specifically, not the Sync's sourcePortIdentity - those name the same
  clock when the grandmaster is on the lan, but differ behind a
  boundary clock, and refclk wants the grandmaster. until a master has
  been seen the attribute is left out rather than claiming "traceable"
  for a clock we know nothing about, and the version bumps when it
  appears. a=recvonly added too, matching what dante's own sdp carries.
  the buffer grew 256->320 for the extra lines.

  two more from the same pass. genlock_locked() reported locked forever
  once syncs stopped: the lib's lockcount only moves when a sync
  exchange arrives, so a pulled cable or a vanished master left it
  frozen at whatever it was - lock led on, status page saying synced,
  nothing true about either. the lib has its own ms-since-last-update
  timer it never exposed (it uses >1100ms as stale internally), one
  getter added, and ptp_client_locked() now also wants link up and an
  update inside PTP_STALE_MS (5s, clear of a 2s sync interval). cable
  pull unlocks instantly via the link callback, a master gone with the
  link still up unlocks within seconds. and the rtp receive socket had
  qnethernet's default queue depth of 1 packet, with process_rx pulling
  one per loop() - so any iteration longer than 1ms (web request, flash
  write) dropped everything but the newest at the socket, before the
  jitter buffer ever saw it, and a backlog could never catch up. queue
  is 8 deep now and process_rx drains it each loop. the jitter buffer
  itself stays 4 slots (4ms of playout latency) on purpose - deepening
  it would ride out longer stalls at the cost of latency, a separate
  knob nobody has asked to turn.

  data path pair from the same pass. the freed frame scheduler only
  advanced next_frame_time while streaming, so after 36-72 minutes with
  streaming off the signed micros() diff read the stale value as "not
  yet" and nothing went out for up to another 36 minutes while the ui
  said streaming. now network_process_frame resyncs to now whenever it
  hasnt been called for over two frame intervals (streaming was off, or
  a hard stall) - one mechanism, no hook into the web handler needed,
  and a resume no longer counts as a missed frame either. and the multi
  turn unwrap moved out of math_process_angle into encoder_driver: it
  ran at loop rate on whatever the isr had last latched, so a loop stall
  longer than half a shaft turn (the 10s network reconfigure did it
  easily) aliased a whole revolution, and handle_boundary_overshoot then
  quietly dragged the calibration window along to match - a permanent
  offset until recalibration, with nothing visibly wrong. the isr now
  accumulates the unwrapped int32 itself at 1khz (same deadband, same
  angle_distance, just where every sample is actually seen) and the loop
  reads it alongside the raw value. iris is the honest exception: its
  polled over i2c from the loop, so loop rate is its sample rate and a
  stall still costs it, thats inherent to i2c-from-the-loop not
  something the move can fix.

  storage, from the same pass. two real holes: teensy littlefs opens for
  write with no truncate (checked in LittleFS.h, FILE_WRITE_BEGIN is
  RDWR|CREAT), so a file could only ever grow and a struct that shrank
  left a stale tail that read back as valid data. and nothing checked
  size or layout on read, so a file written by a previous struct layout
  was read straight into the new one as garbage with no error - the
  struct shapes changed twice in two days, this was a real footgun even
  with nothing on hardware yet. now every file is a small header (magic,
  layout version, payload size) then the payload, the file is removed
  before every write so it cant carry a tail, and the payload is read
  into scratch and only copied to the caller on a full match - a
  struct never gets a partial fill (calibration could previously come
  back with valid=1 and garbage from a short read). per-file layout
  versions start at 1, bump the one whose struct changed, and
  static_asserts keep every struct inside the scratch buffer. a missing
  file (first boot, after a factory reset) stays quiet, an existing file
  that doesnt match logs. filter.bin now goes through a byte like
  bootflag already did, reading flash straight into a bool is undefined
  for anything but 0 and 1. and main.cpp never looked at network_stack_
  init's return: a saved config failing validation (frame_rate 0, dest
  0 - the web ui could save either from an empty field) left ethernet
  down for good with no way in but the reset button. now validated
  before init and replaced with defaults in ram only if rejected -
  validate-first rather than retry-on-failure so a missing cable doesnt
  pay the 10s dhcp wait twice. the bad file is left in place, the next
  save from the web ui replaces it.

  web server, from the same pass. the server has 4 client slots and the
  wait-for-body branch of process_slot returned without ever looking at
  the connection, so a post whose declared content-length never fully
  arrived (an aborted upload, a browser killed mid request, a bogus or
  negative length - which wrapped through size_t into a count nothing
  could satisfy) pinned its slot for good. four of those and every new
  connection was refused until reboot. and there was no idle timeout at
  all, so a client that just went quiet (phone walked out of wifi range,
  no close ever sent, connected() true forever) did the same. fixed
  three ways: the body wait now drops a disconnected client, the
  declared length is clamped to the buffer so the wait is always for a
  finite byte count, and any slot silent for over WEB_CLIENT_TIMEOUT_MS
  (5s, nothing legitimate pauses that long inside one request) is
  dropped. the read return is also checked before being added to the
  buffer length (it was added unsigned as-is - qnethernet never returns
  negative there, but it was one library change away from a wrap).
  content-length is now only looked for inside the headers and at a line
  start, the old scan ran over the whole buffer and matched mid line, so
  a body containing the text could set it.

  the as5048a error flag, from the same pass, checked against the real
  datasheet before touching it (DS000298 v1-11, fetched to the scratchpad
  and text-extracted, no poppler here). page 19 verbatim: "if the error
  flag is set to high because of a communication problem the flag remains
  set until it will be cleared by the CLEAR ERROR FLAG command". latched,
  in other words. the isr discarded every frame with EF set and nothing
  ever issued the clear, so one glitch - a connector wiggle, a parity hit,
  the encoder powering up a moment after the first isr frame - meant
  every frame after it carried EF, got discarded, and that ring froze
  silently for good (encoder_read_angle failed forever too, so
  calibration detection couldnt recover it either). the clear is itself
  a read of register 0x0001: per the datasheet's sequence its response
  carries the error register content, and the read after that is clean.
  now the isr issues it on an EF frame, rate limited to once a second
  while errors persist so an unplugged encoder (floating miso reads as EF
  every tick) doesnt cost a clear every millisecond forever, and marks
  the ring clean again on the next good frame. encoder_read_angle does
  the same so detection self heals, and init clears both chips once for
  whatever they latched during power up. error and recovery transitions
  are printed from the main loop (never from the isr) with the error
  register value and a running count - visible not silent, but no
  diagnostics ui, that was explicitly not wanted.

  genlock interrupt priority: IRQ_ENET_TIMER was left at the teensy
  default of 128, same as i2s dma, ethernet rx and the encoder sampling
  timer, so the sync pulse edge could be delayed by any of their
  handlers. now NVIC_SET_PRIORITY 0, the top - the pulse edge is the one
  thing here that must not wait for anyone. ptp deliberately stays
  below it: qnethernet never sets a priority on IRQ_ENET (checked in
  lwip_t41.c, it just attaches the vector) and doesnt need to, the
  timestamps ptp lives on are captured in hardware by the mac, rx
  interrupt latency doesnt move them. noInterrupts() still masks
  priority 0, teensy uses primask not basepri, so critical sections are
  unaffected.

  last vendored ptp fix from the pass: after sending a delay request
  (and, in master mode, a sync) the library spun in a while loop waiting
  for the hardware tx timestamp of that frame with no exit whatsoever.
  if the link dropped in the instant between deciding to send and the
  frame leaving the mac, the frame never entered the tx ring, the
  timestamp never came, and the whole device hung - and with no watchdog
  (by design, see roadmap) that was a power cycle. "visible failure" needs
  the device still running to be visible at all. now a bounded wait, 5ms
  (the hardware timestamp normally lands within microseconds, so this only
  ever trips on a genuinely lost frame), that logs and skips the round on
  timeout, the next exchange simply retries. both sites go through one
  helper so the timeout logic exists once. only the delay request site
  runs in this projects slave only use, the sync one is fixed identically
  for master mode.

  and the low severity tail of the fourth audit, all in one go. the
  audio pool was allocated without the settings so its blocks defaulted
  to 128 samples and the i2s output printed a mismatch warning per block
  from the audio interrupt, a thousand times a second, in receive mode.
  the rtp loss counter let a late packet rewind expected_seq, so the next
  in-order one read as a second loss for the same gap. four handlers
  reported success when the flash save failed - network, freed, aes67,
  and genlock, which even rebooted on a failed save and came back on the
  old rate having said "rebooting to apply": they now say plainly that
  the change is applied but wont survive a reboot, and genlock doesnt
  reboot at all if the fps didnt save. an empty camera id / port field
  saved 0 (parseInt('') is NaN, serialises as null, atoi("null") is 0) -
  empty number fields are now simply not sent so the device keeps its
  value, and port 0 is rejected server side either way. two dead
  functions gone (calibration_get_progress, ptp_client_offset_ns),
  encoder_stop_sampling deliberately kept: the web firmware update will
  need to halt the sampling isr before it writes flash. the never-written
  calibration_time field dropped from EncoderCalibration, which is the
  first real use of the file versioning - CALIBRATION_LAYOUT is 2 now, an
  old calibration.bin is ignored with a log line instead of being read
  through the wrong layout. and two stale bullets removed from the open
  items below, both long since done: interrupt priority (genlock wins,
  set this pass) and genlock fps being a compile time #define (a web ui
  setting since the third day).
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
- Settings persistence: network config (dhcp/ip/gateway/subnet/dest ip/
  dest port) and camera id (doubles as the device id fleet conflict
  detection uses) now survive a reboot, saved to network.bin as one
  NetworkConfig blob whenever handle_network_set or handle_freed_set
  actually changes something. found and fixed a real bug while wiring
  this up: handle_network_set started from network_get_default_config()
  not the live config, so changing just one field (say the port) would
  silently reset every other field to its hardcoded default instead of
  leaving it alone. now starts from network_get_current_config() (new
  getter, just returns the live g_config) so partial updates behave
  correctly. FreeD frame_rate now also triggers the same NetworkConfig
  save since its part of that same struct. AES67 direction + destination
  survive a reboot too, separate small Aes67Settings struct saved to
  aes67.bin whenever handle_aes67_set runs - stream_name isnt user
  settable so it stays a main.cpp default not something we persist.
  device display name explicitly not wanted, see roadmap.

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
- genlock clock servo rate limit: genlock.cpp passes 800 ns/s at 4 Hz,
  the source paper measured 375 ns/s at 10 Hz against a real camera and
  showed the stable range narrows at lower update rates. if a camera
  drops genlock intermittently while our ui still says synced, this is
  the first thing to change. see the genlock section above.
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
