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
  - Check the pin mapping with a continuity test. The footprint's pin numbers follow the CAZN catalogue (2025, p. 49, X-coded female face); the part drawing itself shows no numbers.
  - Find the panel cut-out and where the panel sits relative to the board edge. Both boards put the edge at y = 64.
  - Check that the shell is metal. The CAZN datasheet lists the shell as "brass Ni / zinc alloy Ni / PA+GF" without saying which applies to C7423431, and its photo shows a black body. (LCSC's "Brass" attribute is the contact material, not the shell.) LCSC's product photos show it sold with a nickel-plated housing, hex nut and O-ring that the CAZN drawing doesn't show. The enclosure is metal with GND = chassis (decided 2026-10-11), so a metal shell bonds to the panel through its nut.
  - With a metal shell on the chassis, the 1500 V PoE isolation also depends on the insulation between the contacts and the shell inside the connector. CAZN publishes no withstand voltage for it; ask CAZN, or hipot a sample.
  - Measure the housing the drawing leaves out: nut across corners, panel thread, O-ring stack, and shoulder to board edge. The panel position relative to y = 64 can only come from this.
  - CAZN's catalogue rates X-coded M12 at 0.5 A per contact (JLC's attribute says 2 A). 0.5 A still covers 802.3af/at.
  - **Second source:** buy one Toponelec C12B-MAX-RM-8-SH (C22387824) with the CAZN samples. Its drawing has the same PCB layout and adds the panel cut-out, but it states a rated impulse voltage of only 800 V: include it in the contacts-to-shell insulation test above.
  - Pin 9 carries an X-shaped metal plate on the board-facing side (LCSC photo). Check whether it sits flush on the board and how far its arms reach. If flush beyond about 3.3 mm from pin 9, the F.Cu SP_45 track at (46.24, 55.35)–(43.79, 57.79) is under it with only solder mask between: move that segment to B.Cu or beyond about 4.2 mm.
- [ ] **DP9900LP pins:** check the pin size. The footprint uses a 1.0 mm drill (LCSC uses 0.8 mm) so that a 0.64 mm square pin also fits.
- [ ] **Stack heights:** measure the real stack, with the Teensy header plastic and a 2×3 2.0 mm male header on the Teensy.
- [ ] **Load current:** measure the 5 V current of the Teensy plus carrier.
  - Under 100 mA: fit R6. The DP9900LP minimum load is 100 mA.
  - The PoE board alone, or on a carrier without a Teensy, is always below it (a few mA), so bench tests of the PoE board need a ≥ 100 mA load: a 47 Ω 2512 1 W resistor (for example LCSC C15261) on the R6 footprint draws 106–112 mA.
  - The maximum is 1.4 A.
- [ ] **Switch detection:** check that your MikroTik (Mode B), UniFi and Cisco (Mode A) switches detect and power the board.

## Before ordering

- [ ] **Primary copper next to the panel edge (review D1, open).** The board edge at y = 64 is the panel line, and the panel is chassis (= isolated GND). Primary copper comes within 0.54 mm of that edge: the SP_45 track along the bottom (15 mm long), the CT_36 via and track (0.94–1.04 mm), D2's lower pads (1.30 mm), BS_NODE and VIN_N (1.57–1.72 mm); 34 primary items are closer than the board's own 2.6 mm isolation rule. Whether that's a real problem depends on where the panel's inner face sits relative to y = 64, which only the M12 sample and the enclosure drawing can tell. Fix options: re-route the primary side away from the edge, or keep the panel (or an insulating sheet) far enough from the board edge.

## Bench checks on the first boards

- [ ] 5 V regulation and ripple. The datasheet ripple figure for the DP9900LP-5V is "TBD".
- [ ] Link at 100 Mbit/s through the M12 cordset on each switch. Check the QNEthernet link status.
- [ ] Temperature of PS1 **and C6** at the measured load, inside the enclosure. C6 is a 2000 h / 105 °C part, about 2.6 years of continuous use at 70 °C (design-notes.md section 4).
- [ ] Insulation test, primary to secondary. If hipot: **impulse only** (802.3 option c, 1500 V 10/700 µs). 1500 Vrms or 2250 V DC for 60 s puts 2.07–2.20 kV on C5, which is rated 2 kV (design-notes.md section 5).
- [ ] Start-up with the real stack: scope 5 V and 3.3 V at PoE power-on (no hiccup; carrier `open-issues.md` has the details).
- [ ] **Interference:** check genlock and LTC for noise from the PoE converter. The PoE board sits above the carrier's analog section. Compare the scope traces with PoE power and with bench power on J7.

## Known limits

- **Spacing:**
  - Isolation is by spacing only: ≥ 2.6 mm by DRC rule, measured as 2.7 mm across C5's pads and ≥ 3.2 mm everywhere else.
  - The board has not been hipot-tested.
  - The DP9900LP's isolation is rated by impulse test (1500 V peak).
- **J1 pin 9 (shield) is left open.** The cable shield bonds only through the connector housing and the panel. Inside the connector pin 9 is 1.53 mm from the data pins; that, not the board, sets the PoE-to-chassis spacing there (see the M12 sample check above).
- **802.3af class:** the module does not document a class setting. The board shows up as the default class.
- **Schematic origin:** the schematic was generated from the routed board, then linked. It is correct and editable, but not hand-drawn.
