import numpy as np
rng=np.random.default_rng(1); N=400000
def run(tol_r, tol_v=0.01, dist="normal"):
    def r(nom,t):
        return nom*(1+ (rng.normal(0,t/3,N) if dist=="normal" else rng.uniform(-t,t,N)))
    V=r(3.3,tol_v); rg=23.0
    R1=r(2000,tol_r)+rg; R2=r(2000,tol_r)+rg; R3=r(1000,tol_r)+r(11,0.01); R4=r(2000,tol_r)+r(22,0.01)
    R5=r(5100,tol_r); R5a=r(5600,tol_r)
    def solve(b1,b0):
        a11=1/R1+1/R3+1/R5+1/R5a; a12=-1/R3; a22=1/R3+1/R2+1/R4
        y1=b1*V/R1+V/R5a; y2=b0*V/R2
        det=a11*a22-a12*a12
        return (y1*a22-a12*y2)/det
    s00,s01,s10=solve(0,0),solve(0,1),solve(1,0)
    R6=r(75,tol_r); g=75/(75+R6)
    lo=(s00-s01)*g*1000; hi=(s10-s01)*g*1000
    ok=(abs(lo+300)<=6)&(abs(hi-300)<=6)&(abs(hi+lo)<6)
    return ok.mean()*100, lo.min(),lo.max(),hi.min(),hi.max()
for tr in (0.01,0.001):
    for d in ("normal","uniform"):
        p,a,b,c,e=run(tr,dist=d); print(f"R tol {tr*100:.1f}% {d:8s}: pass {p:5.1f}%  lo {a:.1f}..{b:.1f}  hi {c:.1f}..{e:.1f}")
