# Enclosure requirements (from the boards)

What a box for the carrier + Teensy + PoE stack has to provide. This isn't the enclosure design (that's still to do: a bent aluminium sheet box plus a 3D-printed version, decided). It's what the boards and connectors force on it. From review E (2026-10-14), against carrier and PoE v0.62.

Coordinates are the carrier's: x along the panels (0–80), y from the rear panel (y = 0) to the front panel (y = 64), z = height above the carrier top. Several numbers come from stand-in parts because the real ones have no drawing yet; those are marked **suspicion**.

## Panels

| Panel | Connectors (centre x, axis height z) | Cut-out |
|---|---|---|
| Front, board edge y = 64 | J1 BNC (x 15, z ≈ 6.6, **suspicion**: scaled), J5 LTC out (x 44, z 6.3), J6 LTC in (x 65, z 6.3), PoE J1 M12 (x 41, z 29.2) | BNC Ø12.8 D-cut, flat 5.5 from centre on top; M8 Ø10.1 / 9.1 double-D; M12 see below |
| Rear, board edge y = 0 | J3 focus (x 17), J4 zoom (x 40), J2 iris (x 63), all z 6.3 | M8 Ø10.1 / 9.1 double-D |

Drawings, seen from outside: [front](enclosure/front-panel-y64.png), [rear](enclosure/rear-panel-y0.png), [lid](enclosure/lid-top-view.png) (SVGs next to them). They're requirement views, not fabrication drawings.

- **One end panel must be a separate plate.** Connectors stick out of both board edges (rear 12–12.4 mm, front up to 24 mm for the BNC), so the stack can't be lowered between two fixed end panels. Build and test the whole stack first, then slide the panels on along y.
- **Panel inner faces** sit about 3.0 mm outside each board edge: the BNC block face is 3.0 mm out (footprint), the M8 flanges 2.0 mm plus their O-ring. Nominal inner panel spacing 70.0 mm; only the M8 O-rings take up the box tolerance.
- **M12 hole: make it a vertical slot** (12.1 wide × about 14.5 high) or an oversize hole that the M12 flange and O-ring cover. The M12 sits on the PoE board at z ≈ 29.2, at the top of a stack of seven tolerances (about ±1.2 mm worst case, ±0.5 typical: sockets, header plastics, Teensy, J2, PoE PCB). A tight Ø12.1 hole would make the panel nut pull the PoE board up or down and strain J2, the Teensy Ethernet header and J3. **Suspicion** until the M12 sample is measured: the C7423431 housing has no drawing, so Ø12.1 is from similar parts (CAZN M12-S3A-GPL M12 catalogue p. 40, Toponelec C12B).
- **Sheet thickness: 1.5 mm aluminium** works for every connector as far as the data shows. Rear M8 (dosinconn) take up to 4.5 mm. The front window is 1.5–2.5 mm only if the M12 housing behaves like the stand-in parts (**suspicion**). The narrowest web between holes (J5–J6, 10.9 mm) caps the sheet at 1.8 mm with the usual ≥ 6 × thickness rule for punched holes.
- **Cordsets on J5/J6:** use straight plugs, or right-angle ones with the cable pointing down. A right-angle M8 plug on J5 with the cable up hits the M12 plug (**suspicion**: head size from a P+F right-angle part).

## Height

- **Lid inner face ≥ 40.5 mm above the carrier top.** The M12's hex housing reaches about 37.8 mm (**suspicion**, stand-in parts have a 15 mm hex), and the bend above the M12 hole needs its distance (about 2.5 × thickness + bend radius at 1.5 mm sheet).
- **Floor:** the BNC leads stick out 2.4–3.0 mm under the carrier and its hole reaches nearly to the carrier level, so put the carrier on **≥ 5 mm standoffs** (MH1–MH4, M3). Only GND copper sits within 4 mm of those holes, and GND = chassis (decided), so metal standoffs are fine there.
- **PoE standoffs (MH5/MH6 ↔ H1/H2):** the gap is 18.87 mm nominal (18.0–19.7 worst case). No stock part has that length: use an 18 mm M2.5 nylon standoff (for example Würth 970180155) plus a 0.8–1.0 mm nylon washer, set after measuring the real stack. **Nylon**, because the holes sit in the isolation band.

## Lid: LEDs and reset button

- **Light pipes** over D1 (x 31, y 22.5) and D2 (x 39, y 22.5): about 39.5 mm long with the lid at 40.5. Up to Ø4.5 fits before the J8 socket body.
- **Reset plunger** over SW1 (x 73.5, y 18.5): the switch travels only 0.2–0.25 mm (XKB TS-1187A drawing). A rigid 39 mm plunger against a lid with ±0.5–1 mm tolerance either holds the button down all the time (5 s = factory reset at every power-up) or never reaches it. **Use a spring-returned plunger with its own stop in the lid.**
- **USB:** the Teensy USB port (x 8.8–14.5) needs a side-wall hole if you want to flash or debug with the lid on. The Teensy PROGRAM button is under the PoE board.

## Sources

- Review E report (spacing, panels, stack, assembly), 2026-10-14; panel drawings above.
- [CAZN M8-S4A-GPL-M10-JD drawing](https://wmsc.lcsc.com/wmsc/upload/file/pdf/v2/lcsc/2311011001_CAZN-M8-S4A-GPL-M10-JD_C19108970.pdf) p. 2 (M8 cut-out, axis 6.3); [dosinconn DS869FPR0470-6P](https://wmsc.lcsc.com/wmsc/upload/file/pdf/v2/lcsc/2411291132_dosinconn-DS869FPR0470-6P_C42395389.pdf) p. 1 (max panel 4.5).
- [CAZN M12-S8X-GPLM12-BJD drawing](https://wmsc.lcsc.com/wmsc/upload/file/pdf/v2/lcsc/2307120959_CAZN-M12-S8X-PPL_C7423431.pdf) (axis 8.71, insert 14.14 above the PCB); [CAZN 2025 catalogue](https://www.cazn.com/uploads/upload/20250922/cb22fb901b11a3fa29d9710860ef2cf9.pdf) p. 40 (A-coded M12 stand-in); [Toponelec C12B-MAX-RM-8-SH drawing](https://datasheet.lcsc.com/datasheet/pdf/9538ace0f291c4b99dfb7a55c546ba1f.pdf?productCode=C22387824) (cut-out Ø12.1, panel ≤ 2.5).
- [SDAPO DP9900 datasheet](https://wmsc.lcsc.com/wmsc/upload/file/pdf/v2/lcsc/2305061824_SDAPO-DP9900LP-5V_C5380108.pdf) p. 8 (DP9900LP 8.00 ± 0.5 above the PCB, spacer included).
- [PJRC Teensy 4.1 dimensions](https://www.pjrc.com/teensy/dimensions_teensy41.png); [Würth 970180155 at RS](https://uk.rs-online.com/web/p/standoffs/2052942).
