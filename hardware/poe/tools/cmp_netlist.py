"""Compare a kicad-cli schematic netlist with the board's pad nets (same partition of pads). Usage: cmp_netlist.py sch.net board.kicad_pcb"""
import re, sys, pcbnew, json
netf, brd = sys.argv[1], sys.argv[2]
t=open(netf).read()
sch={}
for m in re.finditer(r'\(net \(code "\d+"\) \(name "([^"]*)"\)(.*?)\)\)\s*(?=\(net|\)\)\s*$|\)$)', t, re.S):
    name=m.group(1)
    for r,p in re.findall(r'\(node \(ref "([^"]+)"\) \(pin "([^"]+)"\)', m.group(2)):
        sch[(r,p)]=name
b=pcbnew.LoadBoard(brd); pcb={}
for f in b.GetFootprints():
    for p in f.Pads():
        if p.GetNumber()=="": continue
        pcb.setdefault((f.GetReference(),p.GetNumber()),set()).add(p.GetNetname())
pcb={k:v.pop() if len(v)==1 else "|".join(sorted(v)) for k,v in pcb.items()}
REN={}
pcb={(REN.get(r,r),n):v for (r,n),v in pcb.items()}
keys=set(sch)|set(pcb); bad=0; m1={}; m2={}
for k in sorted(keys):
    a=pcb.get(k,"<missing>"); s=sch.get(k,"<missing>")
    if a=="" : a="<nc>"
    if s.startswith("unconnected-"): s="<nc>"
    if a=="<nc>" or s=="<nc>":
        if a!=s: bad+=1; print("NC mismatch",k,a,s)
        continue
    if m1.setdefault(a,s)!=s or m2.setdefault(s,a)!=a:
        bad+=1; print("MISMATCH",k,"pcb",a,"sch",s,"(pcb->",m1.get(a),", sch->",m2.get(s),")")
print(f"pads compared {len(keys)} | nets pcb {len(m1)} sch {len(m2)} | mismatches {bad}")
json.dump(m1,open(sys.argv[3],"w"),indent=1) if len(sys.argv)>3 else None
sys.exit(1 if bad else 0)
