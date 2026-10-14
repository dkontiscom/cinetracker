# Carrier board changelog

No revision has been manufactured yet.

## v0.62 (in review, started 2026-10-13): fixes from the desk review series

Fixes land here as each review finishes; v0.62 is not final until the series and the re-checks are done.

- **Review A1 (carrier footprints, pins, placement rotations):**
  - **J1 BNC footprint (blocker):** the lead row and the post row were swapped. Kinghelm drawing p. 1: posts 7.5 mm behind the body front, leads 12.55 mm. The real part could only have gone in facing into the board. Pads moved (library and board), body courtyard and position unchanged, genlock output re-routed to the new centre pin.
  - **D5** moved 0.4 mm to the right so its new courtyard clears the J1 body; its tracks re-routed.
  - **CPL in JLC's orientation:** U1 90, U7 180, U4–U6 180, U2/U3 270, J2 0, J5/J6 270, J7–J9 0 at the row centre. Uploaded with KiCad rotations, U1/U7 would have lost V− (genlock dead) and U4–U6 their ground. See `ordering.md`.
  - **D3–D5 footprint (TI DPY0002A):** now an SMD part with a courtyard and a fab outline. The project's DRC checks for missing courtyards and footprint type are switched on (they pass).
  - Checks: ERC 0; DRC 0 violations / 0 unconnected / 0 schematic parity; netlist compare 100 parts / 65 nets / 299 pads; Teensy pins 0 mismatches. BOM unchanged.
- **Review B (every IC pin against its datasheet and the firmware):** codecs, genlock op-amps, ESD parts, encoder interfaces, LEDs, button and every Teensy pin agree with their datasheets and with the firmware (I²S: Teensy master, 12.288 MHz MCLK, 48 kHz, I²S format; both codecs strapped to match).
  - **C2 and C34** (U1 / U7 OPA356 bypass) are now 10 µF 0805 (C15850, already on the BOM) instead of 100 nF: TI names 10 µF ceramic as the minimum (SBOS212A p. 10). Same footprint, no layout change. Components +$0.49 per 5-board order.
  - Genlock schematic note now points to `simulation/README.md` (the old path didn't exist).
  - Design notes: a dead short on one encoder cable can reboot the CineTracker once before the fuse trips (suspicion until tested).
- **Review C (ratings, currents, misuse, back-power, ESD, chassis, start-up):** no part is over its rating in normal use. Changes:
  - **Two unprotected Teensy-pin tracks moved out of the panel-connector pin fields** (layout). RESET_BUTTON ran on F.Cu between the J5 and J6 pins, 0.26 mm from the LTC pins, and SPI1_MISO looped round the J3 pins under the M8 body. Both have no clamp on the Teensy side, and TI's TVS layout rule says to keep such tracks away. Now RESET_BUTTON runs on B.Cu under the J9 row (2 vias), SPI1_MISO on B.Cu below the RS row (2 vias). No Teensy-side track or via is within 1.5 mm of any J1–J6 pad; SPI1_MISO is about 19 mm shorter.
  - `encoder-cables.md`: check every signal pin for shorts, not only +3.3 V (a shorted wire pulls about 60 mA from a Teensy pin); power encoders only from the box, no 5 V logic.
  - Design notes: D6 doesn't cover a reversed or hot-plugged bench supply (section 2), bench supply rule in `ordering.md`; the Teensy 4.1 has used the NCV8186 regulator since March 2022 (same 6.0 V limit); LTC ports take ±9 V (section 4); ESD residual through the 33 Ω (section 5, suspicion); the unprotected-track rule (section 7).
  - Bench checks added: PoE start-up with the real stack (scope 5 V and 3.3 V), J5/J6 shell-to-tab continuity.
  - Checks: ERC 0; DRC 0 violations / 0 unconnected / 0 schematic parity; `tools/verify_carrier.py` ALL OK. BOM unchanged.
- **Review D1 (JLC limits, Gerbers in another viewer, clean clone, fab reproducibility):** the committed Gerbers, drills, BOM, CPL, PDFs and fit checks match the board, and a clean clone opens with stock KiCad 10. Changes:
  - **JLC limits as DRC rules:** new `cinetracker-carrier.kicad_dru` (JLC capabilities read 2026-10-13, sources in the file); Board Setup minimum clearance 0 → 0.09 mm and solder mask web 0 → 0.1 mm. Before, a 0.05 mm track passed DRC.
  - **Three GND vias** sat in or touching the R5 and R14 pads (paste could wick into the via); now one via between the pads and one above each (layout).
  - **Thermal reliefs on THT pads** for JLC's hand soldering (all four zones; SMD pads and vias stay solid; J5 MP, J8.2, J9.1 stay solid, no room for two spokes).
  - **Silkscreen:** J2–J6 pin-1 marks and the back URL text up to JLC's 1.0 mm / 0.15 mm (board and project footprints).
  - **Fab script in the repo:** `hardware/tools/make_fab.py` rebuilds or checks every fab output (export settings, JLC rotation table, BOM/CPL rules). The board now stores the plot settings the fab files use, and a title block with the revision.
  - `ordering.md`: green mask, copper weights and stack-up stated; "order number at a specified location" no longer exists at JLC (mark: none); order remark for the parts on the edge; CPL rule worded exactly.
  - Checks: ERC 0; DRC 0 / 0 / 0 with the JLC rules; `tools/verify_carrier.py` ALL OK; `make_fab.py --check` same CSVs. BOM and CPL unchanged.
- **Review D2 (stock, alternates, licences):** every part can be bought for 5 boards on 2026-10-13. No copper change.
  - `ordering.md`: stock updated. J3/J4 (DS869FPR0470-6P) has 53, single source: buy them into the JLC parts library early, and note that a batch over 26 boards needs another source. Drop-in alternates named for OPA356 and the 0.1 % 5.6k. Teensy 4.1 must be the version **with** the Ethernet chip (not `_NE`); headers are no longer included.
  - `encoder-cables.md`: the AS5048 adapter boards are out of stock / back-ordered to December 2026; order early.
  - Licences: each project footprint now names the vendor drawing it follows (description field); `hardware/LICENSE` is CERN's verbatim text (was SPDX's copy, 2 words different); board title block carries the copyright and licence line like the schematics; `CREDITS.md` has a hardware paragraph (paper DOI, KiCad library licence, renamed power symbols); simulation README: rename TI's `OPA356.LIB`, never commit TI's files.

- **Review E (spacing, panels, panel thickness, stack, hand assembly):** no board change. All carrier spacing passes IPC-2221B (0–15 V); planes 0.5 mm from the edges; only GND copper around MH1–MH4. New `hardware/documentation/assembly-order.md` and `enclosure-requirements.md` (panel cut-outs and drawings, lid height, ≥ 5 mm standoffs, reset plunger with its own spring, straight cordsets on J5/J6).
- **Copyright and licence notices (2026-10-14):** copyright holder now "2026 Dimitris Kontis and CineTracker contributors" in the schematic and board title blocks and the READMEs; every Python script under `hardware/` carries a two-line SPDX header (CERN-OHL-S-2.0). CERN-OHL-S user guide rule 2.2.6.
## v0.61 (2026-10-11): KiCad 10, no copper change

- **Revision:** v0.61, together with the PoE board v0.61. Silkscreen, title blocks and fab file names say v0.61.
- **KiCad 10.0.7:** schematics, board and project libraries are in the KiCad 10 format now (were KiCad 7.0.11). KiCad 7 can't open them anymore.
- **First ERC** (KiCad 7's command line had none), 5 errors and 3 warnings, all fixed, none was a wiring fault:
  - J8 pin 3 and J9 pin 15 are both Teensy 3V3 pins (same regulator) and were both power outputs. J9 pin 15 is passive now.
  - +5V and GND come in on J7, +3V3A and +5VA through FB1 and FB2, so nothing drove them as far as ERC knows. They have PWR_FLAGs now, on the Power sheet (no footprint, nothing on the board).
  - The project's copies of the Device:LED and Connector:Conn_Coaxial symbols were refreshed from the KiCad 10 library (same pins and graphics).
- **Board brought in line with the schematic and the KiCad 10 libraries,** like *Update PCB from Schematic* plus *Update Footprints from Library*:
  - Stock footprints take the KiCad 10 library graphics (silkscreen, courtyard, fab) and 3D models. Pads are identical; the 1.7 mm socket pads changed from oval to circle of the same size.
  - Symbol fields (LCSC, Manufacturer, MPN, Description) copied onto the footprints.
  - The 27 no-connect pads got their KiCad `unconnected-...` nets.
  - Zones refilled with KiCad 10 (fill area changed by under 0.4 mm² per layer).
- **Copper unchanged:** all 315 pads, 653 tracks, 103 vias and the outline compared equal with the KiCad 7 file. BOM and CPL came out identical.
- **Gerbers:** the inner layers are now `GNDPLANE.g1` and `PWRPLANE.g2` (KiCad 7 named them `.g2` and `.g3`), see `ordering.md`. Fab files renamed to `carrier-v0.61-*`; BOM and CPL content unchanged.
- **Checks (KiCad 10):** ERC 0; DRC 0 violations, 0 warnings, 0 unconnected, 0 schematic parity issues, 0 library mismatches; independent netlist compare 100 parts / 65 nets / 299 pads; Teensy pins 0 mismatches. New `tools/verify_carrier.py` runs all of them.

## v0.60 (2026-10-11)

Numbering from here: v0.60, v0.61, v0.62, ... (v0.60 is what was called "v0.6" before).

- **Genlock level resistors:** R1, R2, R4 (2k), R3 (1k), R5 (5.1k) and R15 (5.6k) are now 0.1 % thin film (YAGEO RT0805B, Panasonic ERA6AEB562V). Same 0805 footprints, same values; the schematic value text shows 0.1 %.
- **MISO pull-ups:** R27 and R28, 10 k 0805 to +3V3 on SPI_MISO and SPI1_MISO, so an unplugged focus or zoom encoder reads 0xFFFF (error flag set) instead of a floating line. R27 at (28.5, 19.25), R28 at (6.75, 36.0); three new vias, five short tracks, nothing else moved.
- **Connector courtyards:** the BNC and M8 footprints (board and `lib/`) now have courtyards from the vendor drawings. The first placement of R28 landed under J3's body, which DRC could not see before; that is why they were added. No existing part conflicts with them.
- **Fab files:** regenerated as v0.60. BOM: 39 lines (was 37), 22 extended types (was 18). CPL rows of existing parts are unchanged; R27 and R28 added.
- **Checks:** schematic vs board 100 parts, 0 mismatches; DRC 0 violations, 0 unconnected; Teensy pin check 0 mismatches.

## v0.5e update (2026-10-10, same revision, no copper changes)

- **Licence and source location:** CERN-OHL-S-2.0 text on the bottom silkscreen with the repository address; copyright line in every schematic title block. Gerbers and schematic PDF regenerated.
- **Decisions:** J7 link is a long-pin header on the PoE board side, J7 itself unchanged. From v0.6 the six genlock level resistors (R1–R5, R15) become 0.1 % thin film. See `open-issues.md`.

## v0.5e (2026-10-10): provisions for the PoE board

- **Standoff holes:** MH5 at (77.0, 48.1) and MH6 at (10.0, 48.5), both M2.5 NPTH for the PoE board v0.1 standoffs.
  - Each has a 3.2 mm keep-out with no tracks, vias or fill on any layer.
  - Both spots were already free on v0.5d, so no copper moved.
  - Use nylon standoffs: the holes sit in the PoE board's isolation band.
- **Height check:** under the PoE board (underside about 18.9 mm above the carrier), the tallest carrier parts are the panel connectors (about 13 mm) and J7 (8.5 mm).
- **J7 unchanged.** A standard header on the PoE board cannot reach it, and the long-pin header vs cable choice is open. See `../../poe/documentation/open-issues.md`.
- **Fab files:** regenerated as v0.5e. The BOM and placement are the same as v0.5d.

## v0.5d (2026-10-10): Teensy pin fix and move to a KiCad project

- **Critical fix: the Teensy 4.1 socket rows.** From v0.3 to v0.5c the GND/0/1 row was placed where the Teensy's VIN row lands, and the VIN-row pin order was entered as VIN, 23, 22 … instead of VIN, GND, 3.3V, 23, 22 ….
  - With the Teensy plugged in components up, it would have got **+5 V on its GND pin and GND on its VIN pin**, with I2S, I²C and SPI on the wrong pins.
  - The earlier "firmware ↔ board 22/22 pins match" checks compared against that same wrong list, so they could not catch it.
  - **Fix:** J8 (top row) is now the VIN row and J9 (bottom row) the GND/0/1 row, checked against PJRC's pinout card and dimension drawing. All Teensy nets are re-routed. The Teensy keeps the same position and orientation (USB on the left), and all connectors stay where they were.
  - `tools/check_teensy_pins.py` now checks this independently.
- **KiCad project:** the design is now a normal KiCad 7 project with a hierarchical, editable schematic and project symbol and footprint libraries. Until v0.5c the board was produced by a Python generator, and the PDF schematics were drawings made from the board.
- **References renamed** to valid KiCad annotation:
  - J_TL → J9, J_TR → J8.
  - R3b → R13, R4b → R14, R5a → R15.
- **BNC posts:** the BNC mounting posts are pad 2 (GND), which matches the standard coax symbol.
- **Firmware:** the trailing tri-level edge is a single GPIO store, the broad pulse ends at 2112 T at 24/25/50p, and dependency versions are pinned. See the repository history.

## v0.5c (2026-10-09): genlock meets ST 274 at 50/60p too

- **New output stage:** ladder → OPA356 buffer → unity-gain Sallen-Key low-pass (150 Ω / 68 pF 1 % C0G, Q = 0.5) → OPA356 line driver.
- **Rise time:** about 35.2 ns simulated on all three edges, which is inside both the 24/25/30p and 50/60p windows. The v0.5b stage gave 44–53 ns and failed 50/60p.
- **Offset resistors:** 5k6 / 5k1 (the levels go to 0.43 / 1.03 / 1.63 V). CS1 is removed.

## v0.5b (2026-10-09): genlock −300 mV level fix

- The single-supply OPA365 could not reach 0 V while sinking the about 4 mA load current, so the −300 mV level came out at −261 mV (simulated).
- Adding an offset (3k3 to +3V3A ∥ 15k to GND) fixed it, and CS1 22 pF shaped the edges.

## v0.5 (2026-10-01): JLC-assembled connectors, 5 V input

- **J7:** a 1×4 5 V input header for a PoE board (or a bench supply), feeding Teensy VIN, with SMAJ5.0A + 47 µF at the entry.
- **Connectors:** all M8 sockets are female right-angle parts that JLC stocks (CAZN, dosinconn), and J1 is a 75 Ω BNC (Kinghelm KH-BNC75-3511). JLC assembles all connectors.
- **Encoder supplies:** one PTC per encoder supply.
- **I²C:** pull-ups changed from 4.7 k to 2 k.
- **ADC:** PCM1808 VINR goes to GND through 1 µF.
- **LTC connectors:** pin 1 tied to GND.

## v0.4 (2026-09-28)

- **M8 footprints:** rebuilt from the vendor drawings, with mounting slots added.
- **Parts:**
  - Charge-pump capacitor changed to X7R.
  - LEDs changed for visibility behind light pipes.
  - TPD1E10B09 on TI's DPY0002A land pattern.
  - Reset switch pads all on net.
  - +5VA filtered supply for the PCM1808.
- **M8 pinouts:** follow the sensor convention.

## v0.3 (2026-09-28)

- **Net names:** nets are named after the firmware identifiers.
- **Protection:** every external line runs through a TVS and a 33 Ω series resistor.
- **LTC:** level-setting resistors.
- **Status UI:** status LEDs and reset button placed for lid light pipes.
- **Board size:** 64 mm deep.
- The PCM5102A and PCM1808 have been native on the board since v0.2.
