CineTracker was originally built for RP2040, now being rebuilt for Teensy
4.1 to add PTP, genlock and AES67/LTC timecode. see ARCHITECTURE.md for the
technical side.

Order of work (status as of 2026-09-17, none of this tested on real
hardware yet, only build verified):

1. [done, untested] Teensy 4.1 + native ethernet bring up, port the FIZ
   encoder driver and math (calibration, filtering) from the old codebase.
   dropped the saved lens profile slots (named presets, JSON import/export)
   from that port, real focus pullers just recalibrate fresh per lens so
   it was dead weight, not something anyone would actually use. web
   interface + fleet auto discovery also done here even though that was
   originally item 6, built it early since it ties everything else
   together for testing.
2. [done, untested] PTP time sync over the network. vendored not remote
   dependency, see ARCHITECTURE.md for why.
3. [done, untested] Genlock generation, locked to PTP. 24/25/30/50/60fps,
   25 default since were in europe, now a real web ui setting (reboot to
   apply, not a live hot swap - the ptp client is only safely init'd once)
   instead of a compile time #define, no more reflash to change it. true
   fractional rates (23.976 etc) explicitly not wanted, not deferred.
4. [done, untested] AES67 audio carrying LTC timecode, in and out.
   direction + destination address wired to the web ui, real jitter
   buffer on rx.
5. [not started] Extend the FreeD packet with PTP + timecode timestamps,
   and get a modified Unreal LiveLink plugin reading it.
6. [done, untested, moved up] Fleet web UI: auto discovery, read only
   status for multiple devices, camera ID conflict warnings. see item 1.

Explicitly not doing, all considered and rejected not just deferred:
a watchdog that silently reboots and recovers on its own - if the device
gets stuck during a take we want that to be visible and force a manual
reboot, not hidden from whoever's running the shot. config backup/
restore and fleet-wide firmware update - updates and recovery need to
happen carefully by hand on each device, not automated across a fleet.
a separate human friendly device display name - camera id is the only
device identity that matters, and it already doubles as the number
written on the enclosure. a physical calibration set-point button and a
calibration lock - real field testing across three builds (esp32, pico,
teensy) showed physical buttons cant give reliable confirmation, and the
web ui's tab separation already prevents accidental calibration triggers
without needing a lock. an "identify this device" feature - real studios
run at most ~4 cameras, this only pays off at fleet scale this project
will never reach. encoder health diagnostics in the UI - too much detail
nobody asked for. a battery backed RTC for timecode continuity - doesnt
actually help, genlock has no holdover of its own, so losing power kills
sync completely regardless of whether timecode kept ticking through the
gap, the take is unusable either way.

7. [not started] Firmware updates from the web UI - upload a file, click
   update, done. the normal teensy update path needs the teensy loader
   app and a button press on the board, fine for us but not something to
   ask a focus puller or dit to do. real ota (auto pushed over the
   network with rollback) was considered and dropped, too much complexity
   for what this actually needs - a person is standing at the device
   either way. closest existing starting point found is FlasherX with its
   ethernet fork (already built on QNEthernet, same library we use), not
   a drop in though, needs real integration work and a better safety
   story than "type this number to confirm" before it touches a unit at
   a studio.
8. [done, untested] Status LEDs (genlock/ptp lock, freed streaming active)
   and a physical factory reset button (hold 5s to confirm) for wrong
   settings or moving to a different studio's network. also added an
   unexpected reboot flag (crash/power loss vs a deliberate reset) shown
   as a web ui banner until acknowledged, same visible-not-silent idea as
   skipping the watchdog above.
9. [done, untested] Network config, camera id, FreeD frame_rate metadata,
   AES67 direction/destination, genlock fps (see item 3), and now the
   filter on/off switch too all survive a reboot. every currently
   web-settable value is persisted.

This week (starting 2026-09-16) is a sprint to get the repo public ready
with at least the basics working, which pieces count as "basics" is still
getting worked out day by day, but encoders + web interface is the minimum
bar. Target: public on github with a working basic version before applying
for NLnet funding (their deadline for showing a roadmap and basic
architecture is 2026-11-03).
