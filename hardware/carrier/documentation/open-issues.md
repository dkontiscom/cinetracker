# Carrier: open issues

Updated 2026-10-13 for v0.62 (in review). Most items can only be closed with real boards and a scope.

## Firmware items that affect the board

| # | Item | Status |
|---|---|---|
| 1 | The trailing tri-level edge (10 → 01) was two GPIO writes, so the output passed through −300 mV between them | **Fixed** in commit `ab11050`: one store to GPIO9 `DR_TOGGLE`. Still to be measured |
| 2 | The vsync broad pulse ran 440–550 T too long at 24/25/50p (ST 274 §10.6, bp = 2112 T) | **Fixed** in commit `e25cabc`. Still to be measured |
| 3 | `TRISYNC_OFFSETNS` = 0 | Open. The board adds about 23 ns (simulated) plus the cable delay; calibrate against a reference. The upstream note *"64 vs 100 jumps more than 36?! Interrupt collision?"* is unresolved, so measure 0H jitter |
| 4 | The sync ISR busy-waits at NVIC priority 0, about 2.0 µs per line at 30p and 1.1 µs at 60p. Since `e25cabc` it also busy-waits about 30 µs (24/25p) or 15 µs (50p) on the 5 vsync lines | **Rework planned (decided 2026-10-10):** edges from FlexPWM4 submodule 2 (pins 2/3) instead of busy-waits. Measure the current code first, as a baseline. See `GENLOCK-TIMING.md` at the repository root |
| 5 | `noInterrupts()` in `encoder_driver.cpp` also masks the sync ISR | Open. The window is short; include it in the jitter measurement |
| 6 | Segment cycle counts are truncated (about 1 ns short) | No change needed. ST 274 allows 44 T ± 3 T |
| 7 | Library versions were not pinned | **Fixed** in commit `79568e2` |

## Bench checks for the first boards

- [ ] **Tri-level output into a real 75 Ω load:** ±300 mV ± 6 mV, and all three edges between 33.7 and 37.0 ns at 25p and at 60p. Any scope of 100 MHz or more adds less than 0.5 % to a 35 ns rise time.
- [ ] **Camera genlock input termination:** the design assumes 75 Ω at the camera. A high-impedance or loop-through input without a terminator doubles the levels.
- [ ] **Lock test** on each real camera at 24, 25, 30, 50 and 60p, including vertical sync.
- [ ] **Current draw:** measure 3.3 V against PJRC's 250 mA recommendation, and the 5 V input current. The PoE board's supply is sized from this.
- [ ] **LTC levels** in and out with real recorders and cameras.
- [ ] **Encoders on 40 cm cables:** SPI at 8 MHz, I²C at 400 kHz (check the rise time with the 2 k pull-ups).
- [ ] **Unplugged encoder inputs:** with R27/R28 fitted, an empty focus or zoom input must read 0xFFFF and show as not detected at power-up.

## Known limits

- D6 (SMAJ5.0A) only handles transients. A sustained input above 6 V on J7 exceeds the Teensy regulator's absolute maximum.
- The genlock simulation uses TI's typical OPA356 model, with no corner models. The margin to the 50/60p limit (37.04 ns) is 0.33 ns at RC +4 %, and 1 % parts move RC by at most ±2 %.
- **Level resistors (done in v0.60):** R1–R5 and R15 are 0.1 % thin film (2k C706274, 1k C110774, 5.1k C122968, 5.6k C445654). With 1 % parts only 96.8 % (normal spread) to 73.4 % (even spread) of boards would meet ±6 mV; with 0.1 % it is 100 % (Monte Carlo, `simulation/mc_levels.py`). Cost: four more JLC extended part types, about $15 more per 5-board order.
- **LTC levels:** checked against the only published spec found (Sound Devices 888); ARRI, RED and Blackmagic publish none. See design-notes.md section 4 and the sources below.
- The schematic is label-based, because it was generated from the board netlist. It is correct and editable, but it is not hand-drawn.

## Before ordering

- [ ] Owner review of v0.62, then (with funding) a review by a professional PCB designer, then the order.
- [ ] Review the copper. v0.5d was a full re-route; v0.5e added the MH5/MH6 holes; v0.60 adds R27/R28 with three vias and five short tracks; v0.61 changes no copper; v0.62 moves the J1 BNC holes (footprint fix), re-routes the genlock output to it and moves D5 by 0.4 mm.
- [ ] Print `fab/carrier-v0.62-1to1-fit-check.pdf` at 100 % and fit the connectors and the Teensy (USB end on the left). Also print `../../poe/fab/poe-v0.61-stack-1to1-fit-check.pdf` for the stack.
- [ ] In JLC's placement preview, check the IC pin-1 marks and the LED polarity, and that every connector sits on its footprint: the CPL puts J1–J6 at the pad bounding-box centre and J7–J9 at pin 1.
- [x] Hardware licence: CERN-OHL-S-2.0, copyright CineTracker contributors (decided 2026-10-10).
- [x] PoE board standoff holes (MH5, MH6), added in v0.5e.
- [x] 0.1 % level resistors and MISO pull-ups, added in v0.60.
- [x] **J7 ↔ PoE board link:** a long-pin 1×4 header on the PoE board (decided 2026-10-10). J7 stays. The pin length is set after measuring the real stack.

## Sources (LTC levels, checked 2026-10-11)

- [Sound Devices 888 brochure](https://cdn.abicart.com/shop/ws42/76342/art27/171496227-4025a3-sound_devices_888_brochure.pdf): TC in 0.3–3.0 Vpp, 20 kΩ; TC out 5 Vpp, 75 Ω
- [ARRI, synchronization of ARRI cameras with 3rd party devices](https://www.arri.com/resource/blob/233578/1834cdca3692ee85ffc5f09ebca74ae0/2024-10-mrps-synchronization-of-arri-cameras-ti-data.pdf): no LTC levels
- [RØDE, timecode on ALEXA 35 / Mini LF](https://rode.com/en-gb/about/news-info/using-timecode-on-the-arri-alexa-35-and-mini-lf-with-wireless-pro): TC port is LEMO 5-pin, no levels
- [RED KOMODO-X extension port](https://docs.red.com/955-0219/955-0219_V1.0%20Rev-A%20RED%20PS,%20KOMODO-X%20Operation%20Guide/Content/B_MechDrawings/X_Extension_Port.htm): "Timecode In – SMPTE 12M", no levels
- [Blackmagic URSA Cine tech specs](https://www.blackmagicdesign.com/products/blackmagicursacine/techspecs/W-URSA-60): no levels
- [Ambient FAQ, URSA Mini Pro](https://faq.ambient.de/hc/en-001/articles/23095962224530-URSA-Mini-Pro): LTC and tri-level share the REF BNC on that camera
