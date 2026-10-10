# CineTracker carrier board

The carrier holds a Teensy 4.1 and adds everything a camera-side CineTracker needs:
- tri-level sync (genlock) output on BNC;
- LTC timecode in and out on M8 sockets, through a PCM5102A DAC and a PCM1808 ADC;
- three lens-encoder inputs on M8: focus and zoom on SPI (AS5048A), iris on I²C (AS5048B);
- two status LEDs and a reset button;
- a 5 V input header for the PoE board, which sits on top of the Teensy.

**Revision: v0.5d (2026-10-10).** Designed, not yet built or ordered. Prototype orders are planned for v0.6.

> **Do not build any earlier revision (v0.3 to v0.5c).** They wired the two Teensy socket rows wrong and would put +5 V on the Teensy's GND pin. v0.5d fixes this. See [documentation/changelog.md](documentation/changelog.md).

## Board

| | |
|---|---|
| Size | 80 × 64 mm, 1.6 mm, 4 layers: signal / GND plane / +3V3 plane / signal |
| Parts | 92 placed parts, 4 mounting holes (M3). All JLCPCB stock: 37 BOM lines, 18 of them JLC "extended" parts |
| Teensy 4.1 | Plugs in components up, USB end on the left. J8 is the VIN row (top) and J9 the GND/0/1 row (bottom). 8.5 mm tall sockets, because codecs sit underneath |
| Power | 5 V into J7 (from the PoE board or a bench supply) goes to Teensy VIN. **Cut the Teensy's VUSB–VIN pads.** 3.3 V comes from the Teensy regulator |
| Connectors | All panel connectors on two edges. Encoders: J3 focus, J4 zoom (M8 6-pin), J2 iris (M8 4-pin). Sync and timecode: J1 BNC 75 Ω, J5 LTC out, J6 LTC in (M8 3-pin). All M8 sockets are female |

## Files

| Path | What |
|---|---|
| `cinetracker-carrier.kicad_pro` | KiCad 7 project. Open this |
| `cinetracker-carrier.kicad_sch` + `power`, `genlock`, `audio`, `encoders`, `ui.kicad_sch` | Hierarchical schematic, the design source |
| `cinetracker-carrier.kicad_pcb` | Routed board, linked to the schematic |
| `lib/` | Project symbols (`cinetracker.kicad_sym`) and footprints (`cinetracker.pretty`): Teensy rows, PCM1808, TPD4E1U06, M8 sockets, 75 Ω BNC, TI DPY0002A |
| `fab/` | JLCPCB outputs for v0.5d: Gerbers + drill, BOM, CPL, full BOM with prices, 1:1 fit-check PDF |
| `documentation/` | Schematic PDF, design notes, ordering, changelog, open issues |
| `simulation/` | ngspice simulation of the genlock output stage |
| `tools/check_teensy_pins.py` | Checks the Teensy socket nets against PJRC's pinout card and the firmware pin numbers |

## Checks done on v0.5d

- **Schematic vs board:** the netlist exported from the schematic matches the board, with 96 parts, 65 nets and 295 pads identical. Every footprint is linked to its symbol, with the same footprint and value.
- **DRC (KiCad 7):** 0 unconnected, 0 violations, 0 footprint-library mismatches.
- **Teensy pins:** `tools/check_teensy_pins.py` passes with 0 mismatches. It compares against PJRC's pinout card and the firmware's pin defines.
- **Genlock:** SPICE-simulated, see `simulation/`. Not yet measured on hardware.

The schematic was generated from the routed board's netlist when the design moved into KiCad (v0.5d). From now on the schematic is the source: edit it, then *Update PCB from Schematic*.

Licence: the firmware is AGPL-3.0-or-later (see the repository root). **The hardware licence is not decided yet.**
