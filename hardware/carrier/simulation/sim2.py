# SPDX-FileCopyrightText: 2026 Dimitris Kontis and CineTracker contributors
# SPDX-License-Identifier: CERN-OHL-S-2.0
"""genlock output stage (carrier v0.5c onward): ladder (+offset) -> OPA356 buffer -> Sallen-Key (Q=0.5) -> OPA356 line driver -> 47u -> 75R -> 75 ohm cable -> 75R"""
import subprocess, numpy as np, os
import sim
DIR=os.path.dirname(os.path.abspath(__file__))  # TI model files (OPA356.lib, OPA365.lib) go here too
def net(fps=25, vdd=3.3, rg=23.0, tr_gpio=2e-9, skew=5e-9, Rf=100.0, Cf=160e-12, Cs=0.0, Cstray_s=2e-12,
        Cstray_f=1e-12, R5a=3300, R5=15000, R=None, cable_td=10e-9, cload=5e-12, tstop_extra=600e-9,
        model="OPA356W", riso=0.0, sk=None, model1=None):
    T = 1/(148.5e6 if fps in (50,60) else 74.25e6)
    line = {24:2750,25:2640,30:2200,50:2640,60:2200}[fps]*T
    r = dict(R1=2000,R2=2000,R3=1011,R4=2022,R6=75,R7=10000,C1=47e-6); r.update(R or {})
    t0=1e-6
    def pwl(points): return "PWL("+" ".join(f"{t:.12e} {v:.6f}" for t,v in points)+")"
    def edge(p,t,a,b): p.append((t,a)); p.append((t+tr_gpio,b))
    b0=[(0,vdd)]; b1=[(0,0)]
    edge(b0,t0,vdd,0); edge(b1,t0+44*T,0,vdd); edge(b1,t0+88*T,vdd,0); edge(b0,t0+88*T+skew,0,vdd)
    tstop=t0+88*T+tstop_extra
    iso = f"RISO o2 out {riso}" if riso else ""
    fb = "o2" if riso else "out"
    if sk:
        filt = f"RF1 o1 m {sk[0]}\nRF2 m f {sk[1]}\nCF1 m {fb} {sk[2]+Cstray_f}\nCF2 f 0 {sk[3]+Cstray_f}"
    else:
        filt = f"RF o1 f {Rf}\nCF f 0 {Cf+Cstray_f}"
    o2 = "o2" if riso else "out"
    txt=f"""* v0.5c genlock stage
.include {DIR}/ideal.lib
.include {DIR}/OPA356.lib
VDD vdd 0 {vdd}
VA v33a 0 {vdd}
VB0 g0 0 {pwl(b0)}
VB1 g1 0 {pwl(b1)}
RG0 g0 p0 {rg}
RG1 g1 p1 {rg}
R1 p1 s {r['R1']}
R2 p0 b {r['R2']}
R3 s b {r['R3']}
R4 b 0 {r['R4']}
R5 s 0 {R5}
R5a s v33a {R5a}
CSS s 0 {Cs+Cstray_s}
XU1 s o1 v33a 0 o1 {model1 or model}
{filt}
XU2 f {o2} v33a 0 {o2} {model}
{iso}
C1 out dc {r['C1']}
R7 dc 0 {r['R7']}
R6 dc bnc {r['R6']}
CD5 bnc 0 10p
T1 bnc 0 rx 0 Z0=75 TD={cable_td}
RL rx 0 75
CL rx 0 {cload}
.options method=gear
.tran 0.1n {tstop:.9e} 0 0.1n
.control
run
wrdata {DIR}/out2.txt v(rx) v(out) v(f)
.endc
.end
"""
    return txt,t0,T,line
def run(**kw):
    txt,t0,T,line=net(**kw); p=os.path.join(DIR,"run2.cir"); open(p,"w").write(txt)
    of=os.path.join(DIR,"out2.txt")
    if os.path.exists(of): os.remove(of)
    r=subprocess.run(["ngspice","-b",p],capture_output=True,text=True)
    if not os.path.exists(of) or os.path.getsize(of)<1000:
        open(os.path.join(DIR,"err2.log"),"w").write(r.stdout+r.stderr)
        raise RuntimeError("ngspice failed, see err2.log")
    d=np.loadtxt(os.path.join(DIR,"out2.txt")); return d[:,0],d[:,1],d[:,3],d[:,5],t0,T,line
def measure(**kw):
    t,vrx,vout,vf,t0,T,line=run(**kw); d=kw.get("cable_td",10e-9)
    blank=sim.avg(t,vrx,t0-200e-9+d,t0-20e-9+d)
    neg=sim.avg(t,vrx,t0+d+28*T,t0+d+42*T)-blank
    pos=sim.avg(t,vrx,t0+d+72*T,t0+d+86*T)-blank
    trl=sim.rise(t,vrx,t0+d-5e-9,t0+d+40*T,blank,blank+neg)
    trm=sim.rise(t,vrx,t0+d+44*T-5e-9,t0+d+84*T,blank+neg,blank+pos)
    tre=sim.rise(t,vrx,t0+d+88*T-5e-9,t0+d+88*T+300e-9,blank+pos,blank)
    gpio=t0+44*T+d; m=(t>gpio-5e-9)&(t<gpio+300e-9); tt=t[m]; vv=vrx[m]-blank
    i=np.where((vv[:-1]<0)&(vv[1:]>=0))[0][0]; zc=tt[i]+(-vv[i])*(tt[i+1]-tt[i])/(vv[i+1]-vv[i])
    m2=(t>t0+d+44*T)&(t<t0+d+88*T); over=(vrx[m2]-blank).max()-pos
    m3=(t>t0+d+88*T)&(t<t0+d+88*T+400e-9); under=(vrx[m3]-blank).min()
    return dict(neg=neg*1e3,pos=pos*1e3,asym=(pos+neg)*1e3,tr_lead=trl*1e9,tr_mid=trm*1e9,tr_end=tre*1e9,
                zc_ns=(zc-gpio)*1e9,overshoot=over*1e3,trail_dip=under*1e3)
