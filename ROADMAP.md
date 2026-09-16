CineTracker was originally built for RP2040, being rebuilt for Teensy 4.1 to
add PTP, genlock and AES67/LTC timecode. See ARCHITECTURE.md for the technical
side.

Order of work:

1. Teensy 4.1 + native Ethernet bring-up, port the FIZ encoder driver and math
   (calibration, lens profiles, filtering) from the old codebase.
2. PTP time sync over the network.
3. Genlock generation, locked to PTP.
4. AES67 audio carrying LTC timecode, in and out.
5. Extend the FreeD packet with PTP + timecode timestamps, and get a modified
   Unreal LiveLink plugin reading it.
6. Fleet web UI: auto-discovery, read-only status for multiple devices,
   Camera ID conflict warnings.

Smaller things planned alongside the above, roughly where they fit best:
encoder health diagnostics in the UI, status LEDs for genlock/PTP lock state,
battery-backed RTC for timecode continuity across brief dropouts.

This week (starting 2026-09-16) is a sprint to get the repo public-ready with
at least the basics working - which pieces count as "basics" is still being
worked out day by day, but encoders + web interface is the minimum bar.
Target: public on GitHub with a working basic version, before applying for
NLnet funding (their deadline for showing a roadmap and basic architecture is
2026-11-03).
