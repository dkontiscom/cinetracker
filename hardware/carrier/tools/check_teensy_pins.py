"""Independent check: Teensy 4.1 pins on the carrier vs PJRC card + firmware.
usage: python3 check_teensy_pins.py ../cinetracker-carrier.kicad_pcb   (needs KiCad 10's pcbnew python module)
J8 = VIN-row socket, J9 = GND/0/1-row socket.
Source of the pin order: PJRC pinout card card11a_rev4 (top view, USB on the left):
top row VIN GND 3.3V 23..13 GND 41..33, bottom row GND 0..12 3.3V 24..32."""
import pcbnew, sys
B=sys.argv[1]
TOP=["VIN","GND","3V3"]+[str(n) for n in range(23,12,-1)]+["GND"]+[str(n) for n in range(41,32,-1)]
BOT=["GND"]+[str(n) for n in range(0,13)]+["3V3"]+[str(n) for n in range(24,33)]
assert len(TOP)==24 and len(BOT)==24
FW={"VIN":"+5V","3V3":"+3V3","GND":"GND",
    "2":"GENLOCK_PIN_BIT0","3":"GENLOCK_PIN_BIT1",           # include/genlock.h
    "4":"STATUS_LED_SYNC","5":"STATUS_LED_STREAM",          # include/status_leds.h
    "6":"RESET_BUTTON",                                      # include/reset_button.h
    "10":"ENCODER_CS_FOCUS","0":"ENCODER_CS_ZOOM",           # include/encoder_driver.h
    "11":"SPI_MOSI","12":"SPI_MISO","13":"SPI_SCK",          # SPI default pins
    "26":"SPI1_MOSI","1":"SPI1_MISO","27":"SPI1_SCK",        # SPI1 default pins (core SPI.cpp lpspi3: MISO 1, MOSI 26, SCK 27)
    "18":"WIRE_SDA","19":"WIRE_SCL",                         # Wire default pins
    "7":"I2S_TX","8":"I2S_RX","23":"I2S_MCLK","21":"I2S_BCLK","20":"I2S_LRCLK"}  # SAI1 (OUT1A, IN1, MCLK1, BCLK1, LRCLK1)
b=pcbnew.LoadBoard(B); bad=0
pads=[p for f in b.GetFootprints() if f.GetReference() in ("J8","J9") for p in f.Pads()]
ys=sorted({round(pcbnew.ToMM(p.GetPosition().y),2) for p in pads}); assert len(ys)==2, ys
top_y,bot_y=ys
x0=min(pcbnew.ToMM(p.GetPosition().x) for p in pads)
for p in pads:
    x=pcbnew.ToMM(p.GetPosition().x); y=round(pcbnew.ToMM(p.GetPosition().y),2)
    i=round((x-x0)/2.54); name=(TOP if y==top_y else BOT)[i]
    want=FW.get(name,"")
    got=p.GetNetname()
    if got.startswith("unconnected-"): got=""   # no-connect pin, KiCad 10 gives it its own unconnected-... net
    if got!=want:
        bad+=1; print(f"MISMATCH {p.GetParent().GetReference()}.{p.GetNumber()} Teensy {name}: board '{got}' expected '{want}'")
# USB end must be the low-x end and the USB end must be where VIN/GND sit
print("rows y:",ys,"USB end x:",round(x0,2),"| mismatches:",bad)
sys.exit(1 if bad else 0)
