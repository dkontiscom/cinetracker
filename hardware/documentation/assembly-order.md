# Putting the stack together

Order for building one CineTracker from the JLC-assembled carrier and PoE boards. From review E (2026-10-14), against v0.62. It builds on "After the boards arrive" in [carrier ordering.md](../carrier/documentation/ordering.md) and [PoE ordering.md](../poe/documentation/ordering.md).

## What you fit yourself

| Part | Where | When |
|---|---|---|
| 2 × 1×24 2.54 mm male headers | Teensy 4.1 (the version **with** the Ethernet chip), soldered on the Teensy top | step 2 |
| 2×3 2.0 mm male header, pins up | Teensy Ethernet pads, soldered on the Teensy **bottom** | step 2, before the Teensy goes into the carrier (afterwards its bottom faces the carrier) |
| Cut the VUSB–VIN pads | Teensy bottom | step 2 |
| J2, 2×3 2.0 mm socket | PoE board bottom, soldered from the top | step 4 |
| J3, long-pin 1×4 header | PoE board bottom, soldered from the top | step 6, after measuring |
| R6 (47 Ω 2512 1 W), only if needed | PoE board top | step 4 or after measuring the load |
| Nylon M2.5 standoffs and screws | MH5/MH6 ↔ H1/H2 | step 5 |

## Steps

1. **Carrier alone.** Bench supply rule (polarity, about 0.5 A limit, output off, connect, switch on). 5 V on J7; check +5V on J8 pin 1 and that nothing is shorted. It draws about 1 mA.
2. **Teensy.** Cut VUSB–VIN. Solder the two 1×24 headers (the carrier sockets make a good jig). Solder the 2×3 2.0 mm Ethernet header, pins up. **Do it now:** once the Teensy is in the carrier you can't reach its bottom. Don't fit a header on the USB Host pins.
3. **Teensy on the carrier**, USB end left, powered from the bench supply on J7: after the VUSB–VIN cut, USB alone doesn't power it. Flash the firmware and do the carrier bench checks. Use the PROGRAM button now if you need it; later it's under the PoE board.
4. **PoE board alone.** Solder J2. Power it from a PoE switch with at least 100 mA of load (47 Ω 1 W on the R6 footprint), check D4 and the 5 V. If you hipot it, impulse test only.
5. **Measure the stack.** Seat the PoE board on the Teensy Ethernet header (J2) with the standoffs, without J3. Measure the PoE underside height at J3/J7 and at H1/H2. Pick the J3 pin length and the standoff shims from that.
6. **Solder J3.** Cut the pins to length first. Then put the PoE board on the stack, with J3's pins sitting in carrier J7 as the jig, and solder from the top, pins 1 to 4. **Pin 4 is 1 mm from the 8 mm-tall PS1 module:** use a fine conical tip at about 45°, coming from the J3 side (−x), so the iron doesn't touch PS1's plastic. Or order the PoE board from JLC without PS1, fit J3 first and PS1 after.
7. **Full stack on PoE power.** Link, PTP, genlock and LTC; compare noise on PoE power and on bench power; scope 5 V and 3.3 V at power-on (both open-issues files).
8. **Enclosure** ([enclosure-requirements.md](enclosure-requirements.md)). Fit the O-rings, slide the front panel over J1/J5/J6 and the M12 and the rear panel over J2–J4, tighten the nuts (the M12 hole needs its vertical play). Carrier on ≥ 5 mm standoffs. Lid with the light pipes and a spring-returned reset plunger.

## Later access

- The Teensy PROGRAM button is under the PoE board. To recover a hung bootloader, take the PoE board off: two nylon screws, then pull it off J2 and J3.
- The USB port stays reachable from the side.

## Sources

- Review E report, 2026-10-14 (hand assembly, stack).
- [SDAPO DP9900 datasheet](https://wmsc.lcsc.com/wmsc/upload/file/pdf/v2/lcsc/2305061824_SDAPO-DP9900LP-5V_C5380108.pdf) p. 8 (PS1 8.00 mm tall); PoE board file (J3 pin 4 at (47.12, 45.20), PS1 body edge x = 48.95).
- [PJRC Teensy 4.1](https://www.pjrc.com/store/teensy41.html): VUSB–VIN pads, TEENSY41 vs TEENSY41_NE.
