# PoE board: open issues

Updated 2026-10-13 for v0.62 (in review).

## Decisions

| # | Item | Decision |
|---|---|---|
| 1 | **J3 ↔ carrier J7 power link.** The gap is about 10.4 mm, and a standard 2.54 mm header cannot bridge it (see [design-notes.md](design-notes.md) section 2) | **Decided 2026-10-10: A**, a long-pin 1×4 male header, hand-fitted and cut to the measured length. J7 stays as it is. Still to do: measure the stack, pick the header part |
| 2 | Hardware licence for the repository | **Decided 2026-10-10:** CERN-OHL-S-2.0, copyright CineTracker contributors |
| 3 | M12 shield (pin 9) | **Decided 2026-10-10:** left open (no copper). Its housing sits on the chassis, so since v0.61 a DRC rule keeps everything ≥ 1.53 mm from it |
| 4 | Buying M12 samples and a cordset | **Decided 2026-10-10:** after funding. Until then the M12 pin mapping stays a suspicion |

## Check with samples before ordering

- [ ] **M12 sample:** buy 2–3 of C7423431 plus a branded RJ45-to-M12 X-coded cordset.
  - Check that they mate.
  - Check the pin mapping with a continuity test. The footprint's pin numbers come from LCSC's footprint; the datasheet drawing shows no numbers.
  - Find the panel cut-out and where the panel sits relative to the board edge. Both boards put the edge at y = 64.
  - Check that the shell is metal. The CAZN datasheet lists the shell as "brass Ni / zinc alloy Ni / PA+GF" without saying which applies to C7423431, and its photo shows a black body. (LCSC's "Brass" attribute is the contact material, not the shell.) LCSC's product photos show it sold with a nickel-plated housing, hex nut and O-ring that the CAZN drawing doesn't show. The enclosure is metal with GND = chassis (decided 2026-10-11), so a metal shell bonds to the panel through its nut.
  - With a metal shell on the chassis, the 1500 V PoE isolation also depends on the insulation between the contacts and the shell inside the connector. CAZN publishes no withstand voltage for it; ask CAZN, or hipot a sample.
  - Measure the housing the drawing leaves out: nut across corners, panel thread, O-ring stack, and shoulder to board edge. The panel position relative to y = 64 can only come from this.
  - Pin 9 carries an X-shaped metal plate on the board-facing side (LCSC photo). Check whether it sits flush on the board and how far its arms reach. If flush beyond about 3.3 mm from pin 9, the F.Cu SP_45 track at (46.24, 55.35)–(43.79, 57.79) is under it with only solder mask between: move that segment to B.Cu or beyond about 4.2 mm.
- [ ] **DP9900LP pins:** check the pin size. The footprint uses a 1.0 mm drill (LCSC uses 0.8 mm) so that a 0.64 mm square pin also fits.
- [ ] **Stack heights:** measure the real stack, with the Teensy header plastic and a 2×3 2.0 mm male header on the Teensy.
- [ ] **Load current:** measure the 5 V current of the Teensy plus carrier.
  - Under 100 mA: fit R6. The DP9900LP minimum load is 100 mA.
  - The maximum is 1.4 A.
- [ ] **Switch detection:** check that your MikroTik (Mode B), UniFi and Cisco (Mode A) switches detect and power the board.

## Bench checks on the first boards

- [ ] 5 V regulation and ripple. The datasheet ripple figure for the DP9900LP-5V is "TBD".
- [ ] Link at 100 Mbit/s through the M12 cordset on each switch. Check the QNEthernet link status.
- [ ] Temperature of PS1 at the measured load, inside the enclosure.
- [ ] Insulation test, primary to secondary (hipot if available).
- [ ] **Interference:** check genlock and LTC for noise from the PoE converter. The PoE board sits above the carrier's analog section. Compare the scope traces with PoE power and with bench power on J7.

## Known limits

- **Spacing:**
  - Isolation is by spacing only: ≥ 2.6 mm by DRC rule, measured as 2.7 mm across C5's pads and ≥ 3.2 mm everywhere else.
  - The board has not been hipot-tested.
  - The DP9900LP's isolation is rated by impulse test (1500 V peak).
- **J1 pin 9 (shield) is left open.** The cable shield bonds only through the connector housing and the panel. Inside the connector pin 9 is 1.53 mm from the data pins; that, not the board, sets the PoE-to-chassis spacing there (see the M12 sample check above).
- **802.3af class:** the module does not document a class setting. The board shows up as the default class.
- **Schematic origin:** the schematic was generated from the routed board, then linked. It is correct and editable, but not hand-drawn.
