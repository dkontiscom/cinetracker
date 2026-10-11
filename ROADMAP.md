CineTracker was originally built for RP2040, now being rebuilt for Teensy
4.1 to add PTP, genlock and AES67/LTC timecode. see ARCHITECTURE.md for the
technical side.

Two parts below: what the firmware already has (numbered list, kept as
it was written), then the milestones from here on, firmware and hardware
together. status as of 2026-10-10.

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
   and get a modified Unreal LiveLink plugin reading it. now milestones
   M2 + M3 below, spec draft in EXTENDED-FREED.md.
6. [done, untested, moved up] Fleet web UI: auto discovery, read only
   status for multiple devices, camera ID conflict warnings. see item 1.

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
10. [done, untested] AES67's SAP announcement (send_sap_announce, see
    ARCHITECTURE.md) fired every 30s regardless of direction. SAP is a
    sender's mechanism - a receiving device shouldnt self-announce as a
    source at the same multicast address, could show as a conflicting
    duplicate source in dante controller (or anything else discovering
    via sap) if some other device is the real source there. now gated to
    only run when direction is LTC_IN (actually transmitting), and
    flipping to LTC_IN announces immediately not up to 30s later.

7. [not started, milestone M7] Firmware updates from the web UI - upload a file, click
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

Milestones from here

M1. [in progress] Repo ready to show (target 2026-11-03)
    readme, this roadmap, extended FreeD spec draft, genlock timing notes,
    carrier v0.61 and PoE board v0.61 as full KiCad projects with fab files
    and docs. hardware licence CERN-OHL-S-2.0 decided. basics working on
    the firmware side is still the bar: encoders + web interface.

M2. [not started] Extended FreeD, sender side (firmware)
    - settle the OPEN items in EXTENDED-FREED.md (type byte, port, iris
      bits, per axis timestamps, filter delay, timecode source)
    - read the PTP timer in the encoder sampling interrupt, carry the
      sample time through to the packet
    - sequence number, flags, the plain / extended / both switch in the
      web ui (default both), persisted like the other settings
    - timecode: LTC decoder for the audio the device already carries
      and/or timecode derived from PTP time (ST 2059-1)
    - compatibility test: stock Unreal FreeD source and whatever else is
      on hand must ignore the extended packets cleanly

M3. [not started] Unreal LiveLink plugin, receiver side
    - LiveLink source plugin, not an engine fork
    - reads extended packets, uses the PTP sample time to line lens data
      up with the frame time (interpolate between samples), falls back to
      arrival time with plain D1
    - shows lost/reordered packets from the sequence number

M4. [not started] Hardware prototypes
    - v0.60 done for both boards (0.1% genlock resistors, MISO pull-ups,
      connector courtyards on the carrier, poe renumbered from v0.1)
    - v0.61 done for both boards: moved to KiCad 10, ERC clean, PoE track
      moved off the M12 housing pin. next is v0.62
    - my review, then a professional pcb designer review once there's
      funding, fixes go in as the next v0.6x
    - PoE board: whatever the M12 samples show (pin mapping, panel
      position, metal shell), long pin header to carrier J7 cut to the
      measured stack height
    - order both together, bring up, bench checks from
      hardware/carrier/documentation/open-issues.md and
      hardware/poe/documentation/open-issues.md
    - measurements: genlock levels and rise times, real 5 V current,
      genlock timing baseline per GENLOCK-TIMING.md

M5. [planned] Genlock edges from a hardware timer
    - FlexPWM4 submodule 2 drives pins 2 and 3 instead of busy waits in a
      priority 0 interrupt, kept on PTP time (details and the two ways to
      do it in GENLOCK-TIMING.md)
    - first check in the nxp reference manual whether the PTP timer event
      can trigger it in hardware
    - validate against the M4 baseline at all 5 rates

M6. [not started] Field tests
    - lock tests on real cameras at 24, 25, 30, 50, 60p
    - PoE from MikroTik (mode B), UniFi and Cisco (mode A) switches
    - full chain on a stage: lens data, genlock, timecode, Unreal with the
      plugin

M7. [not started] Firmware updates from the web ui, see item 7 above
