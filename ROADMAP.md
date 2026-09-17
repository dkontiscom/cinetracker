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
3. [done, untested] Genlock generation, locked to PTP. 24/25/30/60fps,
   25 default since were in europe. true fractional rates (23.976 etc)
   still not supported.
4. [done, untested] AES67 audio carrying LTC timecode, in and out.
   direction + destination address wired to the web ui, real jitter
   buffer on rx.
5. [not started] Extend the FreeD packet with PTP + timecode timestamps,
   and get a modified Unreal LiveLink plugin reading it.
6. [done, untested, moved up] Fleet web UI: auto discovery, read only
   status for multiple devices, camera ID conflict warnings. see item 1.

Smaller things planned alongside the above, roughly where they fit best:
encoder health diagnostics in the UI [not started], status LEDs for
genlock/PTP lock state [not started], battery backed RTC for timecode
continuity across brief dropouts [not started], one clear ok/not-ok
status readout in the web ui and on the physical leds instead of raw
numbers someone has to interpret [not started], physical factory reset
button (hold to confirm, pin 6) for when settings got messed up or the
device moves to a different studio's network [not started]. explicitly
not doing: a watchdog that silently reboots and recovers on its own -
if the device gets stuck during a take we want that to be visible and
force a manual reboot, not hidden from whoever's running the shot.
config backup/restore and fleet-wide firmware update are also explicitly
not planned - updates and recovery need to happen carefully by hand on
each device, not automated across a fleet.

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

This week (starting 2026-09-16) is a sprint to get the repo public ready
with at least the basics working, which pieces count as "basics" is still
getting worked out day by day, but encoders + web interface is the minimum
bar. Target: public on github with a working basic version before applying
for NLnet funding (their deadline for showing a roadmap and basic
architecture is 2026-11-03).
