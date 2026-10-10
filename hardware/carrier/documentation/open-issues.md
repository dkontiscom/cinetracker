# Carrier: open issues

Updated 2026-10-10 for v0.5e. Most items can only be closed with real boards and a scope.

## Firmware items that affect the board

| # | Item | Status |
|---|---|---|
| 1 | The trailing tri-level edge (10 → 01) was two GPIO writes, so the output passed through −300 mV between them | **Fixed** in commit `ab11050`: one store to GPIO9 `DR_TOGGLE`. Still to be measured |
| 2 | The vsync broad pulse ran 440–550 T too long at 24/25/50p (ST 274 §10.6, bp = 2112 T) | **Fixed** in commit `e25cabc`. Still to be measured |
| 3 | `TRISYNC_OFFSETNS` = 0 | Open. The board adds about 23 ns (simulated) plus the cable delay; calibrate against a reference. The upstream note *"64 vs 100 jumps more than 36?! Interrupt collision?"* is unresolved, so measure 0H jitter |
| 4 | The sync ISR busy-waits at NVIC priority 0, about 2.0 µs per line at 30p and 1.1 µs at 60p. Since `e25cabc` it also busy-waits about 30 µs (24/25p) or 15 µs (50p) on the 5 vsync lines | Open. Run a soak test at each rate with AES67 audio and encoders running |
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

## Known limits

- D6 (SMAJ5.0A) only handles transients. A sustained input above 6 V on J7 exceeds the Teensy regulator's absolute maximum.
- The genlock simulation uses TI's typical OPA356 model, with no corner models. The margin to the 50/60p limit (37.04 ns) is 0.33 ns at RC +4 %, and 1 % parts move RC by at most ±2 %.
- With 1 % ladder resistors, about 96.8 % of boards meet the ±6 mV level tolerance (Monte Carlo). 0.1 % resistors on R1, R2, R3, R4, R5 and R15 would raise this to about 100 %.
- The schematic is label-based, because it was generated from the board netlist. It is correct and editable, but it is not hand-drawn.

## Before ordering

- [ ] Review the copper. v0.5d was a full re-route; v0.5e only adds the MH5 and MH6 holes.
- [ ] Print `fab/carrier-v0.5e-1to1-fit-check.pdf` at 100 % and fit the connectors and the Teensy (USB end on the left). Also print `../../poe/fab/poe-v0.1-stack-1to1-fit-check.pdf` for the stack.
- [ ] In JLC's placement preview, check the IC pin-1 marks and the LED polarity.
- [ ] Decide the hardware licence (open).
- [x] PoE board standoff holes (MH5, MH6), added in v0.5e.
- [ ] **J7 ↔ PoE board link:** long-pin header or cable. This decision is open; see `../../poe/documentation/open-issues.md`.
