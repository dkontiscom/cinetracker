# Carrier v0.62 design notes

**Source of truth for pin functions:** the firmware in this repository (`include/*.h`, `src/*.cpp`). Datasheet values are cited where they are used. Anything not verified is marked **(unverified)**.

## 1. Teensy 4.1 connections

The Teensy plugs in components up, USB end on the left. The pin order comes from the PJRC pinout card rev4, top view:
- **Top row (J8):** VIN, GND, 3.3V, 23 … 13, GND, 41 … 33.
- **Bottom row (J9):** GND, 0 … 12, 3.3V, 24 … 32.

Socket pin 1 is the USB end of each row.

| Teensy pin | Net | Firmware source |
|---|---|---|
| VIN | +5V (from J7) | — |
| 3.3V (both) | +3V3 (carrier 3.3 V plane) | — |
| GND (all three) | GND | — |
| 2 / 3 | GENLOCK_PIN_BIT0 / BIT1 | `include/genlock.h` |
| 4 / 5 | STATUS_LED_SYNC / STREAM | `include/status_leds.h` |
| 6 | RESET_BUTTON | `include/reset_button.h` (INPUT_PULLUP) |
| 10 / 0 | ENCODER_CS_FOCUS / ZOOM | `include/encoder_driver.h` |
| 13 / 11 / 12 | SPI SCK / MOSI / MISO (focus) | `SPI.begin()` default pins |
| 27 / 26 / 1 | SPI1 SCK / MOSI / MISO (zoom) | `SPI1.begin()` default pins. Teensy core `SPI.cpp`: LPSPI3 MISO pin 1, MOSI 26, SCK 27 |
| 19 / 18 | I²C SCL / SDA (iris) | `Wire.begin()`, 400 kHz |
| 23 / 21 / 20 | I2S MCLK / BCLK / LRCLK | SAI1 (MCLK1, BCLK1, LRCLK1), OpenAudio I2S_F32 |
| 7 / 8 | I2S TX (to the DAC) / RX (from the ADC) | SAI1 OUT1A / IN1 |

Unused Teensy pins are left unconnected. The Teensy's 6 Ethernet pins are not used by the carrier; the PoE board plugs onto them from above.

## 2. Power

- **J7, 5 V input:** a 1×4 2.54 mm female header, 8.5 mm tall (XKB X6511FV-04-C85D32). Pins 1–2 are +5V and pins 3–4 are GND. It feeds Teensy VIN.
  - Teensy VIN accepts 3.6–5.5 V (PJRC card).
  - **Cut the Teensy's VUSB–VIN pads** so USB carries data only (PJRC: *"a pair of pads on the bottom side may be cut apart, to separate VUSB from VIN"*).
- **D6, SMAJ5.0A:** a surge clamp, and only a partial one. It starts conducting at 6.4–7.0 V and clamps at 9.2 V (at 43.5 A), both above the 6.0 V absolute maximum of the Teensy regulator (TI TLV757P, or onsemi NCV8186 on Teensy 4.1 boards made after March 2022, PJRC). So it does **not** protect against:
  - a sustained input above 6 V;
  - a reversed supply: D6 conducts, but +5V still sits at about −0.7 to −1.1 V (typical), past the PCM1808's −0.3 V minimum on VCC (via FB2);
  - a bench supply plugged in live: the ceramic bulk caps can ring to about twice the supply (ADI/LT AN88 fig. 2: 24 V into 10 µF peaks at 57.2 V).
  - In use the PoE board feeds J7, so this only matters on the bench. Bring-up rule in `ordering.md`.
- **Teensy 4.1 regulator:** PJRC: *"Teensy 4.1 manufactured after March 2022 has TLV75733P (U4) replaced by NCV8186AMN330TAG"*. Both have a 6.0 V absolute maximum on IN. The NCV8186 current limit is 1.1 A minimum, 1.4 A typical.
- **Bulk capacitors:** C29 47 µF and Cb1 10 µF on +5V; Cb2 10 µF on +3V3.
- **+3V3:** comes from the Teensy regulator. PJRC: *"The recommended maximum for external 3.3V usage is 250mA."*
- **+3V3A:** +3V3 through ferrite FB1 (600 Ω at 100 MHz), with C27 10 µF. It feeds the DAC analog supplies and the genlock op-amps.
- **+5VA:** +5V through FB2, with C28 10 µF. It feeds the PCM1808 VCC.

**3.3 V budget (estimate from datasheet maxima):**

| Load | mA |
|---|---|
| PCM5102A AVDD + CPVDD + DVDD | 45 |
| PCM1808 VDD | 8 |
| 2 × OPA356 (IQ max 11 mA each, specified at 5 V) plus output current | about 24 |
| 3 × AS5048 at 15 mA each | 45 |
| 2 LEDs, 470 Ω | about 6 |
| I²C pull-ups, XSMT pull-up, ladder | about 3 |
| **Total** | **≈ 131 mA, about 52 % of PJRC's 250 mA** |

The Teensy's own Ethernet PHY adds about 58 mA (TI DP83825I). **Measure the real current on the first boards** before sizing the PoE board's 5 V supply.

## 3. Genlock (tri-level sync) output

The signal chain:
- **Ladder:** Teensy pins 3 (BIT1, MSB) and 2 (BIT0, LSB) drive an R-2R ladder.
  - R1 and R2 are 2 k each.
  - NODE_S–NODE_B is R3 1 k + R13 11 Ω.
  - NODE_B–GND is R4 2 k + R14 22 Ω.
  - The ladder follows Riggers et al., RTCSA 2024, Fig. 3; the odd trims absorb the GPIO output resistance.
  - R1–R5 and R15 (the six resistors that set the levels) are 0.1 % thin film since v0.60: YAGEO RT0805B (2k C706274, 1k C110774, 5.1k C122968) and Panasonic ERA6AEB562V (5.6k C445654).
- **Offset:** R15 5k6 to +3V3A and R5 5k1 to GND lift the levels to 0.43 / 1.03 / 1.63 V. That keeps both op-amps inside the OPA356 common-mode range ((V−) − 0.1 V to (V+) − 1.5 V) and above its 0.3 V output-swing limit.
- **Bypass:** C2 (U1) and C34 (U7) are 10 µF 0805 at each op-amp's V+ pin since v0.62. TI's OPA356 datasheet (SBOS212A p. 10) names 10 µF ceramic as the minimum; up to v0.61 they were 100 nF.
- **Buffer:** U1 OPA356, unity gain.
- **Low-pass:** a unity-gain Sallen-Key with RF1 = RF2 = 150 Ω and CF1 = CF2 = 68 pF 1 % C0G, giving Q = 0.5 (no overshoot).
- **Line driver:** U7 OPA356.
- **DC block:** C1 47 µF, with R7 10 k bleed.
- **Output:** R6 75 Ω into J1 (Kinghelm KH-BNC75-3511, 75 Ω). D5 TPD1E10B09 (bidirectional, ±9 V) handles ESD.
- **J1 footprint (fixed in v0.62):** per the Kinghelm drawing (p. 1, side and PCB views) the two Ø1.8 mm mounting posts sit 7.5 mm behind the body front and the signal / ground leads 12.55 mm behind it. Up to v0.61 the footprint had the two rows swapped, so the part could only have gone in facing into the board. JLC's own footprint for C2837588 has the same layout as v0.62. The drawing shows one ground lead beside the centre pin; the footprint keeps a ground hole on each side, so it fits whichever side the real part has it.

**Levels at the camera** (75 Ω cable into 75 Ω): firmware states (BIT1, BIT0) 00 = −300 mV, 01 = 0 mV (blanking), 10 = +300 mV.

**Simulated performance** ([simulation/README.md](../simulation/README.md)):
- 10–90 % rise of 35.2 / 35.3 / 35.2 ns on the three edges. That is inside both SMPTE ST 274 §10 windows: 33.67–74.07 ns at 24/25/30p and 16.84–37.04 ns at 50/60p.
- Levels of −299.2 / +299.1 mV against ±300 ± 6 mV.
- **Resistor tolerance:** with 1 % parts, 96.8 % (normal spread) to 73.4 % (even spread) of boards would meet ±6 mV (Monte Carlo, `simulation/mc_levels.py`). With the 0.1 % parts used since v0.60 it is 100 % in both cases.

**What it depends on:**
- **Firmware, commit `ab11050`:** the trailing edge (10 → 01) must switch both bits at once. The firmware now does this with one write to GPIO9 `DR_TOGGLE`.
- **Firmware, commit `e25cabc`:** the vertical-sync broad pulse must end at 2112 T, which matters at 24/25/50p.
- **Camera input:** the camera must terminate the input in 75 Ω. A high-impedance input doubles the amplitude.
- **Timing offset:** the firmware's `TRISYNC_OFFSETNS` is 0. The simulated stage adds about 23 ns from GPIO edge to 0H crossing, plus the cable delay. Calibrate it against a reference.

## 4. Audio: LTC timecode

The firmware carries LTC as mono on the left channel (channel 0, `src/aes67.cpp`), with no codec control interface.

- **DAC (U3, PCM5102A, TI SLAS859C):**
  - I2S slave with MCLK = 256 fs.
  - Charge pump: C10 2.2 µF across CAPP–CAPM; C11 2.2 µF on VNEG.
  - Control pins: XSMT pulled high by R20 10 k (unmuted). FLT, DEMP and FMT are tied low (I2S format).
  - Decoupling: C17 100 nF on LDOO; C12 to C16 on the supplies.
  - Output filter: 470 Ω / 2.2 nF on both channels (R21/C18, R22/C19).
- **LTC out:** DAC_FL → R23 4.7 k → LTC_OUT, with R26 1 k to GND.
  - About 0.96 Vpp open circuit at full scale, from about 0.84 kΩ source impedance.
  - D3 TPD1E10B09 protects the line. J5 has pin 4 = signal and pins 3 and 1 = GND.
- **ADC (U2, PCM1808, TI SLES177B):**
  - Slave mode (MD0 = MD1 = 0), I2S format (FMT = 0).
  - Supplies: VCC = +5VA, VDD = +3V3.
  - VREF: C20 100 nF + C21 10 µF.
  - The unused VINR is AC-grounded through C30 1 µF, so it keeps its internal bias.
- **LTC in:** J6 pin 4 → R24 20 k → R25 10 k to GND → C26 1 µF → VINL.
  - About 10 Vpp reaches ADC full scale. Input impedance is about 28.6 kΩ.
  - D4 TPD1E10B09 protects the line.
- **Over-voltage on LTC in/out:** any source up to ±9 V is fine indefinitely (D3/D4 working voltage, TI SLLSEB0E §6.3), which covers every TC source. Above about ±9.5 V (D4 breakdown minimum) from a stiff source, such as a pro line or speaker output, the clamp conducts with no resistor in front of it and can fail short (162 mW rating, not for steady-state breakdown, §6.4). The PCM1808 itself is safe behind R24 20 k.
- **Levels:** about 1 Vpp out and about 10 Vpp in-range are design choices.
  - Checked 2026-10-11 against published specs: Sound Devices 888 TC in is 0.3–3.0 Vpp at 20 kΩ, TC out 5 Vpp at 75 Ω ([888 brochure](https://cdn.abicart.com/shop/ws42/76342/art27/171496227-4025a3-sound_devices_888_brochure.pdf)). Our output gives about 0.92 Vpp into 20 kΩ, inside that range; our input takes its 5 Vpp at half scale.
  - ARRI (ALEXA 35, Mini LF), RED (KOMODO, V-RAPTOR) and Blackmagic (URSA) publish no LTC voltage levels in the documents checked; see the sources in `open-issues.md`.
  - The hardware sets the ceiling (about 0.96 Vpp); firmware or the AES67 timecode source can only lower it. If a camera needs more, R23 (4.7 k) is the one part to change.

## 5. Lens encoders

Every cable line runs **connector → TVS → 33 Ω → Teensy**:
- **TVS:** TPD4E1U06, 4 unidirectional channels on pins 1/3/4/6, GND on pin 2, pin 5 NC (TI datasheet).
- **Series resistors:** RS1–RS10.
- **Supply fuses:** each encoder supply has its own PTC (Bourns MF-PSMF010X: 0.1 A hold, 0.3 A trip) plus 100 nF. A dead short on one cable can briefly pull the whole 3.3 V rail down before its fuse trips (the PTC's 1.0–7.5 Ω cold resistance can draw more than the Teensy regulator's current limit), so the CineTracker may reboot once and then run without that encoder. Nothing is damaged. This is a suspicion until tested (Bourns MF-PSMF datasheet p. 1; TI TLV757P §5.5; onsemi NCV8186 p. 3, current limit 1.1 A min).
- **ESD residual (suspicion):** behind the TPD4E1U06, an 8 kV contact discharge still pushes roughly 0.25–0.8 A for a few hundred ns through the 33 Ω into the Teensy pin (TI SLVSBQ9D fig. 3, clamp at about 12–30 V). That's about what the RT1062's own 1 kV HBM rating covers (NXP IMXRT1060CEC table 7). The recessed female M8 contacts help. A larger RS (68–100 Ω) would cut it 2–3×, but check the SPI and I²C timing first; left for the PCB designer review.
- **Cable shorts and foreign voltages:** a shorted signal wire loads a Teensy pin with about 60 mA, and a 5 V signal passes the clamps (6.5 V minimum breakdown). The cable doc covers both: `encoder-cables.md`, "Before you plug it in".

| Connector | Encoder | Pins |
|---|---|---|
| J3 focus, M8 6-pin A-coded female | AS5048A, SPI, 8 MHz MODE1 | 1 +3V3 (via F1), 2 GND, 3 SCK, 4 MOSI, 5 MISO, 6 CS |
| J4 zoom, M8 6-pin A-coded female | AS5048A, SPI1 | 1 +3V3 (via F2), 2 GND, 3 SCK, 4 MOSI, 5 MISO, 6 CS |
| J2 iris, M8 4-pin A-coded female | AS5048B, I²C 400 kHz, address 0x40 | 1 +3V3 (via F3), 2 SDA, 3 GND, 4 SCL |

**Wiring and timing notes:**
- **Cordsets:** the sockets are female, so cordsets need a **male** M8 plug at the board end. Colours vary by maker; the pin number is what counts.
- **Encoder supply:** run the AS5048 in 3.3 V mode, with VDD5V tied to VDD3V (ams DS000298). At the encoder put 100 nF + 10 µF, and tie the TEST pins to GND. For the AS5048B, A1 and A2 go to GND for address 0x40.
- **PTC drop:** worst case R1max 7.5 Ω × 15 mA = 0.11 V, so the encoder sees at least 3.19 V.
- **I²C:** R11 and R12 are 2 k pull-ups. NXP UM10204 Fast-mode needs tr ≤ 300 ns, so Rp(max) = tr / (0.8473 × Cb), which allows Cb up to 177 pF.
- **SPI:** AS5048A needs tCLK ≥ 100 ns, and the firmware runs at 8 MHz. Cable length is designed for 40 cm or less.
- **MISO pull-ups (v0.60):** R27 and R28, 10 k to +3V3 on SPI_MISO and SPI1_MISO (Teensy side of RS3/RS7). With no encoder plugged in, the line reads 0xFFFF, which has the error flag (bit 14) set, so `encoder_detect_presence()` reports the input as not connected instead of reading a floating line. Detection runs once at power-up (decided 2026-10-11).

## 6. Status UI

- **D1 (sync):** KT-0805YG, Teensy pin 4, through R9 470 Ω.
- **D2 (stream):** KT-0805Y, pin 5, through R10 470 Ω.
- **Placement:** both LEDs and SW1 sit in front of the Teensy so light pipes and a plunger can reach the lid.
- **SW1 (reset):** XKB TS-1187A, to GND, on pin 6. Holding it for 5 s is a factory reset (firmware `RESET_BUTTON_HOLD_MS`).

## 7. Layout

- **Layers:** In1 is a solid GND plane and In2 a solid +3V3 plane. +5V is a routed track in the "Power" net class (0.5 mm), about 103 mm long.
- **Under the Teensy:** the codecs and their passives sit there, which is why the sockets are 8.5 mm tall.
- **Unprotected tracks away from the panel connectors (v0.62, review C):** Teensy-side signals with no clamp (RESET_BUTTON, and SPI1_MISO on the Teensy side of RS7) no longer pass through the J3/J5/J6 pin fields. RESET_BUTTON runs on B.Cu under the J9 row, SPI1_MISO on B.Cu below the RS row. No Teensy-side track or via is within 1.5 mm of any J1–J6 pad now. TI's layout rule for both TVS parts: keep unprotected traces away from the protected ones between the TVS and the connector (SLVSBQ9D §10.1, SLLSEB0E §8.4.1).
- **JLC limits in DRC (v0.62, review D1):** `cinetracker-carrier.kicad_dru` encodes JLCPCB's published limits for this order (4 layers, 1 oz / 0.5 oz, Economic PCBA): track 0.09 mm, PTH annular ring 0.15 mm, hole-to-hole 0.45 mm, PTH hole to copper 0.28 mm (0.3 mm inner), silk text 1.0 / 0.15 mm, pad-to-silk 0.15 mm, no via hole within 0.1 mm of an SMD pad, SMD parts ≥ 2.5 mm from the edge. Board Setup: minimum clearance 0.09 mm, solder mask web 0.1 mm. Sources are in the file.
- **Thermal reliefs (v0.62):** all copper zones connect THT pads with thermal spokes (0.5 mm gap, 0.5 mm spokes), so JLC's hand soldering can fill the barrels. SMD pads and vias stay solid. Three GND pads with no room for two spokes (J5 MP, J8 pin 2, J9 pin 1) stay solid by a rule in the same file.
- **Genlock node:** the high-impedance ladder node (GENLOCK_NODE_S) is about 11 mm of copper.
- **Panel connectors:** the M8 flange rear face sits on the board edge. The BNC overhangs the edge by 3.0 mm so its face lines up with the M8s.
- **Panel holes:**
  - M8 sockets: M10 × 0.75, Ø10.1 mm with a 9.1 mm flat.
  - BNC: 1/2-28 UNEF, Ø12.8 mm D-cut.
- **PoE board (v0.61):**
  - **Position:** it sits about 18.9 mm above the carrier (suspicion until measured), on the Teensy Ethernet header and J7.
  - **Standoffs:** MH5 (77.0, 48.1) and MH6 (10.0, 48.5) are M2.5 NPTH holes for nylon standoffs, each with a 3.2 mm copper keep-out.
  - **Clearance:** no carrier part under it is taller than the panel connectors (about 13 mm).
  - **Details:** see `../../poe/documentation/design-notes.md`.
- **Connector courtyards (v0.60):** the BNC and M8 footprints now carry the body outline from the vendor drawings as courtyard (BNC: 14.7 ± 0.3 wide, block 14.5 deep; CAZN M8: 12.5 ± 0.1 wide, 8.13 deep from the board edge; dosinconn M8: about 13 wide, about 8.85 deep, read off the drawing), plus the mounting-post pads. Before v0.60 they had none, so DRC could not see a part under a connector body. Closest existing neighbours: C34 is 1.8 mm from the J5 body, and the PoE standoff at MH6 is 3.5 mm from the BNC body (standoff radius about 2.9 mm).

## Sources

- **PJRC:**
  - [Teensy 4.1](https://www.pjrc.com/store/teensy41.html)
  - [pinout card rev4](https://www.pjrc.com/teensy/card11a_rev4_web.pdf)
  - [dimension drawing](https://www.pjrc.com/teensy/dimensions_teensy41.png)
- **TI datasheets:**
  - [PCM5102A SLAS859C](https://www.ti.com/lit/ds/symlink/pcm5102a.pdf)
  - [PCM1808 SLES177B](https://www.ti.com/lit/ds/symlink/pcm1808.pdf)
  - [OPA356](https://www.ti.com/lit/ds/symlink/opa356.pdf)
  - [TPD4E1U06](https://www.ti.com/lit/ds/symlink/tpd4e1u06.pdf)
  - [TPD1E10B09](https://www.ti.com/lit/ds/symlink/tpd1e10b09.pdf)
  - [DP83825I](https://www.ti.com/lit/ds/symlink/dp83825i.pdf)
  - [TLV757P](https://www.ti.com/lit/ds/symlink/tlv757p.pdf)
- **Other datasheets and notes:**
  - [onsemi NCV8186](https://www.onsemi.com/pdf/datasheet/ncv8186-d.pdf) (Teensy 4.1 regulator since March 2022)
  - [NXP i.MX RT1060 IMXRT1060CEC](https://www.nxp.com/docs/en/data-sheet/IMXRT1060CEC.pdf): table 7 (ESD HBM 1000 V), table 27 (pin driver 23 Ω)
  - [Littelfuse SMAJ5.0A, LCSC C83329 (datasheet linked there)](https://www.lcsc.com/product-detail/esd-and-surge-protection-tvs-esd_littelfuse-smaj5-0a_C83329.html): p. 2 SMAJ5.0A row, p. 4 fig. 8 (forward voltage)
  - [ADI/LT AN88, ceramic input capacitors can cause overvoltage transients](https://www-cce.cldnet.analog.com/media/en/technical-documentation/application-notes/an88f.pdf), fig. 2
- **Other parts and standards:**
  - [ams AS5048 DS000298 (mirror)](https://www.basecamelectronics.com/files/Encoders/AS5048_DS000298_3-00.pdf)
  - [NXP UM10204 (mirror)](https://www.pololu.com/file/0J435/UM10204.pdf)
  - [Bourns MF-PSMF](https://www.bourns.com/docs/Product-Datasheets/mfpsmf.pdf)
  - [SMPTE ST 274:2008](https://pub.smpte.org/latest/st274/st0274-2008.pdf)
- Riggers, Schleusner, Renke, Blume, *PTP-Synchronized Tri-Level Sync Generation for Networked Multi-Sensor Systems*, RTCSA 2024, Fig. 3 (the reference ladder circuit; see `CREDITS.md`).
