CineTracker

Open hardware and firmware for tracking a cinema lens (focus, iris, zoom)
and sending that data to Unreal Engine over the FreeD protocol, for
virtual production. Runs on a Teensy 4.1.

On top of FIZ tracking the same box does PTP time sync, genlock generation
locked to that PTP clock, and AES67 audio for carrying LTC timecode to and
from nearby devices (cameras, recorders). The idea is one small device on
the camera, one cable (PoE ethernet) carrying lens data, sync and timecode.

The headline part is extended FreeD: the same FreeD data plus the PTP time
the encoders were read, timecode and a sequence number, so a receiver
knows exactly when the lens was where it says, not just when the packet
arrived. Draft spec in EXTENDED-FREED.md, a matching Unreal LiveLink plugin
is planned.

This is a rewrite of an earlier RP2040 version of the same idea.

Status: early rewrite, not working yet. dont use this for anything real.
firmware builds but nothing has run on real hardware yet, the boards are
designed but not built. see ROADMAP.md for where things stand.

How it fits together

  lens encoders (3x AS5048, M8 cables)
          |
  carrier board  - genlock out (BNC, tri-level), LTC in/out (M8),
          |        status leds, reset
  Teensy 4.1     - firmware: encoders, FreeD, PTP, genlock, AES67, web ui
          |
  PoE board      - M12 X-coded ethernet, isolated 5 V from PoE
          |
  network (PoE switch, PTP grandmaster) -> Unreal Engine, other devices

Repo layout

  src/, include/        firmware
  lib/t41-ptp,
  lib/t41-tri-sync      vendored PTP client and genlock pulse generator,
                        see CREDITS.md and ARCHITECTURE.md for why vendored
  hardware/carrier/     carrier board, KiCad 10 project, fab files, docs
  hardware/poe/         PoE board, KiCad 10 project, fab files, docs
  ARCHITECTURE.md       how the firmware works and why, decisions log
  ROADMAP.md            milestones and status
  EXTENDED-FREED.md     extended FreeD packet spec, draft
  GENLOCK-TIMING.md     genlock timing, how to measure it, planned rework
  CREDITS.md            the projects this builds on

Hardware status, details in hardware/README.md:

  carrier  v0.61, designed in KiCad, not built. dont build v0.3 to v0.5c,
           they had the teensy socket rows swapped
  poe      v0.61, designed in KiCad, not built
  revisions go v0.60, v0.61, v0.62. before ordering: my review, then a
  professional pcb designer when the funding is there

Building the firmware

Needs PlatformIO. board target is teensy41. platformio.ini pins the
platform and library versions, change them there on purpose.

  pio run

This builds firmware.hex under .pio/build/teensy41/. flash it with the
official Teensy Loader, same as any other Teensy 4.1 project, theres no
special install step here.

The hardware is KiCad 10 (10.0.7 used), older KiCad wont open the files.
open the .kicad_pro in each board folder. the schematics are the source,
fab files in fab/ are generated from them. tools/ in each board folder
has the check scripts (ERC, DRC, schematic vs board and more).

License

Firmware (everything outside hardware/): AGPL-3.0-or-later, see LICENSE.
comes from depending on QNEthernet for the PTP/genlock/AES67 timing stack
which is AGPL itself, so the whole firmware ends up AGPL too.

Hardware (everything under hardware/): CERN-OHL-S-2.0, strongly
reciprocal, copyright 2026 Dimitris Kontis and CineTracker contributors,
see hardware/LICENSE. the
boards carry the source location (github.com/dkontiscom/cinetracker) on
their silkscreen.

The extended FreeD spec text (EXTENDED-FREED.md) is part of this repo and
covered by the same AGPL for now. OPEN: whether the spec should get its
own, more permissive licence so closed receivers can implement it freely.
