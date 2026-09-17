CineTracker

Firmware for tracking a cinema lens (focus, iris, zoom) and sending that data
to Unreal Engine over the FreeD protocol, for virtual production. Runs on a
Teensy 4.1.

On top of FIZ tracking it also does PTP time sync, genlock generation locked
to that PTP clock, and AES67 audio for carrying LTC timecode to and from
nearby devices (cameras, recorders). The FreeD packet is extended with PTP
and timecode timestamps, which needs a modified Unreal LiveLink plugin to
read (in progress).

This is a rewrite of an earlier RP2040-based version of the same idea. See
ROADMAP.md for where things stand and ARCHITECTURE.md for the technical
design. See CREDITS.md for the other projects this is built on.

Status: early rewrite, not working yet. Do not use this for anything real.

Building

Needs PlatformIO. Board target is teensy41.

  pio run

This builds firmware.hex under .pio/build/teensy41/. Flash it with the
official Teensy Loader, same as any other Teensy 4.1 project - there's no
special install step here.

License: AGPL-3.0-or-later, see LICENSE. Comes from depending on QNEthernet
for the PTP/genlock/AES67 timing stack, which is AGPL itself - so the whole
firmware is too.
