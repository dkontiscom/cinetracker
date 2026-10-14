# Ordering the carrier at JLCPCB (v0.62)

**Status:** v0.62 is designed but not ordered. Before any order: owner review, then (with funding) a review by a professional PCB designer. Which revision gets ordered is decided then.

## Files (in `../fab/`)

| File | Upload / use |
|---|---|
| `carrier-v0.62-gerbers.zip` | PCB order. 4 copper layers (In1 = GND, In2 = +3V3), masks, silk, paste, outline, PTH/NPTH Excellon with 6 plated slots. The NPTH file includes the two 2.7 mm PoE standoff holes |
| `carrier-v0.62-BOM-JLCPCB.csv` | Assembly BOM (Comment / Designator / Footprint / LCSC Part #) |
| `carrier-v0.62-CPL-JLCPCB.csv` | Assembly placement in **JLC's orientation** (since v0.62): rotations and origins are those of JLC's own (LCSC/EasyEDA) footprint for each part, not KiCad's. J1–J9 sit at the centre of the bounding box of their pad centre points (= the LCSC footprint origin; for J7–J9 the middle of the row); all other parts at the footprint origin |
| `carrier-v0.62-BOM-full.csv` | Manufacturer, MPN, JLC stock, quantity bought and prices |
| `carrier-v0.62-1to1-fit-check.pdf` | Print at 100 % and test-fit the real connectors and the Teensy |

All fab files are exported from `cinetracker-carrier.kicad_pcb` with `kicad-cli` (KiCad 10.0.7). Rebuild or check them from the repo with `python3 hardware/tools/make_fab.py carrier=v0.62 poe=v0.62 --check` (or `--write`); it needs KiCad 10 and rsvg-convert, and holds every export setting, the JLC rotation table and the BOM/CPL rules. Inner layers in the zip: `GNDPLANE.g1` is layer 2 (In1) and `PWRPLANE.g2` is layer 3 (In2); the X2 header of each Gerber says the same, if JLC asks for the layer order.

## Order settings

- **PCB:** 4 layers, 1.6 mm, ENIG (flat pads for the 0.3 mm X1SON and the TSSOPs), green mask (Economic PCBA offers ENIG only in green), 1 oz outer / 0.5 oz inner copper (JLC default), JLC's standard stack-up. Mark on PCB: none (JLC dropped "order number at a specified location" in May 2026).
- **DRC:** `cinetracker-carrier.kicad_dru` holds JLC's limits for exactly these options (sources in the file), so a clean DRC means JLC can build it. Other copper weights change the limits.
- **Order remark:** the BNC (J1) overhangs the bottom edge by 3.0 mm and the M8 sockets sit on the edges by design; please don't add edge rails on those edges.
- **Assembly:** Economic PCBA, top side, with *Confirm Parts Placement* ticked.
- **Placement preview:**
  - The CPL already carries JLC's rotations (from a comparison with JLC's library footprints). The preview is still the final check. Correct looks like this (board top view):

    | Part | CPL rotation | Correct in the preview |
    |---|---|---|
    | U1 (OPA356) | 90 | pin 1 (OUT) at the upper right, the 3-lead side facing up |
    | U7 (OPA356) | 180 | pin 1 (OUT) at the upper left, the 3-lead side facing left |
    | U4–U6 (TPD4E1U06) | 180 | pin 1 at the upper left, GND (pin 2) on the left-middle pad |
    | U2 (PCM1808), U3 (PCM5102A) | 270 | pin 1 at the upper left, pins running down the left side |
    | J1 (BNC) | 0 | face and thread outside the bottom edge |
    | J2 (M8 4-pin) | 0 | body inside, thread out of the top edge |
    | J3, J4 (M8 6-pin) | 180 | body inside, thread out of the top edge |
    | J5, J6 (M8 3-pin) | 270 | body inside, face out of the bottom edge, the single pin farther from the edge |
    | J7–J9 (sockets) | 0 | row horizontal, centred on the footprint |
    | D1, D2, D6 | as KiCad | cathode mark on the −X pad (pad 1) |

  - JLC places by the silkscreen when silk and CPL disagree (pin-1 marks are on every IC), but the CPL should be right on its own.
- **Stock on 2026-10-13 (JLC):**
  - **DS869FPR0470-6P (J3/J4, C42395389): 53**, unchanged since 2026-10-01, 2 per board. Single source, no part with the same land pattern at JLC/LCSC. Five boards need 10; a batch over 26 boards can't be built from this stock. Buy the parts into your JLC parts library as soon as the order is certain.
  - KH-BNC75-3511: 733. OPA356AIDBVR: 3,935 (was 947 on 10-11; OPA356AIDBVT, C2060087, is the same part on a smaller reel). ERA6AEB562V 0.1 %: 8,019 (YAGEO RT0805BRD075K6L, C865578, is a 0.1 % / 25 ppm 0805 drop-in).
  - Check again at upload, and don't accept JLC's own substitutes for these or for any 0.1 % resistor.

## After the boards arrive

1. **Cut the Teensy 4.1 VUSB–VIN pads** before powering from J7.
2. Power J7 from a current-limited bench supply (5 V, about 0.5 A limit) **before** fitting a Teensy. Check that +5V is present on J8 pin 1 and that there is no short.
   - **Bench supply rule, every time:** check the polarity at the plug, set the current limit to about 0.5 A, connect with the output **off**, then switch it on. D6 doesn't protect against a reversed supply or the ringing of a live plug-in (design-notes.md section 2).
   - Without a Teensy the carrier draws about 1 mA at most (D6 leakage, PCM1808 powered down). That's fine on a bench supply, but on the PoE board it's below the DP9900's 100 mA minimum load (see the PoE board's `ordering.md`).
3. Fit the Teensy with the USB end on the left (the J8/J9 pin-1 end). Run `../tools/check_teensy_pins.py` after any layout change.
4. The whole stack (Teensy headers, PoE board, J3, enclosure), in order: [../../documentation/assembly-order.md](../../documentation/assembly-order.md).
5. Measure the bench checks listed in [open-issues.md](open-issues.md): tri-level levels and rise times into 75 Ω, LTC levels, and the 3.3 V and 5 V currents.

## Cost estimate: 5 assembled boards, ENIG, Economic PCBA, shipped to Cyprus

| Line | USD | Basis |
|---|---|---|
| PCB 4-layer 80 × 64 mm, ENIG, qty 5 | 25.20 | JLC calculator, 2026-10-01 |
| PCBA setup (Economic) | 8.18 | JLC published fee |
| Stencil | 1.53 | JLC published fee |
| SMT joints, 1,100 × $0.0016 | 1.76 | JLC published fee |
| Extended-part loading, 22 types × $3.07 | 67.54 | JLC published fee |
| THT hand soldering, 445 joints × $0.0164 | 7.30 | JLC published fee |
| Hand-soldering labour | 3.58 | JLC published fee |
| Components, including attrition | 164.24 | JLC live prices, 2026-10-11 (v0.62: C2/C34 now 10 µF) |
| **Subtotal** | **279.33** | |
| DHL (DDP) to Cyprus | about 23–30 | JLC live: $22.93 at 0.21 kg (PCB only); rises with assembled weight |
| **Total before VAT** | **about $302–309** | about $60–62 per board |

**Not included:**
- Cyprus VAT (19 %, if charged at checkout).
- Teensy 4.1 **with the Ethernet chip**: PJRC TEENSY41 (or TEENSY41_LOCK), not the `_NE` versions, which have no Ethernet PHY and so no network. Shops now sell them without header strips, so add 2 × 1×24 2.54 mm male headers.
- M8 male cordsets.
- The PoE board.

JLC's checkout total is binding. Sources: [JLCPCB assembly price](https://jlcpcb.com/help/article/pcb-assembly-price), [JLC quote](https://cart.jlcpcb.com/quote).
