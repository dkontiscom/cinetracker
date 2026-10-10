import os, sys, numpy as np
sys.path.insert(0, sys.argv[1]); import sim, sim2
jobs=sys.argv[2].split(",")
out={}
for j in jobs:
    if j=="b60":
        t,v,_,_,t0,T,_=sim.run(fps=60,skew=2e-9,offset=(3300,15000),Cs=22e-12)
    else:
        fps,sk=j.split("_"); t,v,_,_,t0,T,_=sim2.run(fps=int(fps),skew=float(sk)*1e-9,R5a=5600,R5=5100,model="OPA356W",sk=(150,150,68e-12,68e-12))
    np.savez(os.path.join(os.path.dirname(os.path.abspath(__file__)),f"wave_{j}.npz"),t=t,v=v,t0=t0,T=T); print(j,"done",flush=True)
