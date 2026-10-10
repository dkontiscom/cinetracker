Extended FreeD

Draft 0.1, 2026-10-10. Nothing here is implemented yet, its the design to
build the firmware side and the Unreal LiveLink plugin against. Things
still open are marked OPEN and listed at the end.

Why

plain FreeD (the 29 byte D1 packet) says where the lens is but not when.
a receiver only knows when the packet arrived, and that includes switch
and OS delay that moves around by tenths of a ms or more. CineTracker
already runs PTP and genlock off the same clock, so it can say exactly
when the encoders were read. with that the receiver can line the lens
data up with the camera frame (also genlocked to the same PTP clock)
instead of guessing from arrival time. timecode goes in too so the lens
data can be matched to recorded clips in post, and a sequence number so
lost or reordered packets show up instead of being invisible.

Decided (2026-10-10)

  - plain D1 stays exactly as it is. the extended data is a separate,
    self contained packet with its own type byte, not bytes appended to
    D1 (appending breaks any receiver that checks the length is 29)
  - web ui switch: plain / extended / both. default both, so anything
    that reads FreeD today keeps working and the plugin reads the
    extended one
  - the extended packet carries: PTP sample time, SMPTE timecode,
    sequence number. no device latency field, the receiver can work out
    the full latency itself from the PTP sample time

Transport

  - UDP, same destination address and port as plain FreeD (web ui setting,
    default port 40000), unicast or broadcast like today
  - in "both" mode the two packets are built from the same sample and sent
    back to back, plain first
  - same send rate as plain FreeD (the FreeD frame_rate setting, max 240)
  - OPEN: same port as plain or its own port. same port = one receiver
    setting, relies on receivers dropping unknown type bytes. own port =
    zero risk for old receivers, one more setting

Plain D1 as CineTracker sends it (unchanged, for reference)

  byte  0       0xD1
  byte  1       camera id
  bytes 2-19    pan, tilt, roll, x, y, z - 3 bytes each, always 0 (lens
                only tracker)
  bytes 20-22   zoom, 24 bit unsigned, 0..0xFFFFFF over the calibrated
                range
  bytes 23-25   focus, same
  byte  26      iris, 0..255 over the calibrated range
  byte  27      user data (web ui setting)
  byte  28      checksum = 0x40 minus the sum of bytes 0-27, mod 256

note: the two spare bytes (26-27) arent standardised, every vendor uses
them differently (sony puts F-number x100 plus a frame counter there for
example). thats one more reason for a proper packet with its own fields.

Extended packet, version 1 (52 bytes, all multi byte values big endian
like FreeD)

  offset size field
  0      1    type, 0xE1 (OPEN, see below)
  1      1    version, 0x01
  2      1    camera id, same as D1
  3      24   pan, tilt, roll, x, y, z, zoom, focus - exactly D1 bytes
              2-25, so the same parser code works
  27     3    iris, 24 bit unsigned like zoom/focus (OPEN: proposal, D1
              only has 8 bits for it)
  30     1    user data, same as D1 byte 27
  31     4    sequence number, uint32, +1 per extended packet, wraps
  35     1    flags
                bit 0  ptp locked (genlock_locked()) when sampled
                bit 1  timecode valid
                bit 2  timecode source: 0 = decoded LTC, 1 = derived from
                       PTP time
                bit 3  timecode drop frame (always 0 now, fractional rates
                       arent supported, see ROADMAP)
                bits 4-7  reserved, 0
  36     6    sample time, seconds, 48 bit unsigned
  42     4    sample time, nanoseconds, 0..999999999
              (36-45 is the IEEE 1588 timestamp format: PTP time, TAI,
              not UTC)
  46     1    timecode hours   0..23
  47     1    timecode minutes 0..59
  48     1    timecode seconds 0..59
  49     1    timecode frames  0..fps-1
  50     1    timecode rate, frames per second: 24, 25, 30, 50 or 60
  51     1    checksum, 0x40 minus the sum of bytes 0-50, mod 256 (same
              rule as D1)

receivers check byte 0 and the checksum, accept any length >= 52 for
version 1 and ignore extra bytes at the end, so later versions can add
fields at the end without breaking version 1 receivers. a version number
they dont know means the layout changed in a non compatible way, drop it.

What the fields mean exactly

sample time
  the PTP time when the newest focus/zoom sample that went into this
  packet was taken. focus and zoom are read in the sampling interrupt
  (SPI, IntervalTimer). the plan is to read the ethernet PTP timer in
  that same interrupt, right before the SPI reads (not done yet, the
  firmware doesnt timestamp samples today).
  caveats, all OPEN:
    - iris is read over I2C from the main loop at its own, lower rate, so
      the iris value can be older than the sample time. one timestamp
      per packet or one per axis?
    - the values sent are filtered (math_engine). if the filter adds
      delay, the sample time should be shifted by it, or the filter
      delay sent as well
    - the three reads arent simultaneous. SPI focus and zoom are a few
      us apart, probably negligible, but its not measured

timecode
  the timecode of the video frame the sample time falls in.
  the firmware doesnt decode timecode yet, it only carries LTC as audio
  over AES67. two ways to get it, OPEN which ones:
    - decode the LTC its already carrying (from the adc in LTC_IN mode,
      from the network stream in LTC_OUT mode). needs an LTC decoder in
      the firmware
    - derive it from PTP time (SMPTE ST 2059-1 defines how), works with no
      LTC at all but needs a defined time of day / offset setting
  OPEN: how frames map at 50/60p (as far as i know LTC counts frame pairs
  there, needs checking against SMPTE ST 12-1)

sequence number
  counts extended packets only. a jump means lost packets, going
  backwards means reordering or a reboot (flags and sample time tell
  those apart).

Compatibility tests before calling it done

  - stock Unreal LiveLink FreeD source receiving "both" on the same port:
    does it ignore 0xE1 packets cleanly. not checked, epics source isnt
    public without an account
  - same with whatever else is on the stages we work with (aximmetry,
    touchdesigner, disguise etc, whichever is available)
  - our plugin with plain only (must fall back to arrival time), extended
    only, and both

Open questions

  1. type byte. 0xE1 is a placeholder. the only free-d types confirmed so
     far are D0 (stream start/stop) and D1 (data) - sony FR7 free-d
     integration doc. check the full free-d spec (vinten installation
     manual appendices) and known vendor extensions before freezing it
  2. same udp port as plain FreeD or its own port
  3. iris as 24 bit in the extended packet (proposal) or keep 8 bit
  4. one sample time per packet or per axis (iris is read at a different
     rate and time)
  5. filter delay: shift the sample time by it, or send it
  6. timecode source: decoded LTC, derived from PTP, or both with the
     source flag (the layout above allows both)
  7. timecode at 50/60p (frame pairs in LTC?)
  8. should the extended packet also carry the PTP grandmaster identity
     (8 bytes), so a receiver can tell two devices are on different
     clocks

Sources
  - free-d D0/D1 as used by sony: sony ILME-FR7 free-d integration doc,
    https://www-origin.sony.jp/ls-camera/download/pdf/FR7/ILME-FR7_free-d_integration_camera_v1_10.pdf
  - IEEE 1588 timestamp format (48 bit seconds, 32 bit ns): IEEE 1588-2019
  - plain D1 as CineTracker builds it: src/network_stack.cpp,
    include/network_stack.h
