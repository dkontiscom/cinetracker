CineTracker was originally built for RP2040, now being rebuilt for Teensy
4.1 to add PTP, genlock and AES67/LTC timecode. see ARCHITECTURE.md for the
technical side.

Order of work (status as of 2026-09-17, none of this tested on real
hardware yet, only build verified):

1. [done, untested] Teensy 4.1 + native ethernet bring up, port the FIZ
   encoder driver and math (calibration, lens profiles, filtering) from
   the old codebase. web interface + fleet auto discovery also done here
   even though that was originally item 6, built it early since it ties
   everything else together for testing.
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
continuity across brief dropouts [not started].

This week (starting 2026-09-16) is a sprint to get the repo public ready
with at least the basics working, which pieces count as "basics" is still
getting worked out day by day, but encoders + web interface is the minimum
bar. Target: public on github with a working basic version before applying
for NLnet funding (their deadline for showing a roadmap and basic
architecture is 2026-11-03).
