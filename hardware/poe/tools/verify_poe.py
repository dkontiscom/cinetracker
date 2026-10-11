#!/usr/bin/env python3
"""Checks of the PoE board project: ERC, DRC with schematic parity (KiCad's own 'Update PCB from Schematic'
check), an independent netlist compare, every primary-to-isolated gap below 4 mm, the copper nearest M12 pin 9, and
the mechanical match with the carrier (J3 on J7, J2 on the Teensy Ethernet pins, standoff holes clear of carrier copper).
Needs KiCad 10 (kicad-cli and the pcbnew python module).  Run: python3 tools/verify_poe.py
kicad-cli runs on a temporary copy of the project, so nothing is written into the repo."""
import math, os, re, shutil, subprocess, sys, tempfile
import pcbnew
HERE = os.path.dirname(os.path.abspath(__file__))
P = os.path.dirname(HERE)
CAR = os.path.join(P, "..", "carrier", "cinetracker-carrier.kicad_pcb")
PCB = os.path.join(P, "cinetracker-poe.kicad_pcb")
tmp = tempfile.mkdtemp()
W = os.path.join(tmp, "poe"); shutil.copytree(P, W, ignore=shutil.ignore_patterns("fab", "documentation", "*.kicad_prl"))
ok = True
def cli(*a): return subprocess.run(["kicad-cli", *a], capture_output=True, text=True)
def found(t, what): m = re.search(r"Found (\d+) " + what, t); return int(m.group(1)) if m else -1

# 1. ERC
rep = os.path.join(tmp, "erc.txt"); cli("sch", "erc", "--severity-all", "-o", rep, os.path.join(W, "cinetracker-poe.kicad_sch"))
t = open(rep).read(); ne = int(re.search(r"ERC messages: (\d+)", t).group(1))
print(f"1. ERC: {ne} messages"); ok &= ne == 0

# 2. DRC with the project rules and schematic parity
rep = os.path.join(tmp, "drc.txt"); r = cli("pcb", "drc", "--schematic-parity", "--severity-all", "-o", rep, os.path.join(W, "cinetracker-poe.kicad_pcb"))
nv, nu, npar = found(r.stdout, "violations"), found(r.stdout, "unconnected"), found(r.stdout, "schematic parity")
print(f"2. DRC: {nv} violations, {nu} unconnected, {npar} schematic parity issues"); ok &= nv == 0 and nu == 0 and npar == 0

# 3. independent netlist compare
net = os.path.join(tmp, "s.net"); cli("sch", "export", "netlist", "-o", net, os.path.join(W, "cinetracker-poe.kicad_sch"))
r = subprocess.run([sys.executable, os.path.join(HERE, "cmp_netlist.py"), net, PCB], capture_output=True, text=True)
print("3. schematic vs board pads:", r.stdout.strip().splitlines()[-1]); ok &= r.returncode == 0

# 4. isolation: every primary <-> isolated gap below 4 mm
dru = os.path.join(W, "cinetracker-poe.kicad_dru"); _r = open(dru).read(); open(dru, "w").write(_r.replace("(min 2.6mm)", "(min 4mm)"))
rep2 = os.path.join(tmp, "iso.txt"); cli("pcb", "drc", "-o", rep2, os.path.join(W, "cinetracker-poe.kicad_pcb"))
gaps = sorted({float(x) for x in re.findall(r"rule 'PoE isolation' clearance 4\.0000 mm; actual ([0-9.]+) mm", open(rep2).read())})
print("4. isolation gaps < 4 mm (primary <-> isolated copper):", gaps[:6], "| minimum", gaps[0] if gaps else ">= 4")
ok &= bool(gaps) and gaps[0] >= 2.6   # empty would mean the rule matched nothing (net class patterns broken)

# 4b. M12 pin 9 (housing contact, on the chassis once mounted): nearest copper of any other item
_r = open(dru).read(); open(dru, "w").write(_r.replace("(min 1.53mm)", "(min 2.5mm)"))
rep3 = os.path.join(tmp, "pin9.txt"); cli("pcb", "drc", "-o", rep3, os.path.join(W, "cinetracker-poe.kicad_pcb"))
p9 = [(float(d), a + b) for d, a, b in re.findall(r"rule 'M12 housing contact' clearance 2\.5000 mm; actual ([0-9.]+) mm\)\n.*\n(.*)\n(.*)", open(rep3).read())]
pads9 = min((d for d, s in p9 if "PTH pad" in s and " of J1" in s), default=None); trk9 = min((d for d, s in p9 if "Track" in s or "Via" in s), default=None)
print(f"4b. J1 pin 9 (M12 housing): J1 data pads {pads9} mm (the connector's own spacing), nearest track/via {trk9 if trk9 else '>= 2.5'} mm (rule >= 1.53)")
ok &= pads9 is not None and (trk9 is None or trk9 >= 1.53)

b = pcbnew.LoadBoard(PCB)
# 5. mechanical match with the carrier: J3 on carrier J7, J2 on the Teensy ethernet pins, standoff spots free
car = pcbnew.LoadBoard(CAR)
def pads(board, ref):
    f = next(f for f in board.GetFootprints() if f.GetReference() == ref)
    return {p.GetNumber(): (round(pcbnew.ToMM(p.GetPosition().x), 3), round(pcbnew.ToMM(p.GetPosition().y), 3), p.GetNetname()) for p in f.Pads()}
j7, j3 = pads(car, "J7"), pads(b, "J3")
m = all(abs(j3[k][0] - j7[k][0]) < 0.01 and abs(j3[k][1] - j7[k][1]) < 0.01 and j3[k][2] == j7[k][2] for k in j7)
print("5a. J3 pads = carrier J7 pads (position and net):", m, {k: j7[k] for k in sorted(j7)}); ok &= m
TEENSY_ETH = {"TX+": (25.77, 32.45), "TX-": (23.77, 32.45), "GND": (25.77, 30.45), "LED": (23.77, 30.45), "RX-": (25.77, 28.45), "RX+": (23.77, 28.45)}
want = {"TX+": "/ETH_TX_P", "TX-": "/ETH_TX_N", "GND": "GND", "LED": "", "RX-": "/ETH_RX_N", "RX+": "/ETH_RX_P"}
j2 = pads(b, "J2"); mm_ = True
for sig, (x, y) in TEENSY_ETH.items():
    hit = [v for v in j2.values() if abs(v[0] - x) < 0.01 and abs(v[1] - y) < 0.01]
    mm_ &= len(hit) == 1 and (hit[0][2] if not hit[0][2].startswith("unconnected-") else "") == want[sig]
print("5b. J2 pads on the Teensy ethernet pins with the right nets:", mm_); ok &= mm_
# carrier copper/courtyards near the standoff holes
def clear(board, x, y):
    d = 1e9
    for t_ in board.GetTracks():
        if t_.GetClass() == "PCB_VIA":
            p = t_.GetPosition(); d = min(d, math.hypot(pcbnew.ToMM(p.x) - x, pcbnew.ToMM(p.y) - y) - pcbnew.ToMM(t_.GetWidth()) / 2)
        else:
            a, c = t_.GetStart(), t_.GetEnd(); ax, ay, cx, cy = map(pcbnew.ToMM, (a.x, a.y, c.x, c.y))
            dx, dy = cx - ax, cy - ay; L = dx * dx + dy * dy; tt = 0 if L == 0 else max(0, min(1, ((x - ax) * dx + (y - ay) * dy) / L))
            d = min(d, math.hypot(x - (ax + tt * dx), y - (ay + tt * dy)) - pcbnew.ToMM(t_.GetWidth()) / 2)
    for f in board.GetFootprints():
        if f.GetReference() in ("MH5", "MH6"): continue
        cy_ = f.GetCourtyard(pcbnew.F_Cu); bb = cy_.BBox() if cy_.OutlineCount() else f.GetBoundingBox(False, False)
        l, tp, rr, bt = map(pcbnew.ToMM, (bb.GetLeft(), bb.GetTop(), bb.GetRight(), bb.GetBottom()))
        dx = max(l - x, 0, x - rr); dy = max(tp - y, 0, y - bt); d = min(d, math.hypot(dx, dy))
    return round(d, 2)
for ref in ("H1", "H2"):
    f = next(f for f in b.GetFootprints() if f.GetReference() == ref); p = f.GetPosition()
    x, y = pcbnew.ToMM(p.x), pcbnew.ToMM(p.y)
    print(f"5c. {ref} at ({x:.2f}, {y:.2f}): nearest carrier track/via/courtyard (holes MH5/MH6 excluded) {clear(car, x, y)} mm (M2.5 standoff radius about 2.9)")
carh = {f.GetReference(): (round(pcbnew.ToMM(f.GetPosition().x), 2), round(pcbnew.ToMM(f.GetPosition().y), 2)) for f in car.GetFootprints() if f.GetReference() in ("MH5", "MH6")}
poeh = {f.GetReference(): (round(pcbnew.ToMM(f.GetPosition().x), 2), round(pcbnew.ToMM(f.GetPosition().y), 2)) for f in b.GetFootprints() if f.GetReference() in ("H1", "H2")}
hm = carh.get("MH5") == poeh["H1"] and carh.get("MH6") == poeh["H2"]
print("5d. carrier standoff holes MH5/MH6 match PoE H1/H2:", hm, carh, poeh); ok &= hm
print("ALL OK" if ok else "SOME CHECKS FAILED")
