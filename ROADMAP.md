# Roadmap

CineTracker was originally built for the RP2040 and is being rebuilt for the
Teensy 4.1 to add PTP, genlock and AES67/LTC timecode. See
[ARCHITECTURE.md](ARCHITECTURE.md) for the technical side.

Two parts below: what the firmware already has, then the milestones from here
on (firmware and hardware together). Status as of 2026-10-10.

> Everything marked **done** is **untested** — written and build-verified, not
> run on real hardware. There are no boards yet.

## What the firmware already has

1. **Teensy 4.1 + native Ethernet bring-up**, plus the FIZ encoder driver and
   math (calibration, filtering) ported from the old codebase. The saved lens
   profile slots (named presets, JSON import/export) were dropped from that
   port — real focus pullers just recalibrate fresh per lens. The web
   interface and fleet auto-discovery were built here too (originally planned
   later), since they tie everything together for testing.
2. **PTP time sync** over the network. Vendored, not a remote dependency — see
   [ARCHITECTURE.md](ARCHITECTURE.md) for why.
3. **Genlock generation**, locked to PTP. 24/25/30/50/60 fps, 25 default
   (Europe), a runtime web-UI setting (reboot to apply, not a live hot-swap —
   the PTP client is only safely initialised once) rather than a compile-time
   `#define`. True fractional rates (23.976 etc.) are explicitly not wanted,
   not deferred.
4. **AES67 audio carrying LTC timecode**, in and out. Direction and
   destination address are web-UI settings, with a real jitter buffer on
   receive.
5. **Fleet web UI**: auto-discovery, read-only status for multiple devices,
   camera-id conflict warnings.
6. **Status LEDs** (genlock/PTP lock, FreeD streaming active) and a physical
   **factory-reset button** (hold 5 s to confirm) for wrong settings or moving
   to another studio's network. Also an unexpected-reboot flag (crash/power
   loss vs. a deliberate reset), shown as a web-UI banner until acknowledged —
   the same visible-not-silent idea as skipping the watchdog.
7. **Settings persistence**: network config, camera id, FreeD frame-rate
   metadata, AES67 direction/destination, genlock fps and the filter on/off
   switch all survive a reboot. Every currently web-settable value is
   persisted.
8. **AES67 SAP announcement** gated to the transmit direction only. SAP is a
   sender's mechanism — a receiving device shouldn't self-announce as a source
   at the same multicast address (it could show as a conflicting duplicate
   source in Dante Controller or anything else discovering via SAP). Now runs
   only when direction is `LTC_IN`, and flipping to `LTC_IN` announces
   immediately rather than up to 30 s later.

Still to build on the firmware side: **extended FreeD** (milestones M2–M3
below) and **web firmware updates** (M7).

## Explicitly not doing

All considered and rejected, not just deferred:

- **A silent watchdog** that reboots and recovers on its own — if the device
  gets stuck during a take we want that visible and forcing a manual reboot,
  not hidden from whoever's running the shot.
- **Config backup/restore and fleet-wide firmware update** — updates and
  recovery need to happen carefully, by hand, on each device, not automated
  across a fleet.
- **A separate human-friendly device display name** — camera id is the only
  device identity that matters, and it already doubles as the number written
  on the enclosure.
- **A physical calibration set-point button and a calibration lock** — field
  testing across three builds (ESP32, Pico, Teensy) showed physical buttons
  can't give reliable confirmation, and the web UI's tab separation already
  prevents accidental calibration triggers.
- **An "identify this device" feature** — real studios run at most ~4 cameras;
  this only pays off at a fleet scale this project will never reach.
- **Encoder health diagnostics in the UI** — too much detail nobody asked for.
- **A battery-backed RTC for timecode continuity** — doesn't actually help.
  Genlock has no holdover of its own, so losing power kills sync completely
  whether or not timecode kept ticking through the gap; the take is unusable
  either way.

## Milestones

### M1 — Repo ready to show ✅
README, this roadmap, the extended FreeD spec draft, genlock timing notes, and
carrier v0.61 + PoE board v0.61 as full KiCad projects with fab files and
docs. Hardware licence CERN-OHL-S-2.0 decided. Submitted to NLnet.

### M2 — Extended FreeD, sender side (firmware)
- Settle the open items in [EXTENDED-FREED.md](EXTENDED-FREED.md) (type byte,
  port, iris bits, per-axis timestamps, filter delay, timecode source).
- Read the PTP timer in the encoder sampling interrupt and carry the sample
  time through to the packet.
- Sequence number, flags, and the plain / extended / both switch in the web UI
  (default both), persisted like the other settings.
- Timecode: an LTC decoder for the audio the device already carries, and/or
  timecode derived from PTP time (ST 2059-1).
- Compatibility test: stock Unreal FreeD source and whatever else is on hand
  must ignore the extended packets cleanly.

### M3 — Unreal LiveLink plugin, receiver side
- A LiveLink source plugin, not an engine fork.
- Reads extended packets and uses the PTP sample time to line lens data up
  with the frame time (interpolating between samples); falls back to arrival
  time with plain D1.
- Shows lost/reordered packets from the sequence number.

### M4 — Hardware prototypes
- v0.60 done for both boards (0.1% genlock resistors, MISO pull-ups, connector
  courtyards on the carrier, PoE renumbered from v0.1).
- v0.61 done for both boards: moved to KiCad 10, ERC clean, PoE track moved off
  the M12 housing pin. Next is v0.62.
- Owner review, then a professional PCB designer review once there is funding;
  fixes go in as the next v0.6x.
- PoE board: whatever the M12 samples show (pin mapping, panel position, metal
  shell); the long pin header to carrier J7 cut to the measured stack height.
- Order both together, bring-up, bench checks from each board's
  `documentation/open-issues.md`.
- Measurements: genlock levels and rise times, real 5 V current, genlock
  timing baseline per [GENLOCK-TIMING.md](GENLOCK-TIMING.md).

### M5 — Genlock edges from a hardware timer
- FlexPWM4 submodule 2 drives pins 2 and 3 instead of busy-waits in a
  priority-0 interrupt, kept on PTP time (both approaches in
  [GENLOCK-TIMING.md](GENLOCK-TIMING.md)).
- First check the NXP reference manual for whether the PTP timer event can
  trigger it in hardware.
- Validate against the M4 baseline at all five rates.

### M6 — Field tests
- Lock tests on real cameras at 24, 25, 30, 50, 60p.
- PoE from MikroTik (mode B), UniFi and Cisco (mode A) switches.
- Full chain on a stage: lens data, genlock, timecode, Unreal with the plugin.

### M7 — Firmware updates from the web UI
Upload a file, click update, done. The normal Teensy update path needs the
Teensy Loader app and a button press on the board — fine for us, but not
something to ask a focus puller or DIT to do. Real OTA (auto-pushed over the
network with rollback) was considered and dropped: too much complexity for
what this needs, since a person is standing at the device either way. The
closest starting point found is FlasherX with its Ethernet fork (already built
on QNEthernet), though it needs real integration work and a better safety
story than "type this number to confirm" before it touches a unit at a studio.
