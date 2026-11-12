# CineTracker

Open hardware and firmware for tracking a cinema lens (focus, iris, zoom) and
sending that data to Unreal Engine over the FreeD protocol, for virtual
production. Runs on a Teensy 4.1.

On top of FIZ tracking, the same box does PTP time sync, genlock generation
locked to that PTP clock, and AES67 audio carrying LTC timecode to and from
nearby devices (cameras, recorders). The idea is one small device on the
camera and one cable — PoE Ethernet — carrying lens data, sync and timecode.

The headline part is **extended FreeD**: the same FreeD data plus the PTP time
the encoders were read, timecode and a sequence number, so a receiver knows
exactly *when* the lens was where it says it was, not just when the packet
arrived. Draft spec in [EXTENDED-FREED.md](EXTENDED-FREED.md); a matching
Unreal LiveLink plugin is planned.

This is a rewrite of an earlier RP2040 version of the same idea.

> **Status: early rewrite, not working yet — don't use this for anything
> real.** The firmware builds but nothing has run on real hardware. The boards
> are designed but not built. See [ROADMAP.md](ROADMAP.md) for where things
> stand.

## How it fits together

```
  lens encoders (3× AS5048, M8 cables)
          |
  carrier board  — genlock out (BNC, tri-level), LTC in/out (M8),
          |        status LEDs, reset
  Teensy 4.1     — firmware: encoders, FreeD, PTP, genlock, AES67, web UI
          |
  PoE board      — M12 X-coded Ethernet, isolated 5 V from PoE
          |
  network (PoE switch, PTP grandmaster) → Unreal Engine, other devices
```

## Repo layout

| Path | What |
|---|---|
| `src/`, `include/` | Firmware |
| `lib/t41-ptp`, `lib/t41-tri-sync` | Vendored PTP client and genlock pulse generator (see [CREDITS.md](CREDITS.md) and [ARCHITECTURE.md](ARCHITECTURE.md) for why vendored) |
| `hardware/carrier/` | Carrier board — KiCad 10 project, fab files, docs |
| `hardware/poe/` | PoE board — KiCad 10 project, fab files, docs |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | How the firmware works, subsystem by subsystem |
| [`DEVLOG.md`](DEVLOG.md) | Development history: design decisions and the reasoning behind them |
| [`ROADMAP.md`](ROADMAP.md) | Milestones and status |
| [`EXTENDED-FREED.md`](EXTENDED-FREED.md) | Extended FreeD packet spec (draft) |
| [`GENLOCK-TIMING.md`](GENLOCK-TIMING.md) | Genlock timing, how to measure it, planned rework |
| [`CREDITS.md`](CREDITS.md) | The projects this builds on |

## Hardware status

Details in [hardware/README.md](hardware/README.md).

- **Carrier** — v0.61, designed in KiCad 10, not built. Do **not** build v0.3
  to v0.5c: they had the Teensy socket rows swapped.
- **PoE** — v0.61, designed in KiCad 10, not built.

Revisions go v0.60, v0.61, v0.62. Before anything is ordered: owner review,
then a review by a professional PCB designer once there is funding.

## Building the firmware

Needs [PlatformIO](https://platformio.io). Board target is `teensy41`.
`platformio.ini` pins the platform and library versions deliberately — change
them there on purpose.

```
pio run
```

This builds `firmware.hex` under `.pio/build/teensy41/`. Flash it with the
official Teensy Loader, the same as any other Teensy 4.1 project; there is no
special install step.

The hardware is **KiCad 10** (10.0.7 used); older KiCad won't open the files.
Open the `.kicad_pro` in each board folder. The schematics are the source; the
fab files in each `fab/` folder are generated from them. Each board folder's
`tools/` has the check scripts (ERC, DRC, schematic-vs-board and more).

## License

**Firmware** (everything outside `hardware/`): **AGPL-3.0-or-later**, see
[LICENSE](LICENSE). This follows from depending on QNEthernet for the
PTP/genlock/AES67 timing stack, which is AGPL itself, so the whole firmware
ends up AGPL too.

**Hardware** (everything under `hardware/`): **CERN-OHL-S-2.0**, strongly
reciprocal, copyright CineTracker contributors, see
[hardware/LICENSE](hardware/LICENSE). The boards carry the source location
(`github.com/dkontiscom/cinetracker`) on their silkscreen.

The extended FreeD spec text ([EXTENDED-FREED.md](EXTENDED-FREED.md)) is part
of this repo and covered by the same AGPL for now. **Open question:** whether
the spec should get its own, more permissive licence so closed receivers can
implement it freely.
