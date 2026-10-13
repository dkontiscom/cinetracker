# PoE board changelog

No revision has been manufactured yet.

## v0.62 (in review, started 2026-10-13): fixes from the desk review series

Fixes land here as each review finishes; v0.62 is not final until the series and the re-checks are done.

- **Review A2 (PoE footprints, pins, placement rotations):** every footprint, pin mapping and CPL rotation checked against the datasheets and JLC's library footprints. All correct, CPL needs no change.
  - **J3 bottom silkscreen:** the labels read "+5V +5V GND GND" over pins 4–1, i.e. reversed. Now "GND GND +5V +5V", so "+5V" sits over pins 1 and 2.
  - `ordering.md`: D4 and C6 added to the placement preview checks; D1/D2 "+" corner named.
  - `open-issues.md`: M12 sample check now also measures the housing (nut, thread, O-ring, shoulder) and checks the X-shaped plate on pin 9 against the SP_45 track; LCSC's "Brass" is the contact material, not the shell.
  - Silkscreen, title block and fab files v0.62. Checks: `tools/verify_poe.py` ALL OK.
- **Review B (pins against datasheets):** PS1, bridges, TVS, magnetics and the PHY-side centre taps match SDAPO's typical circuit and TI's DP83825I circuit. Design notes: the SMAJ58A clamps at 93.6 V, above the DP9900's 80 V surge rating (SDAPO's own part choice; normal PoE is far below it).
- **Review C (ratings, currents, misuse, ESD, start-up):** no board change. Docs:
  - Hipot only by the 802.3 impulse test (1500 V 10/700 µs). 1500 Vrms or 2250 V DC puts 2.07–2.20 kV on C5, rated 2 kV.
  - Bench tests of the PoE board alone need a ≥ 100 mA load (47 Ω 1 W on the R6 footprint); without it the DP9900 is below its minimum load.
  - C6 is a 2000 h / 105 °C part: measure its temperature next to PS1. Common-mode current path with pin 9 open written down. Start-up with the real stack added to the bench checks.
- **Review D1 (JLC limits, Gerbers, clean clone, fab reproducibility):**
  - **JLC limits as DRC rules** appended to `cinetracker-poe.kicad_dru` (sources in the file); solder mask web 0.1 mm.
  - **Silkscreen:** the PRIMARY / ISOLATED labels, the D1/D2 "+" and "~" marks and the back texts up to JLC's 1.0 mm / 0.15 mm (board and MBS footprint); the back URL now on two lines; the J1 outline moved 0.05 mm off the M12 post holes (≥ 0.15 mm).
  - **J2 and J3** (hand-fitted) carry "exclude from position files" in the schematic and on the board, so no BOM/CPL route can put J2 into the JLC order.
  - Plot settings and a title block with the revision stored in the board; fab files rebuilt by `hardware/tools/make_fab.py`.
  - **Open, not changed yet:** primary copper comes within 0.54 mm of the panel edge (y = 64). See `open-issues.md`, "Before ordering".
  - Checks: ERC 0; DRC 0 / 0 / 0 with the JLC rules; `tools/verify_poe.py` ALL OK. BOM and CPL unchanged.

## v0.61 (2026-10-11): KiCad 10, track moved off the M12 housing pin

- **Revision:** v0.61, together with the carrier v0.61. Silkscreen, title blocks and fab file names say v0.61.
- **M12 pin 9:** J1 pin 9 is the housing contact, and the housing sits on the chassis (= isolated GND) once the connector is panel-mounted. MDI_TX_P ran from J1 pin 1 through the middle of the pin circle, 0.45 mm from pin 9.
  - MDI_TX_P now goes round the outside of the pin circle, between the two J1 posts, to its old run along y = 52.08 to T1.
  - To free that path, MDI_RX_N moved to B.Cu next to MDI_RX_P (both pass between the J1 posts), with one new via next to T1 pin 16. MDI_RX_P moved 0.3 mm down to make room.
  - New DRC rule "M12 housing contact": nothing within 1.53 mm of pin 9, the connector's own pin spacing. Nearest track now 1.79 mm. On the v0.60 route the rule flags the old track (0.45 mm), so it is active.
  - Isolation gaps unchanged: minimum 2.7 mm (C5), then 3.2 mm.
- **KiCad 10.0.7:** schematic, board and project libraries are in the KiCad 10 format now (were KiCad 7.0.11). KiCad 7 can't open them anymore.
- **First ERC:** 1 warning (the project's copy of Device:LED differed from the KiCad 10 library in a simulation property), fixed. 0 now.
- **Board brought in line with the schematic and the KiCad 10 libraries,** like *Update PCB from Schematic* plus *Update Footprints from Library*:
  - The schematic's local labels make root-sheet nets, which KiCad names with a leading "/": the board nets are renamed to match (VIN_P is /VIN_P, and so on for 22 nets). The Primary net class patterns in the project file follow, so the isolation rule still covers the same nets.
  - The 7 no-connect pads got their `unconnected-...` nets. The isolation rule leaves those out, same as no-net copper before (see the comment in `cinetracker-poe.kicad_dru`).
  - Stock footprints take the KiCad 10 library graphics and 3D models (pads identical; oval 1.35/1.7 mm pads on J2/J3 are circles of the same size now). Symbol fields copied onto the footprints.
  - Zones refilled with KiCad 10.
- **KiCad 10 move, copper:** before the pin 9 fix, all 91 pads, 186 tracks, 17 vias and the outline compared equal with the KiCad 7 file. BOM and CPL came out identical, and still are.
- **Checks (KiCad 10):** `tools/verify_poe.py` ALL OK: ERC 0; DRC 0 violations, 0 warnings, 0 unconnected, 0 schematic parity issues; 85 pads / 24 nets match; isolation minimum 2.7 mm (C5), then 3.2 mm; J1 pin 9: data pads 1.53 mm, nearest track 1.79 mm; J3 = carrier J7, J2 = Teensy Ethernet pins, H1/H2 = carrier MH5/MH6. The script uses kicad-cli on a temporary copy now.

## v0.60 (2026-10-11): renumbered

- Renumbered from v0.1 to v0.60 so the PoE board and the carrier carry the same revision. Iterations from here: v0.61, v0.62, ...
- No circuit, layout or BOM change. Silkscreen and title block say v0.60; fab files, schematic PDF and renders regenerated under the new name. The stack fit check now shows the carrier v0.60 with its new connector courtyards.
- Checks: `tools/verify_poe.py` ALL OK (85 pads / 24 nets match, DRC 0, isolation minimum 2.7 mm, J3 = carrier J7, J2 = Teensy Ethernet pins, H1/H2 = carrier MH5/MH6; H2 is 3.5 mm from the carrier BNC body).

## v0.1 update (2026-10-10, same revision, no copper changes)

- **Licence and source location:** CERN-OHL-S-2.0 text on the bottom silkscreen with the repository address; copyright line in the schematic title block. Gerbers and schematic PDF regenerated.
- **Decisions:** J3 to J7 by a long-pin header; M12 shield pin 9 left open; M12 samples after funding. See `open-issues.md`.

## v0.1 (2026-10-10): first design

- **Connector:** M12 X-coded 8-pin socket (CAZN, C7423431). All four pairs come in, so the board takes Mode A or Mode B power from any 802.3af/at PSE.
- **Power:** SDAPO DP9900LP-5V PD module (isolated 5 V, 1.4 A), with two MB6S bridges and an SMAJ58A TVS, per the module's typical circuit.
- **Magnetics:** CND-tek B1608S PoE magnetics, and Bob Smith terminations per TI SLUA454.
- **Board-to-board links:** the Teensy 4.1 Ethernet pins connect through a 2×3 2.0 mm socket on the bottom (PJRC kit pin order). 5 V goes to carrier J7 through a 1×4 2.54 mm footprint; the link type is still open.
- **Isolation:** a 3.0 mm band, enforced by a custom DRC rule. The smallest primary-to-isolated gap is 2.7 mm, across C5 (2 kV).
- **Standoffs:** two M2.5 nylon standoff holes, on spots that are free on carrier v0.5d.
- **Checks:**
  - Schematic vs board: 0 mismatches.
  - DRC: 0.
  - J3 lines up with carrier J7, and J2 with the Teensy Ethernet pins.
