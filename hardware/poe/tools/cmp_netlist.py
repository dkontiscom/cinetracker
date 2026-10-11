"""Compare a kicad-cli schematic netlist with the board's pad nets: same partition of pads, net names may differ.
KiCad's own 'pcb drc --schematic-parity' checks the names too; this one is an independent second check.
No-connect pins (KiCad names their net unconnected-...) must have no net or their unconnected- net on the board.
Usage: cmp_netlist.py sch.net board.kicad_pcb [map.json]   (KiCad 10: kicad-cli and the pcbnew python module)"""
import json, re, sys
import pcbnew

def parse(t):
    st = [[]]
    for k in re.findall(r'\(|\)|"(?:\\.|[^"\\])*"|[^\s()"]+', t):
        if k == "(": st.append([])
        elif k == ")": x = st.pop(); st[-1].append(x)
        else: st[-1].append(k[1:-1].replace('\\"', '"') if k.startswith('"') else k)
    return st[0][0]
def get(lst, key): return next((x for x in lst if isinstance(x, list) and x and x[0] == key), None)
def every(lst, key): return [x for x in lst if isinstance(x, list) and x and x[0] == key]

netf, brd = sys.argv[1], sys.argv[2]
sch = {}
for n in every(get(parse(open(netf).read()), "nets"), "net"):
    name = get(n, "name")[1]
    for nd in every(n, "node"): sch[(get(nd, "ref")[1], get(nd, "pin")[1])] = name
b = pcbnew.LoadBoard(brd); pcb = {}
for f in b.GetFootprints():
    for p in f.Pads():
        if p.GetNumber() == "": continue
        pcb.setdefault((f.GetReference(), p.GetNumber()), set()).add(p.GetNetname())
pcb = {k: v.pop() if len(v) == 1 else "|".join(sorted(v)) for k, v in pcb.items()}
keys = set(sch) | set(pcb); bad = 0; m1 = {}; m2 = {}
for k in sorted(keys):
    a = pcb.get(k, "<missing>"); s = sch.get(k, "<missing>")
    if a == "" or a.startswith("unconnected-"): a = "<nc>"
    if s.startswith("unconnected-"): s = "<nc>"
    if a == "<nc>" or s == "<nc>":
        if a != s: bad += 1; print("NC mismatch", k, a, s)
        continue
    if m1.setdefault(a, s) != s or m2.setdefault(s, a) != a:
        bad += 1; print("MISMATCH", k, "pcb", a, "sch", s, "(pcb->", m1.get(a), ", sch->", m2.get(s), ")")
print(f"pads compared {len(keys)} | nets pcb {len(m1)} sch {len(m2)} | mismatches {bad}")
if len(sys.argv) > 3: json.dump(m1, open(sys.argv[3], "w"), indent=1)
sys.exit(1 if bad else 0)
