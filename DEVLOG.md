# Development log

The detailed history behind the firmware: design decisions, the reasoning
for them, and the bugs found and fixed while building each subsystem. It is
kept separate from `ARCHITECTURE.md` on purpose — `ARCHITECTURE.md` is the
current picture of how the firmware works, this file is how it got there and
why, so the reasoning isn't lost.

Grouped by subsystem. None of this has run on real hardware yet; entries are
from writing and compiling the firmware and from reading the relevant
datasheets and library sources.

## PTP / Ethernet

PTP client is `t41-ptp` (IMS-AS-LUH), one shared client feeding both genlock
and AES67 timing, not two separate clients. This was checked by cloning both
`t41-ptp` and `AES67Teensy` and diffing the actual source — `AES67Teensy`'s
vendored copy still has unresolved git merge conflict markers committed
straight into `ptp-base.h/cpp`, `l3ptp.h` and `t41-ptp.h`, so it wouldn't
even compile as-is and isn't usable.

A second problem surfaced only once the genlock code was written and failed
to compile: the standalone `IMS-AS-LUH/t41-ptp` repo (what was first vendored
via `platformio.ini`) is stale compared to the copy bundled inside
`t41-tri-sync-ptp` itself. The bundled one has an extended `PTPBase::begin()`
(frameDuration, jumpThreshold, rate limit, update rate, two callbacks,
skipAtStart) specifically so PTP clock corrections don't land in the middle
of a genlock pulse and glitch the signal — the standalone repo's plain
`begin()` can't do this at all. So the project now vendors the newer bundled
copy directly into its own tree at `lib/t41-ptp` (and `lib/t41-tri-sync` for
the pulse generator), commit `4a121127f63f826a1db01534c0b7c4614c448026`,
instead of depending on the standalone repo through `platformio.ini` (that
line was removed). MIT license confirmed on both, checked directly in the
repo.

Needs a specific QNEthernet fork per `t41-ptp`'s own readme:
`HedgeHawk/QNEthernet` branch `ieee1588-2-fix`. `platformio.ini` was switched
to it and everything rebuilt — `network_stack.cpp`, `fleet.cpp` and
`web_interface.cpp` all compiled unchanged against it. One real bug found
doing this: the HedgeHawk fork itself is missing `#include <math.h>` in
`lwip_t41.c` (`round()` used without it, a hard compile error not a warning).
Fixed with `-include math.h` in `build_flags` rather than patching the
vendored copy directly, since `.pio/libdeps` gets freshly redownloaded on
every machine and a local patch wouldn't survive that.

## Genlock

A real SMPTE ST 0274-2008 tri-level sync generator. Pins are not hardcoded in
`t41-tri-sync-ptp` — `begin()` takes the two sync pins as parameters, driven
off the Ethernet PTP hardware timer through `digitalWriteFast()`, not a
dedicated PWM peripheral.

**Pin choice (update 2026-10-10):** pins 2 and 3 now matter. The planned
rework moves the edges onto a hardware timer, and pins 2 and 3 happen to be
the A/B outputs of the same FlexPWM block (FlexPWM4 submodule 2), so they
stay. Don't move them without checking `GENLOCK-TIMING.md` first.

Two things from the paper behind this library (Riggers, Schleusner, Renke,
Blume, RTCSA 2024, "PTP-Synchronized Tri-Level Sync Generation for Networked
Multi-Sensor Systems") are worth holding against what the firmware actually
does:

**Clock servo rate limit — check on first hardware test.** The servo is the
feedback loop nudging the Teensy clock into line with the PTP master, and
because the sync pulse is generated off that same clock, every nudge shifts
the signal the camera sees. Nudge too hard and the camera drops genlock.
Their case study swept a real camera (Dreamchip AtomOne) to find its
tolerance and landed on 375 ns/s per adjustment at a 10 Hz update rate.
`genlock.cpp` currently passes 800 ns/s at 4 Hz into `ptp_client_init` —
bigger steps taken less often, wrong on both axes relative to their result,
and the paper says plainly that the stable range gets narrower as the update
rate drops (their fig 5 sweeps 1/4/8/10/12 Hz specifically to show this). Not
changed blind, because their figures are for one particular camera and this
wants a real camera and a picture to watch. But if genlock turns out flaky,
375 and 10 is the first thing to try.

**Settling time.** Genlock is not usable immediately at power-on, and this is
normal rather than a fault. The servo has to pull the local clock into
alignment first, and their fig 7 plots that over a 0–400 second axis for the
worst-case start (half a frame of offset, 30 ppm initial drift). Big offsets
are corrected quickly by the coarse mode, which jumps the clock in whole
frame steps so the pulse phase is preserved, but the final fine alignment
inside half a frame is the slow part. So expect the lock LED on pin 4 to stay
dark for a while after boot, and don't roll camera until it lights. Once
settled, the companion paper measures 40.7 ns standard deviation against the
master clock.

**Frame rates (template refactor).** Originally only 30 or 60 compiled
(`#error` for anything else), and even after 24/25 were added it was still a
single `TRISYNC_FPS` `#define` — one rate baked into the binary at compile
time, changing it meant editing that line and reflashing over USB with the
Teensy Loader.

Fixed properly: `lib/t41-tri-sync/src/t41-tri-sync.h` was restructured from a
namespace (one compiled instance) into a `template<int FPS> class TriSync`.
Every formula and function body is untouched from the original single-rate
version, just moved from `#if` branches to a template parameter, so the
actual generated code for e.g. `TriSync<25>` is the same as the old
`TRISYNC_FPS==25` build would have produced — this was specifically chosen
over rewriting the timing math to be runtime-computed, to avoid touching the
one subsystem where precision matters most. `genlock.cpp` instantiates
`TriSync<24/25/30/50/60>` all in the same firmware and picks which ones
`pulse()` gets attached to `IRQ_ENET_TIMER` based on a runtime fps value,
loaded from storage at boot (`genlock.bin`, defaults to `GENLOCK_DEFAULT_FPS`
if nothing saved yet).

50 fps was new in this pass, checked its clock rate against the real Skyworks
AN377 table rather than assumed by proximity to 25 or 60: 1920×1080p at 50
and 60 both list 148.5 MHz (they need double the pixel throughput of 24/25/30p
at 1080 lines), not 74.25 MHz like 24/25/30 — grouping 50 with 24/25 by "both
sound European" would have been wrong.

48 fps was requested too but left out: checked a real commercial tri-level
genlock product's supported rate list and it doesn't appear at all — where
"48" shows up in real broadcast gear it's actually 47.952 (48000/1001), the
NTSC-fractional family, not a plain integer rate. No legitimate
`SAMPLES_PER_MS` value exists for exact 48.00 fps to build from without
guessing, so it's not included.

Web UI: GET/POST `/api/genlock`, one field (fps). Changing it saves to
storage and reboots the device — not a live hot-swap. The PTP client
(`ptp_client.cpp`) only ever calls `g_ptp.begin()` once, on the first link-up
event, guarded by a `g_started` flag, and `FRAMEDURATION`/
`DEFAULTJUMPTHRESHOLD` get baked into that one call — it was never built to be
safely reconfigured while running, and this pass didn't add that capability
either, on purpose. Reboot to apply is a real behavior change from before but
still fully removes the "need a reflash" pain, which was the actual complaint.

Still not handled: true fractional rates (23.976, 29.97, 59.94) — explicitly
not wanted, not just deferred. They were checked and turn out to be mainly a
US NTSC-legacy production convention still used internationally for
film/streaming work, not something tied to any particular broadcaster today,
but the decision to skip them stands regardless of that context.

**`CYCLES_PER_NS` bug (vendored math).** A real bug in the vendored math
itself, upstream's own original code not something the template refactor
introduced: `CYCLES_PER_NS` in `t41-tri-sync.h` was `TRISYNC_MHZ / 1000` with
`TRISYNC_MHZ` a plain int macro (600) — integer division, evaluates to 0 not
0.6. Only used for the very first `delayCycles()` call at the top of
`pulse()` (the "initial wait for a clean following pulse form"), so it was
waiting ~0 cycles instead of the apparently intended ~120. Fixed by changing
the divisor to `1000.0` to force floating-point division, nothing else
touched. Worth upstreaming to `IMS-AS-LUH/t41-tri-sync-ptp` (see CREDITS.md).

**Trailing edge and broad pulse (2026-10-10),** again upstream's own original
behaviour. One: the trailing edge (+300 mV → 0 mV, bits 10 → 01) was two
`digitalWriteFast` calls, so the output passed through 00 (−300 mV) for the
~7 instructions between the two stores, which bends the edge once the
carrier's analog edges are fast (v0.5c, ~35 ns). `begin()` now checks both
pins sit on the same GPIO port (pins 2 and 3 are GPIO9 bits 4/5) and the
trailing edge becomes one store to that port's `DR_TOGGLE` register, two
writes kept as the fallback. Two: the vsync broad pulse was ended at the
start of the next interrupt, which only lands on 2112T (ST 0274-2008 10.6 /
table 7, bp = 2112T for every system) when the line is 2200 samples. At
24/25/50p it ran 440–550T too long. Those rates now end it inside the same
interrupt after 1980T, at the cost of ~30 µs (24/25p) / ~15 µs (50p) of
priority-0 interrupt time on the 5 vsync lines per frame. Neither checked on
a scope yet. Both worth upstreaming too.

**Interrupt priority.** `IRQ_ENET_TIMER` was left at the Teensy default of
128, same as I2S DMA, Ethernet rx and the encoder sampling timer, so the sync
pulse edge could be delayed by any of their handlers. Now `NVIC_SET_PRIORITY`
0, the top — the pulse edge is the one thing here that must not wait for
anyone. PTP deliberately stays below it: QNEthernet never sets a priority on
`IRQ_ENET` (checked in `lwip_t41.c`, it just attaches the vector) and doesn't
need to, the timestamps PTP lives on are captured in hardware by the MAC, rx
interrupt latency doesn't move them. `noInterrupts()` still masks priority 0,
Teensy uses primask not basepri, so critical sections are unaffected.

**`FRAMEDURATION` int-division.** `t41-tri-sync.h`'s `FRAMEDURATION` had the
same int-division truncation class as the `CYCLES_PER_NS` bug (`NS_PER_S /
FPS` both integers before the double assignment) — fixed locally the same way,
still not upstreamed.

**Lock reporting across a link loss.** `genlock_locked()` reported locked
forever once syncs stopped: the lib's lockcount only moves when a sync
exchange arrives, so a pulled cable or a vanished master left it frozen at
whatever it was — lock LED on, status page saying synced, nothing true about
either. The lib has its own ms-since-last-update timer it never exposed (it
uses >1100 ms as stale internally); one getter was added, and
`ptp_client_locked()` now also wants link-up and an update inside
`PTP_STALE_MS` (5 s, clear of a 2 s sync interval). Cable pull unlocks
instantly via the link callback, a master gone with the link still up unlocks
within seconds.

## AES67 / LTC

Mono only (it's carrying LTC not program audio), only one direction active at
a time — `AES67_LTC_IN` (local audio in → RTP out to network) or
`AES67_LTC_OUT` (RTP in from network → local audio out), never both, gated in
`aes67_process()` by whatever `aes67_set_direction()` was last called with.
Direction and destination ip/port are real web UI settings, GET/POST
`/api/aes67` in `web_interface.cpp`, card in the settings tab in
`web_content.cpp`. Boots at `AES67_OFF` with the `main.cpp` default dest until
changed.

**Audio hardware.** Dropped the PJRC audio shield (SGTL5000 chip). It forced
16-bit unless you went into its internals to override it, and it has its own
onboard SD card slot wired to the exact same pins (10/11/12/13) as the Focus
encoder's SPI bus — a real conflict, not a hypothetical one, found by checking
the real PJRC pin table.

Using a PCM5102A (DAC, LTC out) + PCM1808 (ADC, LTC in) pair instead. Both
are bare chips with no I2C and no control registers at all — bit depth/format
is set by tying a couple of pins to GND/3V3 on the board itself, so 24-bit is
just how the chip behaves, nothing to enable or override in software. Neither
board has an SD card or anything else on it. Confirmed real Teensy wiring for
this pair from someone who's actually built it: bck 21, lrclk 20, mclk 23,
DAC data 7, ADC data 8 — same I2S pin group the SGTL5000 shield used, still
doesn't touch 10/11/12/13, so the Focus encoder pin conflict is gone by
hardware choice not by moving the encoder off its pins.

**24-bit.** Using `OpenAudio_ArduinoLibrary` (`AudioInputI2S_F32`/
`AudioOutputI2S_F32`/`AudioRecordQueue_F32`/`AudioPlayQueue_F32`), a real
dependency in `platformio.ini`, MIT confirmed in the source file headers.
These classes just drive the raw I2S peripheral, they don't know or care what
chip is on the other end, so no codec control class is needed at all with
this pair — `aes67_init()` does `AudioMemory_F32()` and starts the record
queue, that's it.

**RTP.** Plain L24 mono over RTP (RFC 3190 style), 48 samples/packet at 48 kHz
(1 ms packet time, matches the AES67Teensy reference), dynamic payload type 98
— not IANA-fixed for L24, just needs to match what's advertised. Announces
itself via SAP (RFC 2974, the real well-known 239.255.255.255:9875 multicast
address, matches AES67Teensy) every 30 s with a real SDP description, not a
stub.

**Jitter buffer on rx.** 4-packet ring, primes on the first 2 packets then
plays out on its own `micros()` pace independent of when packets actually
arrive. This is what keeps the output steady — without it, playout timing
would follow network arrival jitter directly, which is exactly what you can't
have on a sync signal. The spec calls holding a small buffer "link offset"
and expects a receiver to do this; it's not in tension with AES67's latency
numbers since those numbers already assume a buffer is there. On underrun
(packet actually missing) it plays silence for that one 1 ms slot instead of
stalling. Professional LTC readers implement "freewheel" for exactly this
case — hold the last valid time through a brief gap rather than lose lock —
but how many ms a specific downstream reader will actually freewheel through
is not verified yet, depends on whatever ends up reading this on the
Unreal/LiveLink side. Open item, not assumed fine.

**I2S/I2C pins.** No I2C on this board pair at all (see above). I2S pins are
fixed by the i.MX RT SAI1 peripheral itself, checked by reading
`output_i2s_f32.cpp` directly, nothing chosen or possible to get wrong.

**Multicast receive bind.** AES67 receive could never hear a multicast
stream. The RTP socket was bound with a plain `begin(port)`, a unicast bind —
lwIP has IGMP on and drops multicast for any group the interface hasn't
joined (and the MAC hash filter rejects it before lwIP even sees it), so
`LTC_OUT` mode received nothing, ever, at the default 239.x destination. Now
`bind_rtp_socket()` uses `beginMulticast()` (which does the `joinGroup`) when
the destination is a multicast address and a plain bind otherwise, so a
unicast destination still works. The join is by destination type not
direction: direction can flip at runtime, and a socket sitting in a group
costs nothing in transmit mode, so there's one bind path instead of two. One
subtlety checked in the library source: `stop()` only removes the pcb, the
group membership lives on the netif and is ref-counted, so changing
destination now explicitly `leaveGroup()`s the old address first — and an
ip-only change (same port) now rebinds too, it used to skip that, which was
fine for unicast but is exactly the wrong thing when the group itself is what
changed.

**SAP/SDP announcement body.** Three problems fixed together. The payload-type
field was written as "application/sdp" plus its nul (16 bytes, correct) and
then a second "\0", so the SDP itself started with a 0x00 byte instead of
`v=0` — any real parser rejects that. The `o=` line used `millis()` for both
sess-id and sess-version, so every 30 s announce was a brand-new session to a
receiver and stale ones lingered up to an hour: now sess-id is the MAC as a
48-bit number (unique per device, stable across boots) and sess-version only
moves when the description actually changes (destination, or the grandmaster
appearing or changing), with the RFC 2974 msg-id-hash in the SAP header
filled in so receivers can tell a repeat from an update without parsing. And
AES67 requires `a=ts-refclk` and `a=mediaclk` (RFC 7273), which were missing —
Dante Controller may hide a stream without them. That needed the grandmaster
identity, which the vendored PTP library never exposed: it doesn't parse
Announce on the slave side at all (the servo only needs
Sync/FollowUp/DelayResp), so a small local addition captures
`grandmasterIdentity` (bytes 53–60) and domain from Announce and a getter
hands them out. Announce specifically, not the Sync's `sourcePortIdentity` —
those name the same clock when the grandmaster is on the LAN, but differ
behind a boundary clock, and refclk wants the grandmaster. Until a master has
been seen the attribute is left out rather than claiming "traceable" for a
clock we know nothing about, and the version bumps when it appears.
`a=recvonly` added too, matching what Dante's own SDP carries.

**SAP gated to transmit.** `send_sap_announce()` now only runs when direction
is `LTC_IN` (the transmit path). The 30 s timer sits outside that gate on
purpose so a device flipped to transmit announces at once rather than up to
30 s later. No SAP deletion packet is sent when flipping the other way (RFC
2974 has one) — receivers just age the session out.

**Receive queue depth.** The RTP receive socket had QNEthernet's default queue
depth of 1 packet, with `process_rx` pulling one per `loop()` — so any
iteration longer than 1 ms (web request, flash write) dropped everything but
the newest at the socket, before the jitter buffer ever saw it, and a backlog
could never catch up. Queue is 8 deep now and `process_rx` drains it each
loop. The jitter buffer itself stays 4 slots (4 ms of playout latency) on
purpose — deepening it would ride out longer stalls at the cost of latency, a
separate knob nobody has asked to turn.

**Low-severity tail.** The audio pool was allocated without the settings so
its blocks defaulted to 128 samples and the I2S output printed a mismatch
warning per block from the audio interrupt, a thousand times a second, in
receive mode — fixed. The RTP loss counter let a late packet rewind
`expected_seq`, so the next in-order one read as a second loss for the same
gap — fixed. AES67 status now also exposes `jitter_overwrites` (was already
tracked, just never sent out).

## FIZ encoders and calibration

**Bus layout.** Originally planned as 3× AS5048A each on its own SPI bus
(focus/iris/zoom on SPI/SPI1/SPI2). SPI2 turned out to be unusable — its only
two pin options on the Teensy 4.1 are the same physical pads as the onboard
SD card and the PSRAM/flash memory footprint, confirmed against the real
Teensy `SPI.h` source (comment: "T4.1 has SPI2 pins on memory connectors as
well as SDCard") and independently against PJRC's own forum thread "SPI2 Pins
on Teensy 4.1?". Those pads are also reported as physically fragile (~1 mm
pitch, pads known to pull off the board if the wire moves), so not something
to build a camera-mounted device around even setting the SD card sharing
aside.

Fix: zoom stays an AS5048A but moves onto the SPI1 bus (distinguished by its
own CS pin). Iris switches to an AS5048B instead, same sensor family, I2C
interface instead of SPI, confirmed identical 14-bit resolution to the SPI
part from the real ams datasheet, so no precision lost by switching that one
axis. AS5048B register addresses (0xFE/0xFF for angle, split high byte + low
6 bits) checked against the real datasheet and independently against the
`sosandroid/AMS_AS5048B` Arduino library, both agree.

**Iris off the hardware interrupt.** Iris is deliberately not read inside the
same hardware timer interrupt as focus/zoom. Real reported failure on the
PJRC forum: `IntervalTimer` interrupting an in-progress `Wire` transfer wedges
the I2C bus (SCL stuck high, SDA stuck low). The actual Teensy 4.x `Wire`
source was checked and confirmed its blocking calls poll hardware registers
directly rather than waiting on their own interrupt to fire, so it won't
deadlock, but the community consensus (multiple independent threads) is still
to keep I2C out of a hardware ISR entirely rather than rely on that. Iris gets
its own timer in `encoder_process()`, called from `loop()` same as
`aes67_process()` and friends, not from the interrupt.

Sampled at 1 kHz, focus/zoom via `IntervalTimer`, iris via `elapsedMicros` in
the main loop. Pin 13 is also the Teensy's built-in LED (classic Arduino
SPI/LED pin overlap) — doesn't matter here since external status LEDs on 4 and
5 are used, not the built-in one.

**Read-before-send.** With only the 1 kHz samples, a FreeD packet carried
whatever the last timer sample was, anywhere from 0 to 1 ms old (half a ms on
average) by the time it was built. The 1 ms goal is the age of the data when
Unreal decodes it, so that alone ate most of it. Now `loop()` asks
`network_frame_due()` first, and when a frame is due it calls
`encoder_refresh_now()` — iris over I2C first (the slow one, roughly 0.15 ms
at 400 kHz), then focus and zoom over SPI last, through the same
`accumulate()` unwrap — and the packet is built straight after. Read time
before the build should be ~15 µs for focus/zoom and ~0.2 ms for iris, worked
out from the code not measured yet (on top of the sensor's own ~100 µs
propagation delay, which no read timing changes).

**SPI CS timing.** `as5048_transfer` waited 10 µs after CS low and 10 µs after
CS high, about 30× what the datasheet asks (DS000298 v1-11 SPI timing table:
350 ns CS low to first clk, 50 ns last clk to CS high, 350 ns CS high between
frames). A register read is two frames, so it took ~44 µs and the 1 kHz ISR
held the CPU ~88 µs every ms. Now 500 / 100 / 500 ns via `delayNanoseconds`,
~6–7 µs per read at 8 MHz, margin on every minimum. Focus/zoom come out of the
ISR mask while the loop reads them, same as `encoder_read_angle` does, so the
ISR and the loop never share a bus mid-frame. Found on the way: `actual_fps`
was worked out after the send from the send time just stored, which is
negative and wraps, so the UI showed ~0 fps the whole time it streamed. Fixed
in the same split (`network_send_frame`).

**Multi-turn unwrap.** The AS5048A/B are single-turn absolute encoders, they
only ever report where the shaft is within one 360° revolution with zero
memory of how many full turns happened. CineTracker mounts the encoder to the
lens ring through a swappable gear pair the user picks themselves for whatever
mounting spot and speed a given shot needs, so the gear ratio between lens
ring and encoder shaft isn't fixed and isn't ours to control — which means a
focus pull can genuinely spin the encoder more than 360° even though the lens
ring itself never does. Below 360° this only shows up as a filter glitch for
a few samples crossing the raw 0/16383 point. Past 360° it's worse than a
glitch — the same raw angle now maps to more than one real physical position
and no amount of filtering can tell them apart.

Fix is `EncoderState.unwrapped_angle` in `math_engine.cpp`, a plain int32
running total. Every sample adds `angle_distance(prev_raw, new_raw)` onto it
instead of letting the wrapped 14-bit value reset every revolution — same
accumulate-the-delta structure as `numpy.unwrap` uses for phase data
(confirmed against numpy's own docs), chosen over filtering the sin/cos
decomposition (the alternative real approach, confirmed against VESC
firmware's own encoder docs) because a lens ring is mechanically bounded,
never spins forever, so the accumulator can't run away the way a continuously
spinning motor shaft's would. The filters (ema/median/kalman) now run on this
unwrapped value instead of the raw wrapped one, which also kills the
sub-360° glitch as a side effect — one fix covers both cases.

Calibration has to capture unwrapped positions too, not just live tracking.
`handle_cal_point()` in `web_interface.cpp` reads the live
`ctx->encoders[...].unwrapped_angle` (continuously running since boot) instead
of taking a fresh one-shot `encoder_read_angle()` at each SET button press —
otherwise any turns the user makes physically rotating the lens ring between
two button presses would be invisible and the calibrated range would be wrong.
`EncoderCalibration.min_angle/mid_angle/max_angle` in `calibration.h` are
int32 unwrapped positions now not raw 14-bit values, and
`calculate_angle_range()`/`calibration_angle_to_mm()` dropped their old modulo
wraparound math entirely.

This only stays valid from calibration start through however long the device
stays powered — a single-turn absolute encoder has no way to know about turns
that happened while it was off, that's a hard limit of the sensor not a bug.
Not a new problem for this project though: a fresh 3-point calibration is
already required every time a lens gets mounted (the physical min/max mm are
lens-specific anyway), same as real lens control systems already require a
"set marks" touch per lens.

**Calibration direction bug.** `network_stack.cpp` and `math_engine.cpp` both
assumed `cal_max` is always numerically bigger than `cal_min`, which is only
true for one rotation direction — for a lens calibrated the other way FreeD
output was already silently broken (`cal_max - cal_min` going negative into an
unsigned range, producing garbage normalized values). Direction was always
the right flag to check here, it just wasn't being checked consistently
everywhere `cal_min`/`cal_max` got used. Fixed in `math_engine.cpp`
(`handle_boundary_overshoot`, `process_encoder_sample`) and `network_stack.cpp`
(all three FreeD normalization blocks).

### Full codebase review (2026-09-18)

Read-only pass, fixes applied only after sign-off.

**Calibration wizard non-functional.** The calibration wizard was completely
non-functional from the web UI — the buttons never called `calibration/start`
so the server-side session state machine never left idle, and separately
`CAL_STATE_VALIDATE` had no code path that ever moved it to
`CAL_STATE_COMPLETE`, so even a correctly wired wizard would never have let
save succeed. Root cause was a real design mismatch: `calibration.cpp` still
had a full lens preset picker (`LensProfile`, `LENS_PRESETS` table,
`calibration_select_preset`) that the web UI was never actually built against.
Resolved by dropping the preset system entirely (never used, nobody picks a
lens from a dropdown when the real workflow is two people and a tape measure)
and keeping the guided step-by-step wizard, now backed by a real state machine
that actually works: operator manually toggles which of focus/iris/zoom this
lens has (not auto-detected — a prime has no zoom ring to detect, and SPI
absence detection for focus/zoom isn't reliable enough to gate on, only
I2C/iris can really tell not-connected from connected), each min/mid/max point
optionally takes a typed physical value (mm for focus/zoom, fstop for iris)
purely for a status-page sanity-check display, never sent over FreeD which
only ever gets the 0–100% normalized position. Two segments (min-mid, mid-max)
each get their own scale factor instead of one averaged across the whole
range, since focus rings especially aren't linear across their throw. Focus
max has no physical value at all, it's always infinity by construction, the
status page shows the infinity symbol instead of a number past that point.
This also deleted `math_engine.cpp`'s separate, parallel, already-broken copy
of the same mm conversion (`EncoderState.mm_min/mm_max` were hardcoded 0/100
and never actually set from calibration — `math_engine_apply_calibration()`,
the only thing that would have set them, was dead code nothing called), so
`/api/status`'s `focus_mm/iris_fstop/zoom_mm` fields now come from one real
implementation (`calibration_angle_to_phys`) instead of two, one of which was
silently wrong.

**Fleet stack overflow.** The same review found a real stack overflow in
`handle_fleet()` (`/api/fleet`, the peer-list json builder): it accumulated
`snprintf`'s return value against `sizeof(body) - len` across a loop, but that
return value is what `snprintf` would have written if unbounded, not what
actually fit — so once enough peers pushed it past the buffer the subtraction
(`size_t - int`) wrapped to a huge number and the next `snprintf` wrote a
full, untruncated chunk starting from an already out-of-bounds pointer. Peer
names come straight off an unauthenticated LAN broadcast (`fleet.cpp`), so
this was reachable by anything else on the network, not just a theoretical
edge case. Fixed by formatting each peer into its own small fixed buffer first
and only copying it into the real one if it actually fits, always leaving room
for the closing "]}".

**Stale camera id.** `fleet.cpp` caches `camera_id` (and the `device_name`
derived from it, "cinetracker-<id>") once in `fleet_init()` at boot, but
camera id can change live from the web UI with no reboot. Changing it kept
broadcasting the old id to peers and checking `fleet_camera_id_conflict()`
against the old one too, so conflict detection silently used a stale id until
the next reboot. Fixed with `fleet_set_camera_id()`, called from
`handle_freed_set()` right alongside `network_set_camera_id()` so both update
together, no reboot required.

**Settings page reading a dead endpoint.** The settings page loaded its
initial values from `/api/config`, which never existed server-side — it 404'd
silently every load (the fetch never checked response status) so the network
and FreeD settings cards always showed hardcoded placeholder defaults, never
the actual saved config, even right after saving real values.
`handle_network_set` (the write side) already accepted and saved ip,
multicast and port together, it was only the read side missing —
`handle_network_get` never returned multicast/port at all. Fixed by adding
those two to `handle_network_get` and pointing the page at `/api/network` +
`/api/freed`, the two endpoints that actually back these fields.

**Filter switch.** The settings page's filter/LED toggles: `led_enabled` was
hardcoded true, never real in any generation, removed. `filter_enabled`
genuinely reflected live state on the earlier Pico firmware (`filter_type > 0`)
and maps directly onto `math_engine`'s already-built ema/median/kalman
filtering here, which was already defaulting to ema for every encoder but had
no way to turn off from the web UI in any generation. Built for real this
time: `/api/filter` get+post, one shared on/off switch across all 3 encoders
(ema or none, matching the old semantics), persisted the same way genlock fps
is (`filter.bin`), applied at boot before sampling starts and live via
`math_engine_set_filter()`.

### Third review pass

Focused on everything touched by the previous two passes.

**Calibration wiped on abort.** `calibration_start_session()` memset
`ctx->cal_data` — the live/saved calibration the status page's
`focus_mm/iris_fstop/zoom_mm` readouts and `calibrated` flags come from —
immediately on pressing Start, not just on save. `calibration_abort_session()`
never restored it. So starting a calibration then aborting instead of saving
permanently wiped the status display until reboot, even though the actual
encoder state used for FreeD streaming was untouched (that was set from
storage once at boot, independent of `cal_data`). Fixed by not touching
`cal_data` at all until an actual save writes into it — the session only ever
needed its own `temp_encoders` during the wizard. Version/timestamp stamping
moved from start into the save path too, same reasoning.

**Fleet json injection (server side).** `handle_fleet()` dropped peer names
straight into the json response's `"name":"%s"` unescaped — fine for a normal
name, but those names come straight off an unauthenticated LAN broadcast, so a
crafted one containing a quote or backslash could break or inject fields into
the response for anything parsing `/api/fleet`. Fixed with a small
`json_escape()` helper (doubles quotes/backslashes, flattens control
characters to spaces) run on the name before it's ever formatted in. Buffers
grown to match the worst case of an all-quotes 31-character name doubling in
length.

**FreeD toast "undefined".** `saveFreeDSettings()` showed the literal text
"undefined" as its success toast, since `handle_freed_set()` only ever sent
`{"success":true}` and the js reads `data.message`. Fixed by adding a message
field, matching the style `handle_network_set` already uses.

**Cleanups.** Removed `CalibrationSession.timeout_ms/start_time/
step_start_time` — set throughout the wizard but nothing ever read them back.
Removed `/api/encoders` (`handle_encoders`) entirely — zero frontend consumer,
and `/api/status` already sends the same live per-encoder data the monitor tab
uses. Built the fleet web UI for real (the roadmap claimed it was done but
`/api/fleet` had zero `fetch()` calls anywhere): a Fleet card on the Monitor
tab, a conflict warning banner and a plain list of discovered peers (name/ip/
camera id), polled every 3 s, built with `createElement`/`textContent` not
`innerHTML` so a crafted peer name can't run as html/script in the page.
`handle_fleet()`'s per-entry buffer now also checked against its own size.
`resetOvershoot()`'s toast and `handle_filter_set()`'s response now reflect
real success/failure. The three settings-page init fetches now catch failures
with a visible toast. And 3 real compiler warnings in vendored `lib/t41-ptp`
fixed locally (an unused bool suppressed; a precedence parenthesization; and
one real logic bug — `slave && messageType==3 || messageType==9` parsing as
`(slave && messageType==3) || messageType==9`, fixed to `slave &&
(messageType==3 || messageType==9)` — inert here since PTP only ever runs in
slave mode, but a real bug for master mode).

### Fourth review pass (2026-09-19)

First deep look at the protocol and physics layer rather than module wiring.
Found that four core features had never actually worked.

**FreeD normalization overflow.** The FreeD packet's focus/zoom normalization
in `network_stack.cpp` did `offset * 0xFFFFFF` in int32, which overflows for
any offset past 127 encoder counts — a lens throw is thousands, so every value
past about a degree of travel wrapped negative on the wire, the device's core
output was garbage from the first packet. And right next to it,
`freed_encode_24bit` clamped to the signed 24-bit range (8388607) while the
normalization targets the unsigned one (16777215, the constant the code itself
defines), so even with the overflow gone the top half of the throw would have
sat flat. Both fixed in one place: a single `freed_normalize()` helper (64-bit
multiply, clamps to the window so a position outside it gives an end-stop not
a wrapped value) replacing the three copy-pasted blocks, and the encoder made
unsigned.

**Calibration not applied until reboot.** A calibration saved from the web UI
never reached the stream until reboot. `apply_calibration()` — the thing that
pushes `cal_min/cal_max/range` into the live `EncoderState` FreeD reads from —
was only ever called from `main.cpp` at boot. Save wrote flash and the status
page's `cal_data`, and the wire kept the old range (or the 0..16383 default on
a first-ever calibration) until a power cycle. It was the flip side of what
made the abort fix in the third pass safe: the live state being independent of
`cal_data` cut both ways. Fixed with one `calibration_apply_all()` that both
boot and save go through so they can't drift — a ring not calibrated in the
saved data goes disabled (FreeD streams 0 for it), which is exactly the state
a reboot would have produced and what a lens with no iris or zoom ring is
meant to do. `calibration_end_session` now refuses a save when the wizard
isn't complete, and `apply_calibration` clears the overshoot flag so a fresh
window starts clean.

**PTP never started with the cable in at boot.** `ptp_client_init` registers
QNEthernet's `onLinkState` callback to call `g_ptp.begin()` on link-up, but
that callback is change-only (the library just stores the pointer, no replay
of the current state) and `network_stack_init` runs first and blocks in a 10 s
DHCP wait that services the link — so the up edge fired during that wait,
before anyone was listening, `g_started` stayed false, and genlock never
locked. Only an unplug/replug rescued it, and static ip was a race not a fix.
Now the begin logic is a named function that's both registered as the callback
and called once by hand right after with `Ethernet.linkState()`, so an edge
that already went by is caught.

**AES67 multicast receive** — see the AES67 section above, fixed in this pass.

**FreeD frame scheduler stall.** The FreeD frame scheduler only advanced
`next_frame_time` while streaming, so after 36–72 minutes with streaming off
the signed `micros()` diff read the stale value as "not yet" and nothing went
out for up to another 36 minutes while the UI said streaming. Now
`network_frame_due` resyncs to now whenever it hasn't been called for over two
frame intervals, so a resume no longer counts as a missed frame.

**Unwrap moved into the ISR.** The multi-turn unwrap moved out of
`math_process_angle` into `encoder_driver`: it ran at loop rate on whatever
the ISR had last latched, so a loop stall longer than half a shaft turn (the
10 s network reconfigure did it easily) aliased a whole revolution, and
`handle_boundary_overshoot` then quietly dragged the calibration window along
to match — a permanent offset until recalibration, with nothing visibly wrong.
The ISR now accumulates the unwrapped int32 itself at 1 kHz and the loop reads
it alongside the raw value. Iris is the honest exception: it's polled over I2C
from the loop, so loop rate is its sample rate and a stall still costs it,
that's inherent to I2C-from-the-loop.

**Storage layout safety.** Two real holes: Teensy LittleFS opens for write
with no truncate (checked in `LittleFS.h`, `FILE_WRITE_BEGIN` is RDWR|CREAT),
so a file could only ever grow and a struct that shrank left a stale tail that
read back as valid data. And nothing checked size or layout on read, so a file
written by a previous struct layout was read straight into the new one as
garbage with no error. Now every file is a small header (magic, layout
version, payload size) then the payload, the file is removed before every
write so it can't carry a tail, and the payload is read into scratch and only
copied to the caller on a full match. Per-file layout versions start at 1,
bump the one whose struct changed, and `static_assert`s keep every struct
inside the scratch buffer. `filter.bin` now goes through a byte like `bootflag`
already did (reading flash straight into a bool is undefined for anything but
0 and 1). And `main.cpp` never looked at `network_stack_init`'s return: a
saved config failing validation (frame_rate 0, dest 0 — the web UI could save
either from an empty field) left Ethernet down for good with no way in but the
reset button. Now validated before init and replaced with defaults in RAM only
if rejected.

**Web server slot exhaustion.** The server has 4 client slots and the
wait-for-body branch of `process_slot` returned without ever looking at the
connection, so a post whose declared content-length never fully arrived (an
aborted upload, a browser killed mid-request, a bogus or negative length —
which wrapped through `size_t` into a count nothing could satisfy) pinned its
slot for good. Four of those and every new connection was refused until
reboot. And there was no idle timeout at all, so a client that just went quiet
did the same. Fixed three ways: the body wait now drops a disconnected client,
the declared length is clamped to the buffer so the wait is always for a
finite byte count, and any slot silent for over `WEB_CLIENT_TIMEOUT_MS` (5 s)
is dropped. `content-length` is now only looked for inside the headers and at
a line start, so a body containing the text can't set it.

**AS5048A error flag.** Checked against the real datasheet (DS000298 v1-11,
page 19 verbatim: "if the error flag is set to high because of a communication
problem the flag remains set until it will be cleared by the CLEAR ERROR FLAG
command"). Latched, in other words. The ISR discarded every frame with EF set
and nothing ever issued the clear, so one glitch — a connector wiggle, a
parity hit, the encoder powering up a moment after the first ISR frame — meant
every frame after it carried EF, got discarded, and that ring froze silently
for good. The clear is itself a read of register 0x0001. Now the ISR issues it
on an EF frame, rate-limited to once a second while errors persist so an
unplugged encoder (floating MISO reads as EF every tick) doesn't cost a clear
every millisecond forever, and marks the ring clean again on the next good
frame. `encoder_read_angle` does the same so detection self-heals, and init
clears both chips once for whatever they latched during power-up. Error and
recovery transitions are printed from the main loop (never from the ISR) with
the error register value and a running count — visible not silent, but no
diagnostics UI, that was explicitly not wanted.

**Vendored PTP tx-timestamp hang.** After sending a delay request (and, in
master mode, a sync) the library spun in a while loop waiting for the hardware
tx timestamp of that frame with no exit whatsoever. If the link dropped in the
instant between deciding to send and the frame leaving the MAC, the frame
never entered the tx ring, the timestamp never came, and the whole device hung
— and with no watchdog (by design, see ROADMAP) that was a power cycle.
"Visible failure" needs the device still running to be visible at all. Now a
bounded wait, 5 ms, that logs and skips the round on timeout; the next
exchange simply retries. Both sites go through one helper. Only the delay-
request site runs in this project's slave-only use, the sync one is fixed
identically for master mode.

**Low-severity tail.** Four handlers reported success when the flash save
failed (network, freed, aes67, genlock — the last even rebooted on a failed
save and came back on the old rate having said "rebooting to apply"): they now
say plainly that the change is applied but won't survive a reboot, and genlock
doesn't reboot at all if the fps didn't save. An empty camera id / port field
saved 0 (`parseInt('')` is NaN → null → `atoi("null")` is 0) — empty number
fields are now simply not sent so the device keeps its value, and port 0 is
rejected server-side either way. Two dead functions gone
(`calibration_get_progress`, `ptp_client_offset_ns`); `encoder_stop_sampling`
deliberately kept for the future web firmware update (it will need to halt the
sampling ISR before writing flash). The never-written `calibration_time` field
dropped from `EncoderCalibration` — the first real use of the file versioning,
`CALIBRATION_LAYOUT` is 2 now and an old `calibration.bin` is ignored with a
log line instead of being read through the wrong layout.

## Status LEDs and reset button

Status LEDs: pin 4 genlock/ptp lock, pin 5 FreeD streaming active — not two
copies of the same signal, each pin is its own real status.

Reset button: pin 6, active low, has to be held 5 s not tapped so bumping it
on set can't wipe a device mid-shoot, reuses the same `SCB_AIRCR` reboot
`handle_reboot()` already used.

**Unexpected reboot flag:** a single-byte file (`bootflag.bin`) written false
at the start of every boot and true only right before a deliberate reset (web
UI reboot or the reset button). If a boot finds it still false that means the
previous session never got to mark itself clean — crash or power loss not a
normal restart — and the web UI shows a banner for it under System until
someone clicks Acknowledge. No watchdog silently recovers from this on
purpose (see ROADMAP), this is the visible version of the same idea.
Acknowledging only clears the in-memory flag for the current session.

## Settings persistence

Network config (dhcp/ip/gateway/subnet/dest ip/dest port) and camera id
(doubles as the device id fleet conflict detection uses) survive a reboot,
saved to `network.bin` as one `NetworkConfig` blob whenever
`handle_network_set` or `handle_freed_set` actually changes something. A real
bug found while wiring this up: `handle_network_set` started from
`network_get_default_config()` not the live config, so changing just one field
(say the port) would silently reset every other field to its hardcoded default
instead of leaving it alone. Now starts from `network_get_current_config()` so
partial updates behave correctly. FreeD `frame_rate` also triggers the same
`NetworkConfig` save since it's part of that same struct. AES67 direction +
destination survive a reboot too, a separate small `Aes67Settings` struct
saved to `aes67.bin` whenever `handle_aes67_set` runs. `stream_name` isn't
user-settable so it stays a `main.cpp` default. A separate human-friendly
device display name is explicitly not wanted (see ROADMAP).
