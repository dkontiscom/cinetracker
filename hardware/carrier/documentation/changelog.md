# Carrier board changelog

No revision has been manufactured yet.

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
