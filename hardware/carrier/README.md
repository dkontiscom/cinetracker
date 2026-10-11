# CineTracker carrier board

The carrier holds a Teensy 4.1 and adds everything a camera-side CineTracker needs:
- tri-level sync (genlock) output on BNC;
- LTC timecode in and out on M8 sockets, through a PCM5102A DAC and a PCM1808 ADC;
- three lens-encoder inputs on M8: focus and zoom on SPI (AS5048A), iris on I²C (AS5048B);
- two status LEDs and a reset button;
- a 5 V input header for the PoE board, which sits on top of the Teensy.

**Revision: v0.61 (2026-10-11).** Designed, not yet built or ordered. Next: owner review, then (with funding) a review by a professional PCB designer, then the order. The next revision is v0.62.

The PoE board ([../poe](../poe)) sits on top of the Teensy. It plugs onto the Teensy Ethernet pins and carrier J7, and two M2.5 nylon standoffs (MH5, MH6) hold it.

> **Do not build any earlier revision (v0.3 to v0.5c).** They wired the two Teensy socket rows wrong and would put +5 V on the Teensy's GND pin. v0.5d fixed this. See [documentation/changelog.md](documentation/changelog.md).

## Board

| | |
|---|---|
| Size | 80 × 64 mm, 1.6 mm, 4 layers: signal / GND plane / +3V3 plane / signal |
| Parts | 94 placed parts, 4 mounting holes (M3), 2 PoE-board standoff holes (M2.5, MH5 and MH6). All JLCPCB stock: 39 BOM lines, 22 of them JLC "extended" parts |
| Teensy 4.1 | Plugs in components up, USB end on the left. J8 is the VIN row (top) and J9 the GND/0/1 row (bottom). 8.5 mm tall sockets, because codecs sit underneath |
| Power | 5 V into J7 (from the PoE board or a bench supply) goes to Teensy VIN. **Cut the Teensy's VUSB–VIN pads.** 3.3 V comes from the Teensy regulator |
| Connectors | All panel connectors on two edges. Encoders: J3 focus, J4 zoom (M8 6-pin), J2 iris (M8 4-pin). Sync and timecode: J1 BNC 75 Ω, J5 LTC out, J6 LTC in (M8 3-pin). All M8 sockets are female |

## Files

| Path | What |
|---|---|
| `cinetracker-carrier.kicad_pro` | KiCad 10 project (10.0.7). Open this |
| `cinetracker-carrier.kicad_sch` + `power`, `genlock`, `audio`, `encoders`, `ui.kicad_sch` | Hierarchical schematic, the design source |
| `cinetracker-carrier.kicad_pcb` | Routed board, linked to the schematic |
| `lib/` | Project symbols (`cinetracker.kicad_sym`) and footprints (`cinetracker.pretty`): Teensy rows, PCM1808, TPD4E1U06, M8 sockets, 75 Ω BNC, TI DPY0002A |
| `fab/` | JLCPCB outputs for v0.61: Gerbers + drill, BOM, CPL, full BOM with prices, 1:1 fit-check PDF |
| `documentation/` | Schematic PDF, design notes, ordering, changelog, open issues |
| `documentation/encoder-cables.md` | How to build the three encoder cables (M8 cordset to the ams AS5048 adapter boards) |
| `simulation/` | ngspice simulation of the genlock output stage |
| `tools/verify_carrier.py` | Runs all the checks below: ERC, DRC with schematic parity, an independent netlist compare, the Teensy pin check |
| `tools/check_teensy_pins.py` | Checks the Teensy socket nets against PJRC's pinout card and the firmware pin numbers |

## Checks done on v0.61

All with KiCad 10.0.7, `tools/verify_carrier.py` runs them.

- **ERC:** 0 errors, 0 warnings.
- **Schematic vs board:** KiCad's schematic parity check (what *Update PCB from Schematic* compares: parts, footprints, values, fields, nets) finds 0 issues. An independent compare of the exported netlist agrees: 100 parts (including the six holes), 65 nets and 299 pad connections identical.
- **DRC:** 0 unconnected, 0 violations, 0 warnings, 0 footprint-library mismatches against the KiCad 10 libraries. Since v0.60 the BNC and M8 footprints have courtyards drawn from the vendor drawings, so DRC also catches parts placed under a connector body.
- **Teensy pins:** `tools/check_teensy_pins.py` passes with 0 mismatches. It compares against PJRC's pinout card and the firmware's pin defines.
- **Genlock:** SPICE-simulated, see `simulation/`. Not yet measured on hardware.

The schematic was generated from the routed board's netlist when the design moved into KiCad (v0.5d). From now on the schematic is the source: edit it, then *Update PCB from Schematic*.

Licence: the hardware (everything under `hardware/`) is **CERN-OHL-S-2.0**, copyright CineTracker contributors, see [../LICENSE](../LICENSE). Source location: https://github.com/dkontiscom/cinetracker. The firmware is AGPL-3.0-or-later (see the repository root).
