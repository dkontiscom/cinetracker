# Ordering the carrier at JLCPCB (v0.5d)

**Status:** v0.5d is designed but not ordered. The current plan is to order prototypes at v0.6, together with the PoE board.

## Files (in `../fab/`)

| File | Upload / use |
|---|---|
| `carrier-v0.5d-gerbers.zip` | PCB order. 4 copper layers (In1 = GND, In2 = +3V3), masks, silk, paste, outline, PTH/NPTH Excellon with 6 plated slots |
| `carrier-v0.5d-BOM-JLCPCB.csv` | Assembly BOM (Comment / Designator / Footprint / LCSC Part #) |
| `carrier-v0.5d-CPL-JLCPCB.csv` | Assembly placement. Connector points sit at the footprint pad centroids |
| `carrier-v0.5d-BOM-full.csv` | Manufacturer, MPN, JLC stock, quantity bought and prices |
| `carrier-v0.5d-1to1-fit-check.pdf` | Print at 100 % and test-fit the real connectors and the Teensy |

All fab files are exported from `cinetracker-carrier.kicad_pcb` with `kicad-cli` (KiCad 7.0.11).

## Order settings

- **PCB:** 4 layers, 1.6 mm, ENIG (flat pads for the 0.3 mm X1SON and the TSSOPs). Order number at a specified location.
- **Assembly:** Economic PCBA, top side, with *Confirm Parts Placement* ticked.
- **Placement preview:**
  - The CPL uses KiCad-native rotations. Check the pin-1 dot of U1/U7 (SOT-23-5, pin 1 = OUT), U4–U6 (SOT-23-6) and U2/U3 (TSSOP), plus LED polarity (pad 1 = cathode).
  - Check that the connectors sit on their footprints.
- **Stock:** the tightest lines on 2026-10-10 were DS869FPR0470-6P (53, 2 per board), OPA356AIDBVR (947) and KH-BNC75-3511 (738). Check again at upload, and don't accept substitutes for these.

## After the boards arrive

1. **Cut the Teensy 4.1 VUSB–VIN pads** before powering from J7.
2. Power J7 from a current-limited bench supply (5 V, about 0.5 A limit) **before** fitting a Teensy. Check that +5V is present on J8 pin 1 and that there is no short.
3. Fit the Teensy with the USB end on the left (the J8/J9 pin-1 end). Run `../tools/check_teensy_pins.py` after any layout change.
4. Measure the bench checks listed in [open-issues.md](open-issues.md): tri-level levels and rise times into 75 Ω, LTC levels, and the 3.3 V and 5 V currents.

## Cost estimate: 5 assembled boards, ENIG, Economic PCBA, shipped to Cyprus

| Line | USD | Basis |
|---|---|---|
| PCB 4-layer 80 × 64 mm, ENIG, qty 5 | 25.20 | JLC calculator, 2026-10-01 |
| PCBA setup (Economic) | 8.18 | JLC published fee |
| Stencil | 1.53 | JLC published fee |
| SMT joints, 1,080 × $0.0016 | 1.73 | JLC published fee |
| Extended-part loading, 18 types × $3.07 | 55.26 | JLC published fee |
| THT hand soldering, 440 joints × $0.0164 | 7.22 | JLC published fee |
| Hand-soldering labour | 3.58 | JLC published fee |
| Components, including attrition | 161.04 | JLC live prices, 2026-10-10 |
| **Subtotal** | **263.74** | |
| DHL (DDP) to Cyprus | about 23–30 | JLC live: $22.93 at 0.21 kg (PCB only); rises with assembled weight |
| **Total before VAT** | **about $287–294** | about $57–59 per board |

**Not included:**
- Cyprus VAT (19 %, if charged at checkout).
- Teensy 4.1 boards.
- M8 male cordsets.
- The PoE board.

JLC's checkout total is binding. Sources: [JLCPCB assembly price](https://jlcpcb.com/help/article/pcb-assembly-price), [JLC quote](https://cart.jlcpcb.com/quote).
