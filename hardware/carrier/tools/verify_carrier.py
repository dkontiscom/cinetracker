#!/usr/bin/env python3
"""Checks of the carrier project: ERC, DRC with schematic parity (KiCad's own 'Update PCB from Schematic' check), an
independent netlist compare (../poe/tools/cmp_netlist.py) and the Teensy pin check (check_teensy_pins.py).
Needs KiCad 10 (kicad-cli and the pcbnew python module).  Run: python3 tools/verify_carrier.py
kicad-cli runs on a temporary copy of the project, so nothing is written into the repo."""
import os, re, shutil, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
P = os.path.dirname(HERE)
PCB = os.path.join(P, "cinetracker-carrier.kicad_pcb")
tmp = tempfile.mkdtemp()
W = os.path.join(tmp, "carrier"); shutil.copytree(P, W, ignore=shutil.ignore_patterns("fab", "documentation", "simulation", "*.kicad_prl"))
ok = True
def cli(*a): return subprocess.run(["kicad-cli", *a], capture_output=True, text=True)
def found(t, what): m = re.search(r"Found (\d+) " + what, t); return int(m.group(1)) if m else -1

rep = os.path.join(tmp, "erc.txt"); cli("sch", "erc", "--severity-all", "-o", rep, os.path.join(W, "cinetracker-carrier.kicad_sch"))
ne = int(re.search(r"ERC messages: (\d+)", open(rep).read()).group(1))
print(f"1. ERC: {ne} messages"); ok &= ne == 0

r = cli("pcb", "drc", "--schematic-parity", "--severity-all", "-o", os.path.join(tmp, "drc.txt"), os.path.join(W, "cinetracker-carrier.kicad_pcb"))
nv, nu, npar = found(r.stdout, "violations"), found(r.stdout, "unconnected"), found(r.stdout, "schematic parity")
print(f"2. DRC: {nv} violations, {nu} unconnected, {npar} schematic parity issues"); ok &= nv == 0 and nu == 0 and npar == 0

net = os.path.join(tmp, "s.net"); cli("sch", "export", "netlist", "-o", net, os.path.join(W, "cinetracker-carrier.kicad_sch"))
r = subprocess.run([sys.executable, os.path.join(P, "..", "poe", "tools", "cmp_netlist.py"), net, PCB], capture_output=True, text=True)
print("3. schematic vs board pads:", r.stdout.strip().splitlines()[-1]); ok &= r.returncode == 0

r = subprocess.run([sys.executable, os.path.join(HERE, "check_teensy_pins.py"), PCB], capture_output=True, text=True)
print("4. Teensy pins vs PJRC card and firmware:", r.stdout.strip().splitlines()[-1]); ok &= r.returncode == 0
print("ALL OK" if ok else "SOME CHECKS FAILED")
sys.exit(0 if ok else 1)
