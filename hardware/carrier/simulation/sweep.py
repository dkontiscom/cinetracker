# SPDX-FileCopyrightText: 2026 Dimitris Kontis and CineTracker contributors
# SPDX-License-Identifier: CERN-OHL-S-2.0
import json, sys
from sim2 import measure
base=dict(R5a=5600,R5=5100,model="OPA356W",sk=(150,150,68e-12,68e-12))
runs=[("25p skew0.5",dict(fps=25,skew=0.5e-9)),("25p skew2",dict(fps=25,skew=2e-9)),("25p skew5",dict(fps=25,skew=5e-9)),
      ("25p skew10",dict(fps=25,skew=10e-9)),("60p skew2",dict(fps=60,skew=2e-9)),
      ("25p skew2 RC+2%",dict(fps=25,skew=2e-9,sk=(153,153,69.36e-12,69.36e-12))),
      ("25p skew2 RC-2%",dict(fps=25,skew=2e-9,sk=(147,147,66.64e-12,66.64e-12))),
      ("25p skew2 vdd 3.267",dict(fps=25,skew=2e-9,vdd=3.267))]
out={}
for tag,kw in runs:
    k=dict(base); k.update(kw)
    try:
        m=measure(**k); out[tag]=m
        print(f"{tag:22s} lvl -{abs(m['neg']):.2f}/+{m['pos']:.2f} asym {m['asym']:.2f} | tr {m['tr_lead']:.2f} {m['tr_mid']:.2f} {m['tr_end']:.2f} | over {m['overshoot']:.2f} dip {m['trail_dip']:.2f} zc {m['zc_ns']:.1f}",flush=True)
    except Exception as e:
        print(tag,"ERR",e,flush=True)
json.dump(out,open("sweep-results.json","w"),indent=1)
