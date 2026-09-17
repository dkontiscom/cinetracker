CineTracker

Firmware for tracking a cinema lens (focus, iris, zoom) and sending that
data to Unreal Engine over the FreeD protocol, for virtual production.
Runs on a Teensy 4.1.

On top of FIZ tracking it also does PTP time sync, genlock generation
locked to that PTP clock, and AES67 audio for carrying LTC timecode to and
from nearby devices (cameras, recorders). The FreeD packet gets extended
with PTP and timecode timestamps, which needs a modified Unreal LiveLink
plugin to read it (still in progress).

This is a rewrite of an earlier RP2040 version of the same idea. See
ROADMAP.md for where things stand and ARCHITECTURE.md for the technical
side. See CREDITS.md for the other projects this is built on.

Status: early rewrite, not working yet. dont use this for anything real.

Building

Needs PlatformIO. board target is teensy41.

  pio run

This builds firmware.hex under .pio/build/teensy41/. flash it with the
official Teensy Loader, same as any other Teensy 4.1 project, theres no
special install step here.

License: AGPL-3.0-or-later, see LICENSE. comes from depending on QNEthernet
for the PTP/genlock/AES67 timing stack which is AGPL itself, so the whole
firmware ends up AGPL too.
