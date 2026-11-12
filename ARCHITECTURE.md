# Architecture

How the Teensy 4.1 firmware is put together and why. This is the current
picture; the reasoning behind each decision and the history of fixes behind
it live in [DEVLOG.md](DEVLOG.md). Genlock timing has its own note
([GENLOCK-TIMING.md](GENLOCK-TIMING.md)), as does the extended FreeD packet
([EXTENDED-FREED.md](EXTENDED-FREED.md)).

Nothing here has run on real hardware yet. The firmware builds; the subsystems
are verified against datasheets, library sources and by compiling, not on a
bench. See [ROADMAP.md](ROADMAP.md) for status.

## Priorities

In order: **genlock**, **timecode** (LTC over AES67), **FIZ encoder data and
its FreeD output**. Same latency and frame-rate targets as the earlier RP2040
version: sub-millisecond encoder latency, up to 240 fps FreeD. Everything else
(fleet UI, diagnostics, status LEDs) is secondary and must not get in the way
of those three.

## The main design rule

Anything time-critical runs off a hardware timer, DMA or interrupt, never off
the main `loop()`. The Teensy 4.1 has a single core (RT1062, Cortex-M7,
600 MHz, with an FPU, unlike the old RP2040's M0+). `loop()` is reserved for
the web server, JSON building and storage writes — work that can tolerate
being delayed by a few hundred microseconds. Interrupts always preempt
`loop()` on this chip, so as long as the ISRs stay short, genlock, PTP, audio
and encoders keep their timing even while the web server is busy.

## Subsystems

### Ethernet + PTP

Native Teensy 4.1 Ethernet MAC via QNEthernet, with hardware timestamping in
the MAC itself (not software-timed). The official Ethernet kit connects
through its own dedicated pads, outside the standard numbered pin set, so it
costs no GPIO.

One shared PTP client (`t41-ptp`, from IMS-AS-LUH) feeds both genlock and
AES67 timing — not two separate clients. Both `t41-ptp` and the tri-level sync
generator are **vendored into this repo** (`lib/t41-ptp`, `lib/t41-tri-sync`)
rather than pulled as remote dependencies, because the version genlock needs
is the one bundled inside `t41-tri-sync-ptp`, newer than the standalone repo.
It needs the `HedgeHawk/QNEthernet` fork (branch `ieee1588-2-fix`). The full
reasoning is in [DEVLOG.md](DEVLOG.md) and [CREDITS.md](CREDITS.md). The
device is only ever a PTP **slave**, never a master.

### Genlock

A real SMPTE ST 0274-2008 tri-level sync generator (`src/genlock.cpp`, built
on `lib/t41-tri-sync`). The Ethernet PTP timer fires a compare interrupt once
per video line, and the handler flips the two ladder-output pins (2 and 3) to
make the waveform. That interrupt (`IRQ_ENET_TIMER`) runs at the top NVIC
priority (0) — the sync edge is the one thing that must not wait for anything
else.

Frame rate is a runtime web-UI setting (`/api/genlock`), 24/25/30/50/60 fps,
default 25, persisted in `genlock.bin`. Changing it reboots the device to
apply — the PTP client is only safely initialised once per boot, so this is
not a live hot-swap. True fractional rates (23.976, 29.97, 59.94) are
deliberately **not** supported. There is a planned rework to move the edges
onto a hardware timer; see [GENLOCK-TIMING.md](GENLOCK-TIMING.md).

Genlock is not usable immediately at power-on: the PTP servo has to pull the
local clock into alignment first, so the lock LED (pin 4) stays dark for a
while after boot. Don't roll camera until it lights.

### AES67 / LTC

Carries SMPTE LTC timecode as audio over the network. Mono, and only one
direction is active at a time — `LTC_IN` (local audio in → RTP to network) or
`LTC_OUT` (RTP from network → local audio out), never both. Direction and
destination ip/port are web-UI settings (`/api/aes67`), persisted in
`aes67.bin`; boots off until set.

Audio hardware is a **PCM5102A DAC + PCM1808 ADC** pair (not the PJRC audio
shield, which was 16-bit and conflicted with the Focus encoder's SPI pins).
Both are bare chips with no I2C and no control registers — 24-bit is how they
are wired on the board, nothing to configure in software. The firmware drives
the raw I2S peripheral through `OpenAudio_ArduinoLibrary`'s 32-bit-float I2S
classes.

Transport is plain L24 mono over RTP (RFC 3190 style), 48 samples/packet at
48 kHz, announced via SAP/SDP (RFC 2974/7273). A 4-packet jitter buffer on
receive plays out on its own `micros()` pace, independent of network arrival
jitter, which is what keeps a sync signal steady; on a missing packet it plays
one 1 ms slot of silence rather than stalling.

### FIZ encoders

Three magnetic rotary encoders on the lens read focus, iris and zoom:

- **Focus** — AS5048A, SPI bus
- **Zoom** — AS5048A, SPI1 bus
- **Iris** — AS5048B, I2C

(Iris moved to the I2C part because the Teensy 4.1's SPI2 pins are unusable —
they share the SD-card and memory pads. The AS5048B is the same 14-bit sensor
over I2C, so no resolution is lost.)

Focus and zoom are sampled in a 1 kHz hardware-timer interrupt. Iris is polled
over I2C from `loop()`, deliberately **not** from the interrupt — an
`IntervalTimer` firing mid-`Wire`-transfer can wedge the I2C bus. When a FreeD
frame is due, `loop()` refreshes all three encoders right before building the
packet (**read-before-send**), so the data is as fresh as possible by the time
Unreal decodes it rather than up to 1 ms old.

The AS5048 parts are single-turn absolute encoders, but a focus pull through a
user-chosen gear ratio can spin the encoder past 360°. A **multi-turn unwrap**
(an int32 running total accumulated in the sampling ISR) tracks true position
across revolutions. This holds from calibration start until power is lost — a
single-turn sensor cannot know about turns made while it was off, so a fresh
3-point calibration is required each time a lens is mounted, the same as real
lens-control systems.

**Calibration** is a guided 3-point web-UI wizard (min / mid / max per axis).
The operator toggles which of focus/iris/zoom a given lens has; each point can
take an optional typed physical value (mm for focus/zoom, fstop for iris) for
a status-page sanity check only — FreeD itself only ever carries the 0–100%
normalized position. Each axis gets two independent segment scale factors
(min–mid, mid–max) because lens rings aren't linear across their throw.

### Web UI and fleet discovery

Runs in `loop()`, non-blocking. Serves the settings and monitor pages and a
read-only fleet view: devices auto-discover each other on the LAN and the UI
lists discovered peers (name / ip / camera id) with a camera-id conflict
warning. Peer names come off an unauthenticated LAN broadcast, so both the
server-side JSON and the client-side rendering treat them as untrusted.

### Status LEDs and reset

Pin 4 shows genlock/PTP lock, pin 5 shows FreeD streaming active — two
independent signals. The reset button (pin 6, active-low) must be held 5 s, so
a bump on set can't trigger a reboot mid-shoot. An **unexpected-reboot flag**
(`bootflag.bin`) distinguishes a crash or power loss from a deliberate restart
and surfaces it as a web-UI banner until acknowledged — the visible
alternative to a watchdog that silently recovers (which is deliberately not
used; see ROADMAP).

### Settings persistence

Every web-settable value survives a reboot: network config and camera id
(`network.bin`), FreeD frame rate (same struct), AES67 direction/destination
(`aes67.bin`), genlock fps (`genlock.bin`), and the filter on/off switch
(`filter.bin`). Each file carries a small header (magic, layout version,
payload size) and is validated on read, so a file from an older struct layout
is ignored rather than read as garbage.

## Pin map

All interfaces, checked for overlaps against each other.

| Interface | Bus | Pins |
|---|---|---|
| Focus encoder (AS5048A) | SPI | sck 13, mosi 11, miso 12, cs 10 |
| Zoom encoder (AS5048A) | SPI1 | sck 27, mosi 26, miso 1, cs 0 |
| Iris encoder (AS5048B) | I2C | sda 18, scl 19 |
| Genlock output | — | bit0 2, bit1 3 |
| Status LEDs | — | genlock/PTP lock 4, FreeD streaming 5 |
| Audio (I2S) | — | mclk 23, bclk 21, lrclk 20, DAC data 7, ADC data 8 |
| Factory reset button | — | pin 6 (hold 5 s to confirm) |
| Ethernet | — | native Teensy 4.1 RJ45, fixed by the board, used directly by PTP |

Pin 13 doubles as the Teensy on-board LED (external LEDs are used instead, so
this is harmless), and that is the only overlap on the whole board. Everything
else is a clean 1:1 assignment, and nothing sits on the SD-card/memory pads.

## Open items

Current unknowns, all pending real hardware:

- **Genlock clock-servo rate limit.** `genlock.cpp` passes 800 ns/s at 4 Hz;
  the source paper measured 375 ns/s at 10 Hz against a real camera and showed
  the stable range narrows at lower update rates. If a camera drops genlock
  intermittently while the UI still says synced, this is the first thing to
  change. See [GENLOCK-TIMING.md](GENLOCK-TIMING.md).
- **LTC dropout tolerance (freewheel)** on the receiving end is not verified
  against any real downstream reader yet — only the theory checks out.
- **Nothing is tested on real hardware.** The pin map and both the SPI and the
  new I2C encoder path are verified against source and by compiling, not on a
  bench. The I2C-from-`loop()` decision specifically was checked by reading the
  `Wire` driver source but not proven on a real bus.
- **BOM changed** (2× AS5048A + 1× AS5048B instead of 3× AS5048A; PCM5102A +
  PCM1808 instead of the PJRC audio shield) and is not ordered yet.
- **Multi-turn unwrap** is verified by compiling and by hand-tracing the
  arithmetic, not against a real gear pair on a bench. The accumulator is a
  plain int32 with no overflow guard — fine for any realistic single
  powered-on session, and a fresh calibration resets the reference anyway.

See [CREDITS.md](CREDITS.md) for the upstream projects this builds on.
