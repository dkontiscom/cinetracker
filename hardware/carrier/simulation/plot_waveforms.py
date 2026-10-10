"""Plot v0.5c genlock sims from cached waveforms (wave_*.npz written by run_waveforms.py)."""
import numpy as np, matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
import os, sys; HERE=os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0,HERE); import sim
d=10e-9
def load(j):
    z=np.load(os.path.join(HERE,f"wave_{j}.npz")); return z["t"],z["v"],float(z["t0"]),float(z["T"])
def meas(t,v,t0,T):
    b=sim.avg(t,v,t0-200e-9+d,t0-20e-9+d)
    neg=sim.avg(t,v,t0+d+28*T,t0+d+42*T)-b; pos=sim.avg(t,v,t0+d+72*T,t0+d+86*T)-b
    lead=sim.rise(t,v,t0+d-5e-9,t0+d+40*T,b,b+neg); mid=sim.rise(t,v,t0+d+44*T-5e-9,t0+d+84*T,b+neg,b+pos)
    trail=sim.rise(t,v,t0+d+88*T-5e-9,t0+d+88*T+300e-9,b+pos,b)
    return b,neg*1e3,pos*1e3,lead*1e9,mid*1e9,trail*1e9
fig,ax=plt.subplots(1,2,figsize=(15,5.8))
res={}
for j,lab,c,lw in (("b60","v0.5b  OPA365 buffer + CS1 22p","#999",1.2),("60_2","v0.5c  OPA356 + Sallen-Key Q=0.5 + OPA356","#c00",1.6)):
    t,v,t0,T=load(j); b,neg,pos,le,mi,tr=meas(t,v,t0,T); res[j]=(neg,pos,le,mi,tr)
    m=(t>t0-100e-9)&(t<t0+900e-9)
    ax[0].plot((t[m]-t0-d)*1e9,(v[m]-b)*1e3,color=c,lw=lw,label=f"{lab}\nrise {le:.1f} / {mi:.1f} / {tr:.1f} ns, levels {neg:.1f} / +{pos:.1f} mV")
T60=T
ax[0].axhline(300,color="#6a6",ls="--",lw=0.7); ax[0].axhline(-300,color="#6a6",ls="--",lw=0.7)
for k in (0,44,88): ax[0].axvline(k*T60*1e9,color="#ccc",ls=":",lw=0.8)
ax[0].set_xlim(-30,800); ax[0].set_ylim(-340,340)
ax[0].set_title("1080p60 (T = 6.734 ns), firmware bit skew 2 ns\nST 274 rise 10-90 %: 50/60p 16.8-37.0 ns, 24/25/30p 33.7-74.1 ns")
ax[0].set_xlabel("ns after the bit0 GPIO edge (at the camera: 75 Ω cable into 75 Ω)"); ax[0].set_ylabel("mV relative to blanking"); ax[0].legend(fontsize=8,loc="lower right",bbox_to_anchor=(1.0,0.06)); ax[0].grid(alpha=.3)
for sk,c in (("0.5","#000"),("2","#c00"),("5","#e80"),("10","#36c")):
    t,v,t0,T=load(f"25_{sk}"); b,neg,pos,le,mi,tr=meas(t,v,t0,T); res["25_"+sk]=(neg,pos,le,mi,tr)
    te=t0+88*T+d; m=(t>te-20e-9)&(t<te+160e-9)
    ok="✓" if tr>=33.67 else "✗"
    ax[1].plot((t[m]-te)*1e9,(v[m]-b)*1e3,color=c,label=f"skew {sk} ns: trail {tr:.1f} ns  (24/25/30p {ok}, 50/60p ✓)")
ax[1].axhline(0,color="#6a6",ls="--",lw=0.7); ax[1].axhline(300,color="#6a6",ls="--",lw=0.7)
ax[1].set_xlim(-20,160)
ax[1].set_title("1080p25 trailing edge (+300 → 0 mV) vs firmware write skew\n(bit1 LOW then bit0 HIGH: brief 00 = −300 mV state)")
ax[1].set_xlabel("ns after the bit1 GPIO edge"); ax[1].set_ylabel("mV relative to blanking"); ax[1].legend(fontsize=8,loc="upper right"); ax[1].grid(alpha=.3)
fig.suptitle("CineTracker carrier v0.5c genlock — ngspice 42, TI OPA356 (SBOM848) / OPA365 (SBOC133) macromodels, waveform from firmware t41-tri-sync.h",fontsize=10)
fig.tight_layout(); fig.savefig(os.path.join(HERE,"genlock-sim.png"),dpi=130)
for k,v in res.items(): print(k,["%.2f"%x for x in v])
