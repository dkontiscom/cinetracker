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
  - Primary copper came within 0.54 mm of the panel edge (y = 64): fixed below (D1-01, option A).
  - Checks: ERC 0; DRC 0 / 0 / 0 with the JLC rules; `tools/verify_poe.py` ALL OK. BOM and CPL unchanged.
- **Review D2 (stock, alternates, licences):**
  - **J1 (M12) footprint redrawn from the CAZN drawing** (8 pins at 27° on Ø6.26). The old pads were LCSC/EasyEDA library data, whose terms don't allow redistribution. Pads move by 0.005 mm at most; track ends follow.
  - **D1/D2 (MBS) footprint redrawn from MDD's suggested pad layout** (datasheet Rev 2024A2 p. 3): pads 1.84 × 1.2 at ±3.0 (were 2.0 × 1.1 at ±3.1, the LCSC copy). Track ends follow. D2's lower pads are now 1.30 mm from the panel edge (were 1.35, see the open item in `open-issues.md`).
  - MPN of D1/D2 now "MB6S-50MIL", JLC's current name for C2487 (same datasheet).
  - `ordering.md`: stock updated (M12 48, DP9900LP 461), Toponelec C12B-MAX-RM-8-SH as a second-source M12 to qualify with the samples, components $88.30 / subtotal $127.99 (2026-10-13; the old $91.22 didn't match BOM-full), Teensy with Ethernet chip.
  - `open-issues.md`: M12 pin numbers from the CAZN catalogue; 0.5 A contact rating; Toponelec sample (800 V rated impulse voltage) in the insulation test.
  - Checks: ERC 0; DRC 0 / 0 / 0; `tools/verify_poe.py` ALL OK.
- **D1-01, option A (decided 2026-10-14): primary side moved off the panel edge** (layout).
  - The board edge at y = 64 sits on the panel (chassis). Primary copper came within 0.54 mm of it (SP_45 along the bottom, CT_36, D2, BS_NODE, VIN_N; 34 items under 2.6 mm). Now every primary pad, track and via is ≥ 2.65 mm away, the same 2.6 mm the board keeps between primary and isolated copper.
  - Moved: R1/C1, R2/C2 (columns turned so CT pads face T1), R3/C3/R4/C4 (now four rows, references beside them), D1 and D2 (stacked 4.4 mm apart), D3 (0.2 mm up). Re-routed with Freerouting 2.1.0: BS1–BS4, BS_NODE, CT_12, CT_36, SP_45, SP_78, VIN_P, VIN_N. The isolated side and the MDI pairs are unchanged. 18 vias on the primary side.
  - New keepout rule area "panel edge (chassis): no copper" (y 61.4–64) holds it in DRC; `tools/verify_poe.py` prints the margin (check 4c).
  - The JLC "part to board edge" rule now only excuses C5 (D2, C1, C2, R4 are ≥ 2.5 mm from the edge now).
  - Isolation gaps unchanged (2.7 mm across C5, then 3.2 mm); nearest track to M12 pin 9 1.93 mm (was 1.79). Bob Smith node 63 mm of track (was about 93).
  - Checks: ERC 0; DRC 0 / 0 / 0 with all rules; `tools/verify_poe.py` ALL OK.
- **Review E (spacing, panels, panel thickness, stack, hand assembly):** no board change.
  - IPC-2221B check of the 0.3 mm primary clearance: fine under solder mask; 16 land-to-copper spots below the 0.5 mm for bare lands. Open owner decision (`open-issues.md`).
  - Design notes §2: PS1 top 28.5 ± 0.5 (the datasheet's 8.00 includes the spacer), M12 housing ≈ 37.8 (suspicion), lid ≥ 40.5 mm above the carrier (was "about 35"), M12 height tolerance ±1.2 mm.
  - New `hardware/documentation/`: assembly order of the stack (J3 soldered after measuring, next to PS1 with care) and enclosure requirements with panel drawings (M12 hole as a vertical slot, 1.5 mm sheet, separate end panel, ≥ 5 mm carrier standoffs, sprung reset plunger).

- **Copyright and licence notices (2026-10-14):** copyright holder now "2026 Dimitris Kontis and CineTracker contributors" in the schematic and board title blocks and the READMEs; every Python script under `hardware/` carries a two-line SPDX header (CERN-OHL-S-2.0). CERN-OHL-S user guide rule 2.2.6.
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
