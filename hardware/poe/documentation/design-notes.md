# PoE board v0.62 design notes

v0.60 is v0.1 renumbered to match the carrier. v0.61 is the same circuit; it moved MDI_TX_P (and MDI_RX_N to the bottom layer) so no track comes near the M12 housing pin. "v0.1" below refers to the same design.

Date: 2026-10-10. Status: designed in KiCad 7, moved to KiCad 10 (10.0.7) on 2026-10-11; not built or ordered.

All positions are in **carrier board coordinates** (mm). The origin is the carrier's top-left corner and y runs down. The PoE board's KiCad file uses the same coordinates, so the Teensy pins, carrier J7 and the standoff holes line up by construction. `tools/verify_poe.py` checks this against the carrier board file.

## 1. What the board does

- **Power:** takes 802.3af/at PoE from an M12 X-coded cable (Mode A or Mode B) and makes an isolated 5 V supply. The 5 V goes into carrier J7, which feeds the Teensy VIN.
- **Ethernet:** passes 100BASE-TX through PoE magnetics to the Teensy 4.1's six Ethernet pins.

## 2. Stack and heights

| Level | Height above the carrier top (mm) | Source |
|---|---|---|
| Carrier sockets J8/J9 (XKB X6511FV-24-C85D32) | 8.5 | carrier BOM |
| Teensy male header plastic | about 2.5 | **suspicion**: assumes standard 2.54 mm headers. Measure your Teensy |
| Teensy PCB | 1.57 | [PJRC dimension drawing](https://www.pjrc.com/teensy/dimensions_teensy41.png) |
| **Teensy top surface** | **about 12.6** | sum of the rows above |
| 2×3 2.0 mm male header on the Teensy Ethernet pads (plastic) | about 2.0 | **suspicion**: typical 2.0 mm header. PJRC's kit lists a generic 2 mm 3×2 header ([Ethernet kit](https://www.pjrc.com/store/ethernet_kit.html)) |
| J2: 2×3 2.0 mm female socket, 4.3 mm (C18723065) | 4.3 | JLC part attributes |
| **PoE board underside** | **about 18.9** | sum |
| PoE board top (1.6 mm PCB) | about 20.5 | |
| M12 axis | about 29.2 (8.71 above the PCB) | [CAZN drawing](https://wmsc.lcsc.com/wmsc/upload/file/pdf/v2/lcsc/2307120959_CAZN-M12-S8X-PPL_C7423431.pdf) |
| M12 insert top | about 34.6 (14.14 above the PCB) | same |
| M12 hex housing top | about 37.8 | **suspicion**: the C7423431 housing has no drawing; similar parts have a 15 mm hex (review E) |
| DP9900LP top | 28.5 ± 0.5 (8.00 ± 0.5 above the PCB, the 1.5 mm spacer included) | [DP9900 datasheet](https://wmsc.lcsc.com/wmsc/upload/file/pdf/v2/lcsc/2305061824_SDAPO-DP9900LP-5V_C5380108.pdf) section 4.2, p. 8 |

**Consequence:** the lid's inner face must be at least 40.5 mm above the carrier top (the M12 housing plus the sheet-metal bend above its hole). Full list of what the box must provide: [../../documentation/enclosure-requirements.md](../../documentation/enclosure-requirements.md). Without the PoE board, the tallest things on the carrier are the panel connectors at about 13 mm.

**Tolerance:** the M12 axis (≈ 29.2 mm) sums seven tolerances, about ±1.2 mm worst case. Its panel hole has to allow for that (a vertical slot), or the panel nut strains J2, the Teensy Ethernet header and J3.

### Problem: J3 cannot reach carrier J7 with standard headers

- **The gap:** the top of carrier J7 is at 8.5 mm and the PoE board underside at about 18.9 mm, so the gap is about 10.4 mm.
- **Why a standard header fails:** a standard 2.54 mm male header (2.5 mm plastic, 6 mm pin) on the PoE underside ends about 1.9 mm above J7, so it never makes contact.
- **What is needed:** about 12 mm of pin below the plastic, for 3–4 mm of insertion into J7. A taller J7 does not fix this on its own. The 11 mm socket JLC stocks (C41427519) still leaves only about 0.6 mm of insertion.
- **What v0.1 does:** J3 is a plain 1×4 2.54 mm through-hole footprint at the J7 position. Both options below fit it:
  - **A. Long-pin 1×4 male header**, hand-fitted and cut to the measured length. J7 stays as it is. Not JLC-assembled.
  - **B. Short 2-wire cable with locking connectors** on both boards. It tolerates any stack height and uses JLC parts, but it is one more cable, and J3 and J7 change footprint.
- **Both options:** measure the real stack (Teensy header plastic, Ethernet header) first.
- **Decided 2026-10-10: option A**, the long-pin header. The part and its pin length are chosen after the stack is measured.

### Standoffs

- **Holes:** two M2.5 holes (NPTH), H1 at (77.0, 48.1) and H2 at (10.0, 48.5).
- **Carrier side:** both spots were free on carrier v0.5d; carrier v0.5e and later have matching holes (MH5, MH6). On carrier v0.60 the nearest track, via or courtyard is 5.5 mm (H1) and 3.5 mm (H2, the BNC body courtyard added in v0.60) away, checked by `tools/verify_poe.py`.
- **Isolation:** both holes sit inside the isolation band. Use **nylon** standoffs and screws, so no metal links the PoE side to the carrier.
- **Panel:** the M12 is panel-mounted, so its nut also holds the board at the panel edge.

### Keep-outs

- **On top of the Teensy:** the PoE board underside is about 6 mm above the Teensy top. The tallest Teensy parts are 3.07 mm (micro USB) and 2.5 mm (button) (PJRC drawing).
- **USB Host pins:** do not fit a header on the Teensy USB Host pins (x 17.15–27.31, y 38.19) when the PoE board is used.
- **Carrier LEDs and SW1:** the PoE board starts at y = 26, so carrier D1/D2 (y 22.5) and SW1 (y 18.5) stay uncovered for the lid light pipes and the plunger.

## 3. Ethernet path

- **J2:** 2×3 2.0 mm socket on the **bottom** side, onto a 2×3 2.0 mm male header soldered on the Teensy. Pin order is the same as PJRC's Ethernet kit header: 1 TX+, 2 TX−, 3 GND, 4 LED (not used), 5 RX−, 6 RX+.
  - Pin positions: [PJRC Teensy 4.1 Ethernet pinout](https://www.pjrc.com/store/teensy41_ethernet_pinout.png).
  - Pin order: [PJRC kit schematic](https://www.pjrc.com/store/schematic_ethernet.png).
- **T1, B1608S** (10/100 PoE+ magnetics with CMC, 720 mA, 1500 Vrms):
  - **Wiring:** Teensy TX goes to the RD winding (pins 6/8) and Teensy RX to the TD winding (pins 1/3). This avoids crossings.
  - **Polarity:** the dotted ends are in phase, 1–16 and 6–11 (datasheet), so polarity carries through to the cable.
  - **MDI order:** standard, with TX on RJ45 1/2. That works with or without crossover detection.
  - **Crossover and polarity are covered twice:**
    - The DP83825I does Auto-MDIX, enabled by default by its strap. *Auto MDIX Enable* is the default in the strap table of the [DP83825I datasheet](https://www.ti.com/lit/ds/symlink/dp83825i.pdf). How the Teensy board straps this is not checked.
    - 100BASE-TX is "immune to polarity problems" (same datasheet, 6.3.13.4).
- **PHY-side centre taps:** each gets its own 100 nF to GND (C8, C9). PJRC's kit uses one 0.1 µF for both. The DP83825I is a voltage-mode driver ([TI product page](https://www.ti.com/product/DP83825I)), so the centre taps are not biased.
- **Layout:** the pairs are 0.25 mm tracks of about 15–20 mm over a solid GND pour. That is electrically short for 100BASE-TX; the impedance is not controlled.

## 4. PoE input (primary side)

- **J1:** CAZN M12-S8X-GPLM12-BJD (X-coded, 8-pin, 60 V). With a T568B cordset the pins map like this (CAZN 8X table):
  - 1/2 → RJ45 1/2
  - 3/4 → RJ45 3/6
  - 8/7 → RJ45 4/5
  - 5/6 → RJ45 7/8
  - **Suspicion:** confirm with your cordset.
- **Power inputs:** the circuit follows the DP9900 typical connection (datasheet fig. 3).
  - **Mode A:** D1 (MB6S) takes the T1 cable-side centre taps.
  - **Mode B:** D2 (MB6S) takes the spare pairs.
  - **TVS:** D3 (SMAJ58A) sits across VIN, as SDAPO's typical circuit specifies. Its clamping voltage (93.6 V at rated pulse current) is above the DP9900's 80 V surge rating, so a large surge could still reach the module; normal PoE (57 V max) is far below its 58 V standoff.
- **PS1, DP9900LP-5V:**
  - Rated 5 V at 1.4 A, 7 W at 70 °C, with 1500 V impulse isolation.
  - ADJ is left open, which gives 5.00 V.
  - **Minimum load is 100 mA.** Below that it can whine and the PSE can drop the port (datasheet note 2).
  - R6 is an unfitted 2512 bleeder footprint, in case the measured Teensy + carrier load is under 100 mA. A 47 Ω part draws about 106 mA at about 0.53 W.
- **Output capacitors:** C6 100 µF 25 V and C7 10 µF, as in the datasheet's typical circuit.
  - C6 (KNSCHA RST) is the short-life electrolytic class: 2000 h at 105 °C. With the usual rule (life doubles every 10 °C cooler, Würth SN008a) that's about 0.9 years of continuous use at 85 °C, 2.6 years at 70 °C and 10 years at 50 °C. For a 24/7 install that matters, so measure C6's temperature next to PS1 in the bench check; if it runs hot, use a same-size 105 °C part rated 5000 h or more.
- **D4:** a green "5 V present" LED for bring-up.
- **Bob Smith terminations**, as in [TI SLUA454](https://www.ti.com/lit/an/slua454/slua454.pdf) section 1.4:
  - Each of the four pairs gets 75 Ω + 10 nF 100 V. The capacitors are needed because PoE puts DC on the pairs.
  - The common node goes through C5 (1 nF 2 kV) to the isolated GND. Through the BNC shell that GND is the chassis ground.
- **J1 pin 9** (shield / housing contact) **is left open.** It sits 1.5 mm from the data pins inside the connector, so tying it to anything on the PCB would undercut the isolation spacing. The housing bonds to a metal panel through its nut.
  - With GND on the chassis, that puts pin 9 on the isolated side. A DRC rule keeps every track and via ≥ 1.53 mm from it, the connector's own pin-to-pin-9 spacing. Up to v0.60 MDI_TX_P ran through the middle of the pin circle, 0.45 mm from it; v0.61 routes it round the outside, and MDI_RX_N moved to B.Cu next to MDI_RX_P so they both pass between the J1 posts. Nearest track since the v0.62 re-route: 1.93 mm (the B.Cu links between pins 5–6 and 7–8).

## 5. Isolation

- **Requirement:** IEEE 802.3 clause 33.4.1 asks for 1500 V isolation between the MDI and every accessible conductor (quoted in [IEEE 802.3 law_2_0705](https://ieee802.org/3/poep_study/public/jul05/law_2_0705.pdf)). Everything on the carrier and the Teensy, the BNC shell included, is on the isolated side.
- **The band:** a 3.0 mm band (y 46.6–49.6) runs the full width of the board as a KiCad rule area with no tracks, vias or copper fill.
  - Only these cross it: PS1 (input and output rows 11.38 mm apart), T1 (rows 8.4 mm apart), C5 (2 kV part) and the two nylon standoff holes.
  - Everything south of the band is primary.
- **Panel edge (v0.62):** the board edge at y = 64 sits on the panel, which is chassis (= isolated GND). A keepout rule area "panel edge (chassis): no copper" covers y 61.4–64, so all primary copper stays ≥ 2.6 mm from the panel, like the band. Measured: 2.65 mm (D2's lower pads). Before v0.62 it was 0.54 mm (review D1).
- **DRC rule:** the project's `cinetracker-poe.kicad_dru` holds a custom rule. It requires ≥ 2.6 mm between any primary-class copper and any other connected copper. Copper with no net and no-connect pins (KiCad names their nets `unconnected-...`) are left out: T1's NC pins and J1 pin 9.
  - **Measured gaps:** 2.7 mm across C5's own pads (rated 2 kV), then 3.2 mm and up everywhere else. `tools/verify_poe.py` lists every gap below 4 mm.
  - **Rule check:** tightening the rule makes it fire, so it is active.
- **Limits:**
  - This is spacing only. It is not a hipot test.
  - The DP9900LP is rated by a 1500 V impulse test, not 1500 Vrms. 802.3 accepts that as one of its three test options.
  - **If you hipot it, use the impulse test** (802.3 option c: 1500 V, 10/700 µs, 10 pulses 60 s apart). In an MDI-to-GND test C5 takes about 98 % of the voltage, because the four 10 nF Bob Smith caps sit in series with it. 1500 Vrms (2.07 kV peak on C5) or 2250 V DC (2.20 kV) for 60 s would overstress C5, which is rated 2 kV and proof-tested by Yageo at 1.2 × 2 kV for 1–5 s only. The impulse puts about 1.46 kV on it.
- **Common-mode path:** with J1 pin 9 open, common-mode current from the cable doesn't return through the M12 housing. It goes through the Bob Smith network (about 63 mm of track) and C5 into the isolated GND, then through J2/J3 into the Teensy and carrier ground and out through the BNC/M8 shells to the panel. In the 802.3 impulse test that's about 0.15 A through the ground copper for the 10 µs front (1 nF × 1500 V / 10 µs), which should be harmless (suspicion). Worth remembering if ESD testing at the M12 upsets the Teensy.

## 6. Board

- **Board:**
  - 2 layers, 1.6 mm.
  - Outline 72 × 38 mm overall: a main area of x 17–71, y 26–64, plus tabs for the standoffs.
  - Edge on the panel line, y = 64.
- **Isolated side:** GND fill on both layers.
- **Primary side:** no fill.
- **Netclasses:**

  | Class | Nets | Track | Clearance |
  |---|---|---|---|
  | Default | everything else | 0.25 mm | 0.2 mm |
  | Power | +5V, GND | 0.6 mm | 0.2 mm |
  | Primary | all PoE-side nets | 0.4 mm | 0.3 mm |

- **Primary clearance:** 0.3 mm between primary nets, which see up to 57 V. Checked against IPC-2221B Table 6-1, 51–100 V row (review E): tracks under solder mask need 0.13 mm (B4), so 0.3 mm passes there. Bare soldering lands next to copper at another DC need 0.5 mm (A6): a DRC rule enforces it (pairs at the same DC, i.e. a pair's two wires, its centre tap and its Bob Smith resistor node, are left out). Measured minimum 0.53 mm since the v0.62 fix. The M12 pads are 0.44 mm apart, but only between the two pins of one pair (same DC).
- **JLC limits in DRC (v0.62, review D1):** `cinetracker-poe.kicad_dru` adds JLCPCB's published limits for this order (2 layers, 1 oz, Economic PCBA) after the isolation rules: PTH annular ring 0.18 mm, hole-to-hole 0.45 mm, PTH hole to copper 0.28 mm, silk text 1.0 / 0.15 mm, pad-to-silk 0.15 mm, no via hole within 0.1 mm of an SMD pad, SMD parts ≥ 2.5 mm from the edge (C5 on the H2 ear is the one known exception, see `ordering.md`). Sources are in the file.
- **How it was made:** the board was placed by script, routed with Freerouting, and the schematic generated from the routed board. **From now on the schematic is the source.** v0.62: the primary side (R1–R4, C1–C4, D1–D3 and the Bob Smith, centre-tap, spare-pair and VIN nets) was re-placed by hand and re-routed with Freerouting 2.1.0, with the isolated side, the MDI pairs and every part locked, then checked by DRC (all project rules) and `tools/verify_poe.py`. After review E, 16 track pieces next to lands were re-routed by hand (text-level edits) to meet the 0.5 mm land rule.

## Sources

- **PJRC:**
  - [Teensy 4.1 dimensions](https://www.pjrc.com/teensy/dimensions_teensy41.png)
  - [Ethernet pinout](https://www.pjrc.com/store/teensy41_ethernet_pinout.png)
  - [Ethernet kit](https://www.pjrc.com/store/ethernet_kit.html) and [kit schematic](https://www.pjrc.com/store/schematic_ethernet.png)
- **Parts:**
  - [SDAPO DP9900](https://wmsc.lcsc.com/wmsc/upload/file/pdf/v2/lcsc/2305061824_SDAPO-DP9900LP-5V_C5380108.pdf)
  - [CND-tek B1608S](https://wmsc.lcsc.com/wmsc/upload/file/pdf/v2/lcsc/2405301629_CND-tek-B1608S_C507596.pdf)
  - [CAZN M12-S8X-GPLM12-BJD](https://wmsc.lcsc.com/wmsc/upload/file/pdf/v2/lcsc/2307120959_CAZN-M12-S8X-PPL_C7423431.pdf)
  - [MDD MB6S](https://datasheet.lcsc.com/datasheet/pdf/d166b641999f7c4a86c69b88af3f89c6.pdf)
  - [C5, Yageo CC1812KKX7RDBB102 (LCSC C118019)](https://www.lcsc.com/product-detail/multilayer-ceramic-capacitors-mlcc-smd-smt_yageo-cc1812kkx7rdbb102_C118019.html): Yageo high-voltage MLCC spec V.16, "Voltage Proof": Ur ≥ 1 kV → 1.2 Ur for 1–5 s
  - [C6, KNSCHA RST100UF25V004 (LCSC C4747964)](https://www.lcsc.com/product-detail/aluminum-electrolytic-capacitors-smd_knscha-rst100uf25v004_C4747964.html): 2000 h at 105 °C, 88 mA ripple at 120 Hz
  - [Würth SN008a, electrolytic capacitor lifetime](https://www.we-online.com/components/media/o173043v410%20SN008a%20EN.pdf): life doubles per 10 °C
- **IEEE 802.3 clause 33.4.1**, isolation test options (a) 1500 Vrms 60 s, (b) 2250 V DC 60 s, (c) 1500 V 10/700 µs impulse × 10: quoted in [law_2_0705](https://www.ieee802.org/3/poep_study/public/jul05/law_2_0705.pdf)
- **TI:**
  - [DP83825I datasheet](https://www.ti.com/lit/ds/symlink/dp83825i.pdf)
  - [SLUA454: EMI-compliant PoE PD design](https://www.ti.com/lit/an/slua454/slua454.pdf)
- **Requirements and switch survey:** [requirements-v0.1.md](requirements-v0.1.md)
