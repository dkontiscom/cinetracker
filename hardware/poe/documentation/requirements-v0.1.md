# CineTracker PoE board v0.1: requirements

Date: 2026-10-10 (updated the same day after the user's decisions). Status: requirements settled; schematic not started.

## 1. Decided

- **The stack ("sandwich"), from bottom to top:**
  1. The carrier PCB.
  2. The Teensy 4.1, components facing up, plugged into the carrier's 1×24 sockets (unchanged).
  3. The PoE board. It plugs onto the Teensy's 6 Ethernet pins and onto carrier J7 for power.
- **Ethernet connector: M12 X-coded, 8-pin, female, right-angle, CAZN M12-S8X-GPLM12-BJD (JLC C7423431)** (decided 2026-10-10). It sits on the PoE board, at the same panel edge as the carrier's BNC and LTC connectors.
  - All four pairs reach the board, so it powers up from any 802.3af/at PSE in Mode A or Mode B. That includes MikroTik switches and injectors.
  - It is rated 60 V DC.
  - M8 was dropped. Its only Ethernet variant is 4-pin D-coded, which leaves no spare pairs and so cannot take Mode B power. Its 8-pin version is rated only 30 V. See section 3.
- **Power:** use a **module** for v0.1 (SDAPO DP9900LP-5V; user, 2026-10-10). Our own discrete converter is deferred to a later PoE board version (v2 or so).
- **PoE source:** PoE switches, not injectors (user).
- **Folders:**
  - `hardware/carrier/` and `hardware/poe/` hold the design files and fab outputs.
  - `hardware/carrier/documentation/` and `hardware/poe/documentation/` hold the documentation.
  - The folders are named `documentation`, not `docs`, because the repo's `.gitignore` ignores every folder called `docs`. `.gitignore` stays unchanged.
- **Repo:** one repository for firmware and hardware (`dkontiscom/cinetracker`), with real KiCad sources (`.kicad_sch` and `.kicad_pcb`).

## 2. Geometry, in carrier board coordinates

The carrier is 80 × 64 mm. The origin is the top-left corner and y runs down.
- **Encoder connectors** are on the top edge (y = 0).
- **BNC and LTC connectors** are on the bottom edge (y = 64).

| Item | Position (mm) | Source |
|---|---|---|
| Teensy outline | x 9.53–70.49, y 24.73–42.51 (USB end at x ≈ 9.5) | carrier v0.5c board file |
| Teensy Ethernet pins, 2×3 at 2.0 mm pitch | x 23.77 / 25.77; y 34.79 / 36.79 / 38.79. RX+ at (23.77, 38.79), RX− at (25.77, 38.79), LED at (23.77, 36.79), GND at (25.77, 36.79), TX− at (23.77, 34.79), TX+ at (25.77, 34.79) | PJRC dimension drawing and XenGi KiCad footprint, which agree to 0.05 mm |
| Teensy USB Host pins, 1×5 at 2.54 mm pitch (keep clear) | x 17.15–27.31, y 29.05 | same |
| J7 (+5 V, +5 V, GND, GND), female, 8.5 mm tall | x 39.50 / 42.04 / 44.58 / 47.12, y 45.20 | carrier v0.5c |
| Free space on the bottom edge between carrier connectors | x 21.7–37.0 (between the BNC and J5) and x 51.0–58.0 | carrier v0.5c |
| Carrier analog genlock section, directly under the PoE board's path | x 20–50, y 44–62 | carrier v0.5c |

**Heights:**
- **Teensy top surface:** about 10.1 mm above the carrier (8.5 mm socket + 1.57 mm PCB).
- **Tallest parts on top of the Teensy:** the micro-B USB is 3.07 mm and the pushbutton 2.5 mm (PJRC drawing). Keep at least 3.1 mm clearance.
- **Ethernet link:** a 2×3 2 mm male header on the Teensy pointing up, as in PJRC's Ethernet kit, mating with a 2×3 2 mm female socket under the PoE board. This puts the PoE board's underside at roughly 16 mm above the carrier (**suspicion**: depends on the socket chosen).
- **Reaching J7:** the PoE board's pins must bridge about 7.5–8 mm above J7's 8.5 mm socket, so it needs a long-pin 1×4 header (about 14 mm below the board). The alternative is a taller J7 on carrier v0.5d.

## 3. Ethernet connector

**Facts (verified):**
- 100BASE-TX on M8 uses the **4-pin D-coded** variant (IEC 61076-2-114). The pinout is 1 = TD+, 2 = RD+, 3 = TD−, 4 = RD−. Pairs are on opposite corners (1/3 and 2/4).
- **Two pairs only** means PoE can arrive only as **Mode A** (power on the data pairs). A PSE that only does Mode B (spare pairs) cannot power the board; this includes many midspan injectors. The same is true for 4-pin M12 D-coded.
- **Switch compatibility (researched 2026-10-10, table below):** UniFi works and Cisco Catalyst 9300 works. **MikroTik af/at switches use Mode B, so they will NOT power a 4-pin board.**
- D-coded M8 contacts are rated 4 A and 60 V DC (Phoenix; binder 63 V). CAZN's 2025 catalog gives its D-coded M8 4 A and **48 V AC / 60 V DC**; JLC's "48V" is the AC figure. 60 V DC covers 802.3af/at (44–57 V).

**Which switches can power a 4-wire (Mode-A-only) device** (vendor documents checked 2026-10-10):

| Vendor | 802.3af/at output pairs | Mode-A-only device works? | Source |
|---|---|---|---|
| **MikroTik**: all PoE-out devices (CRS328-24P, CRS354-48P, CRS112-8P, CSS610-8P, RB5009UPr+S+, hEX PoE, netPower 16P) | **Mode B** (pins 4,5 + / 7,8 −). MikroTik: *"Mode A PD would require Mode B to Mode A converter."* | **No** | [MikroTik PoE-Out](https://help.mikrotik.com/docs/spaces/ROS/pages/19136769/PoE-Out) |
| MikroTik CRS320-8P-8B-4S+RM | both Mode A and Mode B. Its 802.3bt ports have a `forced-on-a` setting, which powers the A pairs with detection turned off | Only with manual forced power | same page |
| **UniFi** Gen2 and later: USW-24-PoE, USW-Pro-24-PoE (af/at on the bt ports too), Enterprise 8/24/48-PoE | **Mode A**: *"Pins 1, 2+; 3, 6-"* | **Yes** | [USW-24-PoE QSG](https://dl.ui.com/qsg/USW-24-POE/USW-24-POE_EN.html), [Pro-24-PoE datasheet](https://dl.ui.com/ds/usw-pro-24-poe_ds.pdf), [Enterprise-8-PoE datasheet](https://dl.ui.com/ds/usw-enterprise-8-poe_ds.pdf) |
| UniFi Lite-8-PoE, Pro Max | Mode A according to reseller copies of the datasheets; Ultra and Pro HD have no pins published | Probably (**suspicion**) | reseller copies |
| **Cisco** Catalyst 9300 (UPOE/UPOE+) | 802.3at uses *"ALT-A (signal pair)"*. The spare pairs are only enabled after CDP/LLDP negotiation | **Yes** | [Cat9300 PoE config guide 17.15](https://www.cisco.com/c/en/us/td/docs/switches/lan/catalyst9300/software/release/17-15/configuration_guide/int_hw/b_1715_int_and_hw_9300_cg/configuring_poe.html) |
| Cisco Catalyst 9200, C1000/1200/1300, CBS250/350, Meraki MS130/MS150 | not documented | Probably (**suspicion**) | — |

The cordset cannot fix this. 100 Mbit data must stay on pins 1,2/3,6, and the spare pairs do not exist in a 4-wire cable. **A 4-pin connector (M8 or M12 D-coded) therefore rules out every MikroTik switch.** An **8-pin** connector carries all four pairs. With bridges on both pair sets, the device then accepts Mode A and Mode B, which the DP9900LP's two-bridge input already provides.

**8-pin M8 is not an option:** CAZN rates its 8-pin M8 at 1.5 A / 30 V ([family drawing](https://atta.szlcsc.com/upload/public/pdf/source/20250214/1AA0F00C09FE0D64F0186C7C636B96A3.pdf), [CAZN 2025 catalog](https://www.cazn.com/uploads/upload/20250922/cb22fb901b11a3fa29d9710860ef2cf9.pdf) p. 23), and PoE needs up to 57 V.

**M12 X-coded pinout:**
- The CAZN C7423431 datasheet, page 2, "8X" table, gives pin 1 WHOG D1+, 2 OG D1−, 3 WHGN D2+, 4 GN D2−, 5 WHBN D4+, 6 BN D4−, 7 WHBU D3−, 8 BU D3+.
- With T568B colours on the RJ45 end of a standard cordset, that maps to the RJ45 pins below. **Suspicion**: confirm against the datasheet of the cordset you actually buy.

| M12 pin | Colour | RJ45 pin (T568B) | Role on CineTracker |
|---|---|---|---|
| 1, 2 | WH/OG, OG | 1, 2 | 100BASE-TX pair, plus Mode A power |
| 3, 4 | WH/GN, GN | 3, 6 | 100BASE-TX pair, plus Mode A power |
| 8, 7 | BU, WH/BU | 4, 5 | spare pair: Mode B power only |
| 5, 6 | WH/BN, BN | 7, 8 | spare pair: Mode B power only |

**PoE board wiring that follows from this:**
- Pins 1–4 go to the PoE magnetics (B1608S). Their cable-side centre taps feed **bridge 1**.
- Pins 5–8 feed **bridge 2** directly: 4,5 as one polarity, 7,8 as the other.
- Both bridges feed the DP9900LP input.

**Candidates:**

| Part | Type | LCSC / JLC # | JLC stock (2026-10-10) | Price | Notes |
|---|---|---|---|---|---|
| CAZN M8-S4D-GPLM10 ([JLC page](https://jlcpcb.com/partdetail/CAZN-M8_S4DGPLM10/C42931207)) | M8 D-coded, female, **right-angle**, through-hole, rear-panel M10×0.75 nut | C42931207 | **10** | $3.81 | The only right-angle D-coded M8 at JLC. It is the "4P D" member of the CAZN M8-Sxx-GPLM10 family ([family drawing](https://atta.szlcsc.com/upload/public/pdf/source/20250214/1AA0F00C09FE0D64F0186C7C636B96A3.pdf)), the same family as the carrier's J5/J6 (C42931206). The drawing gives 4 A / 60 V and a 1500 V impulse rating. JLC's attribute table says 48 V. JLC only, with no LCSC page (the same is true of J5/J6). No coding standard is cited, so **buy a sample plus a Phoenix or binder D-coded cordset and test the fit before committing.** **Supply (2026-10-10):** there is no end-of-life sign. LCSC China lists it as `productCycle: normal` ([item 45024858](https://item.szlcsc.com/45024858.html)), with the same 10 pieces JLC shows. CAZN's 2025 catalog ([cazn.com](https://www.cazn.com/uploads/upload/20250922/cb22fb901b11a3fa29d9710860ef2cf9.pdf)) still lists the angled female back-mount PCB socket family, M10×0.75, with D coding. For more pieces: order from CAZN (sales@cazn.com) or through szlcsc's ordering service, then [consign them to JLC](https://jlcpcb.com/help/article/how-to-consign-parts-to-jlcpcb-from-mainland-china). |
| **CAZN M12-S8X-GPLM12-BJD (CHOSEN)** ([JLC](https://jlcpcb.com/partdetail/CAZN-M12_S8X_GPLM12BJD/C7423431), [datasheet](https://wmsc.lcsc.com/wmsc/upload/file/pdf/v2/lcsc/2307120959_CAZN-M12-S8X-PPL_C7423431.pdf)) | **M12 X-coded, 8-pin**, female, right-angle, PCB, front-panel mount, grounded | C7423431 | 48 (also on LCSC) | $10.07 (1), $8.62 (10+) | 2 A / 60 V. Cites IEC 61076-2-109, the X-coded standard. All 4 pairs, so it works with Mode A **and** Mode B switches, MikroTik included. 14.85 mm wide with the axis 8.71 mm above the PCB; the M8 is 12.5 mm wide with the axis at 6.3 mm |
| CAZN M8-P4D-GPLM8 | M8 D-coded, male, right-angle | C42931204 | 15 | $3.81 | Male on the device is unusual; female matches the carrier's other sockets |
| CAZN M12-S4D-GPLM12 (shielded -JD: C42931310) | M12 D-coded, female, right-angle | C42931295 | 26 (19) | $5.77 | Larger, but a common industrial Ethernet connector, with a shielded option |
| Toponelec C12B-FAD-RM1-4-SH | M12 D-coded, right-angle, shielded | C22387805 | 42 | $7.32 | |
| binder 818 series 86 6620 1121 00404 | M8 D-coded, right-angle, shielded | none at JLC | — | — | Branded. Would need hand soldering, and distributor stock is unchecked |

## 4. PoE power architecture

**Load:** Teensy 4.1 plus the carrier, estimated at 1–2 W. **This is not measured yet.**

| Option | Parts | Area / height | JLC | Risks |
|---|---|---|---|---|
| **A. Module (chosen for v0.1)** | SDAPO DP9900LP-5V (C5380108, 5 V at 1.4 A, 7 W at 70 °C), 2× MB6S bridge (C2487), SMAJ58A (C2980408), CND-tek B1608S PoE magnetics (C507596, 720 mA, 1500 Vrms), 2 kV termination capacitors | about 40 × 18 mm, 8–9.5 mm tall | all in stock, all extended parts; the module is through-hole (JLC hand-solders it) | Thin datasheet: ripple is marked "TBD". Isolation is rated by the 1500 V impulse test, which 802.3 accepts as one of its three test options. Needs at least 100 mA load. |
| B. TPS23753A discrete flyback | TPS23753APWR (C92115), POE70P-50LD transformer (C20417054, $16, 200 in stock), opto, TLV431, MOSFET, Schottky, about 40 parts | about 35 × 18 mm (**suspicion**) | in stock, but the transformer is expensive and stock is thin | Flyback layout, loop compensation, EMI |
| C. TPE15017 / MP8017 (no opto) | smallest | — | the controller is in stock; no stocked transformer | Needs a custom transformer |

**Isolation:** IEEE 802.3 clause 33.4.1 requires 1500 V isolation between the MDI and every accessible conductor. The BNC shield, the Teensy and the whole carrier must therefore sit on the isolated 5 V side. Capacitors that bridge the barrier need a 2 kV rating, and the barrier needs at least 3 mm clearance.

**Magnetics, PHY side:** the Teensy has no magnetics. PJRC's kit magjack ties the PHY-side centre taps to 0.1 µF to GND. Copy that arrangement. The kit's magjack is not PoE-capable.

## 5. What this means for the carrier (v0.5d)

1. Add one or two standoff holes under the PoE board. MH3 and MH4 are in the bottom corners and are not under it.
2. Choose between a taller J7 and a long-pin header on the PoE board.
3. Keep the area under the PoE board free of anything taller than about 5 mm. The current parts there are low: SOT-23, 0805, 1206 and SMA.
4. **EMI:** the PoE converter will sit directly above the analog genlock and audio section. Put the module over the Teensy's middle (x about 28–50, y about 26–41) rather than over the genlock. Also keep a solid ground plane on the PoE board's isolated side facing the carrier. **The effect is a suspicion until measured.**

## 6. Open decisions

None. Connector, power architecture and PSE type are all decided.

**Before ordering v0.1 boards:**
1. Buy 2–3 of C7423431, a branded RJ45-to-M12 X-coded cordset (Phoenix, binder or Murrelektronik) and a matching panel nut. Check that they mate and that the pins map as in section 3.
2. Measure the carrier's real 5 V current (Teensy plus carrier), to confirm the DP9900LP's operating point (100 mA minimum load, 1.4 A maximum).
3. Check that the MikroTik, UniFi and Cisco switches you use detect the board as an 802.3af PD on both pair sets.

## Sources

- **PJRC:**
  - [Teensy 4.1 dimensions](https://www.pjrc.com/teensy/dimensions_teensy41.png)
  - [Teensy 4.1 schematic](https://www.pjrc.com/teensy/schematic41.png)
  - [Ethernet kit](https://www.pjrc.com/store/ethernet_kit.html) and [kit schematic](https://www.pjrc.com/store/schematic_ethernet.png)
  - [LPJ4012AHNL magjack](https://www.pjrc.com/store/LPJ4012AHNL.pdf)
  - [XenGi Teensy41.kicad_mod](https://raw.githubusercontent.com/XenGi/teensy.pretty/master/Teensy41.kicad_mod)
- **Connectors:**
  - [Phoenix M8 D-coded cable datasheet](https://docs.rs-online.com/09b7/A700000013615656.pdf)
  - [binder M8-D brochure](https://www.telegaertner.co.uk/fileadmin/user_upload/TGU/binder_circular_connectors/Files__Brochures/binder_M8-D_Automation_Technology_Series.pdf)
  - [CAZN M8-S4D-GPLM10 drawing](https://atta.szlcsc.com/upload/public/pdf/source/20250214/1AA0F00C09FE0D64F0186C7C636B96A3.pdf)
  - [IEEE 802.3af midspan Alternative B](https://ieee802.org/3/power_study/email/msg00808.html)
- **Power:**
  - [DP9900LP-5V datasheet](https://wmsc.lcsc.com/wmsc/upload/file/pdf/v2/lcsc/2305061824_SDAPO-DP9900LP-5V_C5380108.pdf)
  - [B1608S datasheet](https://wmsc.lcsc.com/wmsc/upload/file/pdf/v2/lcsc/2405301629_CND-tek-B1608S_C507596.pdf)
  - [TI TPS23753AEVM-001 (SLVU315)](https://www.ti.com/lit/pdf/slvu315)
- **Isolation (clause 33.4.1, quoted in):**
  - [IEEE 802.3 law_2_0705](https://ieee802.org/3/poep_study/public/jul05/law_2_0705.pdf)
  - [IEEE 802.3da Potterf 2022](https://www.ieee802.org/3/da/public/102622/SPMD_Potterf_Consequences_of_Being_Grounded_2022-10-26.pdf)
