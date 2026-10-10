# Carrier v0.5d design notes

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
- **D6, SMAJ5.0A:** clamps hot-plug and surge transients. It conducts if the supply is reversed. Its 6.4–7.0 V breakdown is above the Teensy LDO's 6.0 V absolute maximum (TI TLV757P), so it does **not** protect against a sustained over-voltage.
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
- **Offset:** R15 5k6 to +3V3A and R5 5k1 to GND lift the levels to 0.43 / 1.03 / 1.63 V. That keeps both op-amps inside the OPA356 common-mode range ((V−) − 0.1 V to (V+) − 1.5 V) and above its 0.3 V output-swing limit.
- **Buffer:** U1 OPA356, unity gain.
- **Low-pass:** a unity-gain Sallen-Key with RF1 = RF2 = 150 Ω and CF1 = CF2 = 68 pF 1 % C0G, giving Q = 0.5 (no overshoot).
- **Line driver:** U7 OPA356.
- **DC block:** C1 47 µF, with R7 10 k bleed.
- **Output:** R6 75 Ω into J1 (Kinghelm KH-BNC75-3511, 75 Ω). D5 TPD1E10B09 (bidirectional, ±9 V) handles ESD.

**Levels at the camera** (75 Ω cable into 75 Ω): firmware states (BIT1, BIT0) 00 = −300 mV, 01 = 0 mV (blanking), 10 = +300 mV.

**Simulated performance** ([simulation/README.md](../simulation/README.md)):
- 10–90 % rise of 35.2 / 35.3 / 35.2 ns on the three edges. That is inside both SMPTE ST 274 §10 windows: 33.67–74.07 ns at 24/25/30p and 16.84–37.04 ns at 50/60p.
- Levels of −299.2 / +299.1 mV against ±300 ± 6 mV.

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
- **Levels (unverified):** no public LTC level specification was checked. The about 1 Vpp out and about 10 Vpp in-range are design choices. To change them, change R23/R26 and R24/R25.

## 5. Lens encoders

Every cable line runs **connector → TVS → 33 Ω → Teensy**:
- **TVS:** TPD4E1U06, 4 unidirectional channels on pins 1/3/4/6, GND on pin 2, pin 5 NC (TI datasheet).
- **Series resistors:** RS1–RS10.
- **Supply fuses:** each encoder supply has its own PTC (Bourns MF-PSMF010X: 0.1 A hold, 0.3 A trip) plus 100 nF. A shorted cable only trips its own fuse.

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

## 6. Status UI

- **D1 (sync):** KT-0805YG, Teensy pin 4, through R9 470 Ω.
- **D2 (stream):** KT-0805Y, pin 5, through R10 470 Ω.
- **Placement:** both LEDs and SW1 sit in front of the Teensy so light pipes and a plunger can reach the lid.
- **SW1 (reset):** XKB TS-1187A, to GND, on pin 6. Holding it for 5 s is a factory reset (firmware `RESET_BUTTON_HOLD_MS`).

## 7. Layout

- **Layers:** In1 is a solid GND plane and In2 a solid +3V3 plane. +5V is a routed track in the "Power" net class (0.5 mm), about 103 mm long.
- **Under the Teensy:** the codecs and their passives sit there, which is why the sockets are 8.5 mm tall.
- **Genlock node:** the high-impedance ladder node (GENLOCK_NODE_S) is about 11 mm of copper.
- **Panel connectors:** the M8 flange rear face sits on the board edge. The BNC overhangs the edge by 3.0 mm so its face lines up with the M8s.
- **Panel holes:**
  - M8 sockets: M10 × 0.75, Ø10.1 mm with a 9.1 mm flat.
  - BNC: 1/2-28 UNEF, Ø12.8 mm D-cut.

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
- **Other parts and standards:**
  - [ams AS5048 DS000298 (mirror)](https://www.basecamelectronics.com/files/Encoders/AS5048_DS000298_3-00.pdf)
  - [NXP UM10204 (mirror)](https://www.pololu.com/file/0J435/UM10204.pdf)
  - [Bourns MF-PSMF](https://www.bourns.com/docs/Product-Datasheets/mfpsmf.pdf)
  - [SMPTE ST 274:2008](https://pub.smpte.org/latest/st274/st0274-2008.pdf)
- Riggers, Schleusner, Renke, Blume, *PTP-Synchronized Tri-Level Sync Generation for Networked Multi-Sensor Systems*, RTCSA 2024, Fig. 3 (the reference ladder circuit; see `CREDITS.md`).
