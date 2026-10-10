# PoE board changelog

No revision has been manufactured yet.

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
