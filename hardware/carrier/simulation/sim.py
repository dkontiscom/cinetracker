# SPDX-FileCopyrightText: 2026 Dimitris Kontis and CineTracker contributors
# SPDX-License-Identifier: CERN-OHL-S-2.0
"""v0.5b reference: single OPA365 buffer stage, used only for the before/after comparison"""
import subprocess, numpy as np, os, sys, json, tempfile
DIR=os.path.dirname(os.path.abspath(__file__))  # TI model files (OPA356.lib, OPA365.lib) go here too
def net(fps=25, vdd=3.3, rg=23.0, tr_gpio=2e-9, skew=5e-9, R=None, Cs=0.0, offset=None,
        cable_td=10e-9, cload=5e-12, tstop_extra=600e-9, broad_lines=0):
    T = 1/(148.5e6 if fps in (50,60) else 74.25e6)
    line = {24:2750,25:2640,30:2200,50:2640,60:2200}[fps]*T
    R = R or {}
    r = dict(R1=2000,R2=2000,R3=1011,R4=2022,R5=2700,R5a=None,R6=75,R7=10000,C1=47e-6)
    r.update(R)
    t0=1e-6
    # bit0: high (blank), low at t0, high at t0+88T+skew ; bit1: high t0+44T..t0+88T
    def pwl(points):
        s=" ".join(f"{t:.12e} {v:.6f}" for t,v in points); return f"PWL({s})"
    def edge(pts, t, v0, v1):
        pts.append((t, v0)); pts.append((t+tr_gpio, v1))
    b0=[(0,vdd)]; b1=[(0,0)]
    tl=t0
    nlines = max(1, broad_lines+1)
    for n in range(nlines):
        ts = t0 + n*line
        edge(b0, ts, vdd, 0)
        edge(b1, ts+44*T, 0, vdd)
        edge(b1, ts+88*T, vdd, 0)
        edge(b0, ts+88*T+skew, 0, vdd)
        if n < broad_lines:
            # broad pulse: firmware drops bit0 again 88T after pulse end, raises it 44T before next pulse
            edge(b0, ts+176*T, vdd, 0)
            edge(b0, ts+line-44*T, 0, vdd)
    tstop = t0 + (nlines-1)*line + 88*T + tstop_extra
    r5 = "R5 s 0 {}".format(r["R5"]) if offset is None else "R5 s 0 {}\nR5a s vdd {}".format(offset[1], offset[0])
    cs = f"CS s 0 {Cs}" if Cs else ""
    txt=f"""* genlock stage
.include {DIR}/OPA365.lib
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
{r5}
{cs}
XU1 s out v33a 0 out OPA365
C1 out dc {r['C1']}
R7 dc 0 {r['R7']}
R6 dc bnc {r['R6']}
CD5 bnc 0 10p
T1 bnc 0 rx 0 Z0=75 TD={cable_td}
RL rx 0 75
CL rx 0 {cload}
.tran 0.2n {tstop:.9e} 0 0.2n
.control
run
wrdata {DIR}/out.txt v(rx) v(out) v(s)
.endc
.end
"""
    return txt, t0, T, line
def run(**kw):
    txt,t0,T,line=net(**kw)
    p=os.path.join(DIR,"run.cir"); open(p,"w").write(txt)
    r=subprocess.run(["ngspice","-b",p],capture_output=True,text=True)
    d=np.loadtxt(os.path.join(DIR,"out.txt"))
    t=d[:,0]; vrx=d[:,1]; vout=d[:,3]; vs=d[:,5]
    return t,vrx,vout,vs,t0,T,line
def avg(t,v,a,b):
    m=(t>=a)&(t<=b); return v[m].mean()
def rise(t,v,ta,tb,v0,v1):
    m=(t>=ta)&(t<=tb); tt=t[m]; vv=v[m]
    lo=v0+0.1*(v1-v0); hi=v0+0.9*(v1-v0)
    def cross(level):
        s=np.sign(vv-level); idx=np.where(np.diff(s)!=0)[0]
        if len(idx)==0: return np.nan
        i=idx[0]; return tt[i]+(level-vv[i])*(tt[i+1]-tt[i])/(vv[i+1]-vv[i])
    return abs(cross(hi)-cross(lo))
def measure(**kw):
    t,vrx,vout,vs,t0,T,line=run(**kw)
    d=kw.get("cable_td",10e-9)
    blank=avg(t,vrx,t0-200e-9+d,t0-20e-9+d)
    neg=avg(t,vrx,t0+d+20*T,t0+d+40*T)-blank
    pos=avg(t,vrx,t0+d+64*T,t0+d+84*T)-blank
    end=avg(t,vrx,t0+d+88*T+300e-9,t0+d+88*T+500e-9)-blank
    tr_lead=rise(t,vrx,t0+d-5e-9,t0+d+30*T,blank,blank+neg)
    tr_mid=rise(t,vrx,t0+d+40*T,t0+d+70*T,blank+neg,blank+pos)
    tr_end=rise(t,vrx,t0+d+84*T,t0+d+120*T,blank+pos,blank)
    # glitch: minimum after end transition
    m=(t>=t0+d+86*T)&(t<=t0+d+100*T)
    glitch=vrx[m].min()-blank
    zc=None
    return dict(neg_mV=neg*1e3,pos_mV=pos*1e3,asym_mV=(pos+neg)*1e3,end_mV=end*1e3,
                tr_lead_ns=tr_lead*1e9,tr_mid_ns=tr_mid*1e9,tr_end_ns=tr_end*1e9,
                glitch_mV=glitch*1e3, vout_min=vout[(t>t0)&(t<t0+44*T)].min())
if __name__=="__main__":
    for fps in (25,60):
        print(fps, json.dumps({k:round(v,2) for k,v in measure(fps=fps).items()}))
