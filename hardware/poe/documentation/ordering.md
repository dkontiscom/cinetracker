# Ordering the PoE board at JLCPCB (v0.62)

**Status:** v0.62 is designed but not ordered. Before any order: owner review, then (with funding) a professional PCB designer review. Before ordering, close the items in [open-issues.md](open-issues.md), especially the J3 interconnect and the sample fit checks.

## Files (in `../fab/`)

| File | Upload / use |
|---|---|
| `poe-v0.62-gerbers.zip` | PCB order: 2 layers, masks, silk, paste, outline, PTH and NPTH Excellon |
| `poe-v0.62-BOM-JLCPCB.csv` | Assembly BOM (Comment / Designator / Footprint / LCSC Part #) |
| `poe-v0.62-CPL-JLCPCB.csv` | Assembly placement. Through-hole parts sit at their pin centroid. Hand-fitted parts (J2, J3) are excluded by their "exclude from position files" flag |
| `poe-v0.62-BOM-full.csv` | Manufacturer, MPN, JLC stock, quantity bought and prices. It also lists the parts JLC does not fit |
| `poe-v0.62-stack-1to1-fit-check.pdf` | Carrier and PoE board outlines overlaid in one coordinate system. Print at 100 % |

All fab files come from `cinetracker-poe.kicad_pcb`, exported with `kicad-cli` (KiCad 10.0.7). Rebuild or check them from the repo with `python3 hardware/tools/make_fab.py carrier=v0.62 poe=v0.62 --check` (or `--write`); it needs KiCad 10 and rsvg-convert, and holds every export setting, the JLC rotation table and the BOM/CPL rules.

## Order settings

- **PCB:** 2 layers, 1.6 mm, any finish, green mask, 1 oz copper (JLC default). Mark on PCB: none (JLC dropped "order number at a specified location" in May 2026).
- **DRC:** `cinetracker-poe.kicad_dru` holds JLC's limits for these options next to the isolation rules (sources in the file).
- **Order remark:** J1 (M12) overhangs the panel edge by design, and D2, C1, C2, R4, C5 are 1.35–2.4 mm from an edge, under JLC's 2.5 mm assembly rule. Expect a DFM question; if JLC wants edge rails, they can't go on the M12 edge.
- **Assembly:** Economic PCBA, top side, with *Confirm Parts Placement* ticked. J1 (M12) and PS1 (DP9900LP) are through-hole parts that JLC hand-solders.
- **Placement preview:** the CPL uses KiCad rotations, and four footprints are project footprints. Check each of these:
  - **T1 (B1608S):** pin-1 dot at the north-east corner. Pins 1–8 face the Teensy side.
  - **PS1:** pin 1 square pad at the north-east corner. The input pins face the M12.
  - **D1, D2 (MB6S):** "+" lead on the north-west pad (VIN_P), "−" on the south-west pad, AC leads on the two east pads.
  - **D3:** cathode bar towards /VIN_P.
  - **J1:** the mating face over the board edge.
  - **D4 (LED):** cathode on the east pad (GND).
  - **C6 (electrolytic):** "+" on the west pad; the chamfered base corners point west.
  - The CPL rotations already match JLC's library footprints for all 21 rows (review A2, 2026-10-13); the preview is still the final check.
- **Not fitted by JLC:**
  - J2, the 2×3 2.0 mm socket on the bottom. Hand-solder it.
  - J3, the J7 link. Its type is still to be decided.
  - R6, the bleeder option.
- **Stock on 2026-10-10:** the M12 socket C7423431 had **48** at JLC. Five boards use 5. Check again at upload.

## Cost estimate: 5 assembled boards

| Line | USD | Basis |
|---|---|---|
| PCB 2-layer, 72 × 38 mm, qty 5 | **not quoted** | run the JLC calculator at upload |
| PCBA setup (Economic) | 8.18 | JLC published fee |
| Stencil | 1.53 | JLC published fee |
| SMT joints, 280 × $0.0016 | 0.45 | JLC published fee |
| Extended-part loading, 8 types × $3.07 | 24.56 | JLC published fee |
| THT hand soldering, 85 joints × $0.0164 | 1.39 | JLC published fee |
| Hand-soldering labour | 3.58 | JLC published fee |
| Components, including attrition | 91.22 | JLC live prices, 2026-10-10 (the M12 alone is $50.33 and the DP9900LP $34.35) |
| **Subtotal without PCB and shipping** | **130.91** | about $26 per board |

**Not included:**
- the PCB;
- shipping;
- Cyprus VAT;
- J2, the J3 header or cable, the 2×3 2.0 mm male header for the Teensy;
- nylon M2.5 standoffs;
- M12 X-coded cordsets.

JLC's checkout total is binding. Sources: [JLCPCB assembly price](https://jlcpcb.com/help/article/pcb-assembly-price), [JLC quote](https://cart.jlcpcb.com/quote).

## After the boards arrive

1. **Before plugging into the carrier:** power the PoE board alone from a PoE switch or a 48 V current-limited supply on the bridge inputs.
   - Check 5 V at J3.
   - D4 should light.
   - Load it with at least 100 mA, for example 47 Ω 1 W (LCSC C15261, fits the R6 footprint). Below 100 mA the DP9900 can whine and the switch can drop the port, so don't judge the 5 V without that load.
2. **Isolation:** check isolation from every primary net to GND with an insulation tester, if one is available. For a hipot use the 1500 V impulse test only, not 1500 Vrms or 2250 V DC (C5 is rated 2 kV, see design-notes.md section 5).
3. **Fit check:** fit the stack with the Teensy (VUSB–VIN cut) and check the heights against [design-notes.md](design-notes.md) section 2.
