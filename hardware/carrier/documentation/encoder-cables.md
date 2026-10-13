# Encoder cables

How to wire the three lens encoders to the carrier. You build these cables yourself: a ready-made M8 cordset on the box end, soldered to the ams adapter board on the encoder end.

Checked against carrier v0.62 (schematic sheet "Encoders") and the ams adapter board manual AS5048-AB-1.1. Sources at the end.

## What you need

| Input | Box socket | Encoder board | Interface | Cable |
|---|---|---|---|---|
| Focus | J3, M8 6-pin | ams **AS5048A-TS_EK_AB** (AS5048A adapter board) | SPI | M8 6-pin **male**, A-coded, single-ended cordset |
| Zoom | J4, M8 6-pin | ams **AS5048A-TS_EK_AB** | SPI | same as focus |
| Iris | J2, M8 4-pin | ams **AS5048B-TS_EK_AB** (AS5048B adapter board) | I²C | M8 4-pin **male**, A-coded, single-ended cordset |

- The sockets on the box are female, so the cable needs a male plug.
- Keep each cable at **40 cm or less**. That's what the design assumes; not tested on a bench yet.
- Focus and zoom cables are the same. Iris is different, so don't swap them: the panel labels tell them apart.
- Any mix of encoders works. Plug in only the ones you use.
- Order the adapter boards early. On 2026-10-13 LCSC had none, and RS showed them back-ordered until 7 Dec (AS5048A) and 9 Dec 2026 (AS5048B). Check DigiKey, Mouser and Farnell too.

## The adapter board connector (P1)

From the ams manual, table 1. Same 8 pins on the A (SPI) and B (I²C) boards; the pins have two names because they do SPI on one board and I²C on the other.

| P1 pin | Name | SPI board (A) | I²C board (B) |
|---|---|---|---|
| 1 | GND | ground | ground |
| 2 | A2/MISO | MISO, data out | address pin A2 |
| 3 | A1/MOSI | MOSI, data in | address pin A1 |
| 4 | SCL/SCK | SPI clock | I²C clock |
| 5 | SDA/CSn | chip select | I²C data |
| 6 | PWM | not used | not used |
| 7 | 3.3V | **bridge to pin 8** | **bridge to pin 8** |
| 8 | 5V | supply in | supply in |

**Bridge P1-7 to P1-8 on every board.** The box feeds 3.3 V, and the datasheet says: "In 3V operation the VDD3V must be shorted to VDD5V". The adapter manual says the same thing about P1-7: "Connect to VDD for 3V supply voltage".

## Focus and zoom cable (SPI, M8 6-pin)

| M8 pin | Signal (carrier) | Goes to on the AS5048A board |
|---|---|---|
| 1 | +3.3 V | P1-8, with P1-7 bridged to it |
| 2 | GND | P1-1 |
| 3 | SCK | P1-4 (SCL/SCK) |
| 4 | MOSI | P1-3 (A1/MOSI) |
| 5 | MISO | P1-2 (A2/MISO) |
| 6 | CSn | P1-5 (SDA/CSn) |
| - | - | P1-6 (PWM): leave open |

## Iris cable (I²C, M8 4-pin)

| M8 pin | Signal (carrier) | Goes to on the AS5048B board |
|---|---|---|
| 1 | +3.3 V | P1-8, with P1-7 bridged to it |
| 2 | SDA | P1-5 (SDA/CSn) |
| 3 | GND | P1-1, **and** P1-2 (A2) and P1-3 (A1) |
| 4 | SCL | P1-4 (SCL/SCK) |
| - | - | P1-6 (PWM): leave open |

**Tie A1 and A2 to GND.** The datasheet builds the I²C address from the fixed bits 10000 followed by A2 and A1. With both low that's 0x40, which is the address the firmware uses (`AS5048B_I2C_ADDRESS` in `include/encoder_driver.h`). The datasheet doesn't say what happens if they float, so don't leave them floating.

The I²C pull-ups (2 kΩ) are on the carrier, so the cable needs none.

## Wire colours

Colours vary between cordset makers, so go by the pin numbers on your cordset's datasheet, or check each wire with a meter, plug end to open end, before you solder.

## Before you plug it in

1. With the cable unplugged, check for shorts with a meter. None of these may be a short:
   - +3.3 V to GND: M8 pins 1 and 2 (6-pin) or pins 1 and 3 (4-pin).
   - Every signal pin to GND, and every signal pin to pin 1 (+3.3 V). Signal pins are 3–6 on the 6-pin, 2 and 4 on the 4-pin.
   - Why the signal pins too: nothing trips on a shorted signal wire. A chip select or clock shorted to GND or +3.3 V pulls about 60 mA out of a Teensy pin for as long as the box is on (3.3 V across the pin's 23 Ω driver plus the 33 Ω resistor). PJRC recommends 4 mA at most, and the 33 Ω resistor then runs at 92 % of its 125 mW rating.
2. Check every M8 pin against the tables above with a meter.
3. Plug the encoders in **before** powering the box. Encoders are detected once, at power-up. An input that is empty at power-up stays off until the next power cycle.
4. Power the encoders **only from the box**. Never connect another supply, or 5 V logic, to an encoder cable. The Teensy pins are not 5 V tolerant (PJRC), and the carrier's ESD clamps only start conducting at 6.5 V, so a 5 V signal goes straight through to the Teensy pin. A 5 V supply on pin 1 would back-feed the box's 3.3 V rail through the fuse.
5. An unplugged focus/zoom input reads as not connected, not as a wrong value (the carrier has pull-ups on MISO for this). Each input has its own resettable fuse (0.1 A hold, 0.3 A trip) on the 3.3 V pin, which limits what a shorted cable can draw.

## Carrier side, for reference

From the v0.62 schematic, sheet "Encoders":

- **J3 focus / J4 zoom** (dosinconn DS869FPR0470-6P): 1 = +3.3 V (via F1 / F2), 2 = GND, 3 = SCK, 4 = MOSI, 5 = MISO, 6 = CSn. Focus is on the Teensy's SPI (CS pin 10), zoom on SPI1 (CS pin 0).
- **J2 iris** (CAZN M8-S4A-GPL M10-JD): 1 = +3.3 V (via F3), 2 = SDA, 3 = GND, 4 = SCL. Teensy Wire (pins 18/19).
- Every signal line has a 33 Ω series resistor and an ESD clamp (TPD4E1U06) on the carrier.

## Sources

- [ams AS5048 adapter board manual, AS5048-AB-1.1, table 1 (copy from Arrow, on manuals.plus)](https://manuals.plus/ams/as5048-14-bit-rotary-position-sensor-with-digital-angle-and-pwm-output-manual): P1 pinout, P1-7 note
- [ams AS5048A/AS5048B datasheet, revision 1.3 (Mouser)](https://www.mouser.com/pdfdocs/AS5048_Datasheet_EN_v4.PDF): section 4.1 (3 V operation), sections 4.3 and 4.3.4 and figure 13 (I²C address from A1/A2)
- [PJRC Teensy 4.1](https://www.pjrc.com/store/teensy41.html): "The recommended maximum output current is 4mA", "The pins are not 5V tolerant"
- [NXP i.MX RT1060 datasheet IMXRT1060CEC](https://www.nxp.com/docs/en/data-sheet/IMXRT1060CEC.pdf), table 27: 23 Ω output driver at full drive strength (DSE 111)
- [TI TPD4E1U06](https://www.ti.com/lit/ds/symlink/tpd4e1u06.pdf), p. 5: breakdown voltage 6.5 V minimum
- [33 Ω resistor RS1–RS10, LCSC C17634](https://www.lcsc.com/product-detail/chip-resistor-surface-mount_uni-royal-uniroyal-elec-0805w8f330jt5e_C17634.html): 125 mW
- Adapter board stock, read 2026-10-13: [RS UK 201-8309 (AS5048A board)](https://uk.rs-online.com/web/p/sensor-development-tools/2018309), [RS IE 201-8314 (AS5048B board)](https://ie.rs-online.com/web/p/sensor-development-tools/2018314); LCSC C17375201 / C17547840 (0 in stock)
- [AS5048A-TS_EK_AB at DigiKey](https://www.digikey.com/en/products/detail/ams/AS5048A-TS_EK_AB/3188612), [AS5048B-TS_EK_AB at DigiKey](https://www.digikey.com/en/products/detail/ams-osram-usa-inc/AS5048B-TS-EK-AB/3188613)
