#!/usr/bin/env python3
"""Rebuild every KiCad-made fab output of both CineTracker boards from the repo alone.

Needs: KiCad 10 (kicad-cli and its pcbnew python module) and rsvg-convert (librsvg) for the fit-check PDFs and PNGs.
Works on temporary copies, so KiCad never writes .kicad_prl or lock files into the repo.

  python3 hardware/tools/make_fab.py carrier=v0.62 poe=v0.62 --out /tmp/fab     # build into /tmp/fab/<board>/...
  python3 hardware/tools/make_fab.py carrier=v0.62 poe=v0.62 --check            # build to a temp dir, diff with the committed files
  python3 hardware/tools/make_fab.py carrier=v0.62 poe=v0.62 --write            # build and overwrite the committed files

Not rebuilt: the JLC stock / price columns of *-BOM-full.csv (a dated JLC snapshot); --check verifies only its KiCad-derived columns.
"""
import csv, os, re, shutil, subprocess, sys, tempfile, zipfile
import pcbnew

HW = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))          # .../hardware
ARGS = [a for a in sys.argv[1:] if "=" in a]; REVS = dict(a.split("=") for a in ARGS)
MODE = "--write" if "--write" in sys.argv else "--check" if "--check" in sys.argv else "--out"
OUT = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else tempfile.mkdtemp(prefix="fab-")
TMP = tempfile.mkdtemp(prefix="fabsrc-")

LAYERS = {"carrier": "F.Cu,In1.Cu,In2.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts",
          "poe": "F.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts"}
# JLC fits a part when it has an LCSC field, isn't DNP and isn't excluded from position files. Hand-fitted parts carry
# "exclude from position files" in the schematic and on the board (PoE J2 and J3); mounting holes have no LCSC field.
# JLC CPL: JLC places with its own (LCSC/EasyEDA) footprint origin and rotation (review A1, 2026-10-13).
# ref: rotation in JLC's frame. Position rule per ref below.
JLC_ROT = {"carrier": {"U1": 90, "U7": 180, "U4": 180, "U5": 180, "U6": 180, "U2": 270, "U3": 270,
                       "J2": 0, "J5": 270, "J6": 270, "J7": 0, "J8": 0, "J9": 0}}
# position = centre of the bounding box of the pad centres (= the LCSC footprint origin for these parts)
JLC_BBOX_CENTRE = {"carrier": {"J1", "J2", "J3", "J4", "J5", "J6", "J7", "J8", "J9"}}


def cli(*a):
    r = subprocess.run(["kicad-cli", *a], capture_output=True, text=True)
    if r.returncode: sys.exit(f"kicad-cli {' '.join(a)} failed:\n{r.stdout}\n{r.stderr}")


def rk(r):
    m = re.match(r"([A-Za-z_]+)(\d*)", r); return (m.group(1), int(m.group(2) or 0))


def sexpr(t):
    st = [[]]
    for k in re.findall(r'\(|\)|"(?:\\.|[^"\\])*"|[^\s()"]+', t):
        if k == "(": st.append([])
        elif k == ")": x = st.pop(); st[-1].append(x)
        else: st[-1].append(k[1:-1].replace('\\"', '"') if k.startswith('"') else k)
    return st[0][0]


def kids(lst, key): return [x for x in lst if isinstance(x, list) and x and x[0] == key]


def fields(netfile):
    """{ref: {field: value}} from a kicad-cli netlist (KiCad sexpr)."""
    root = sexpr(open(netfile).read()); out = {}
    for c in kids(kids(root, "components")[0], "comp"):
        ref = kids(c, "ref")[0][1]; d = {}
        for f in (kids(kids(c, "fields")[0], "field") if kids(c, "fields") else []):
            d[kids(f, "name")[0][1]] = f[2] if len(f) > 2 and not isinstance(f[2], list) else ""
        out[ref] = d
    return out


def copy_board(board):
    w = os.path.join(TMP, board)
    shutil.copytree(os.path.join(HW, board), w, ignore=shutil.ignore_patterns("fab", "documentation", "simulation", "*.kicad_prl"))
    return w


def gerbers(board, w, rev, dst):
    g = os.path.join(TMP, board + "-gerbers"); os.makedirs(g)
    pcb = os.path.join(w, f"cinetracker-{board}.kicad_pcb")
    # Protel extensions, X2 attributes and netlist attributes are kicad-cli defaults. --check-zones refills first
    # (v0.62: identical output, the stored fills are current).
    cli("pcb", "export", "gerbers", "--layers", LAYERS[board], "--subtract-soldermask", "--check-zones", "-o", g + "/", pcb)
    # separate PTH/NPTH files, G85 slots (kicad-cli default oval format "alternate", as JLC's KiCad guide asks),
    # absolute origin, mm, decimal; Gerber X2 drill maps
    cli("pcb", "export", "drill", "--format", "excellon", "--excellon-separate-th", "--generate-map", "--map-format", "gerberx2",
        "--drill-origin", "absolute", "--excellon-units", "mm", "-o", g + "/", pcb)
    with zipfile.ZipFile(os.path.join(dst, f"{board}-{rev}-gerbers.zip"), "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(os.listdir(g)): z.write(os.path.join(g, f), f)


def bom_cpl(board, w, rev, dst):
    net = os.path.join(TMP, board + ".net"); cli("sch", "export", "netlist", "-o", net, os.path.join(w, f"cinetracker-{board}.kicad_sch"))
    F = fields(net); b = pcbnew.LoadBoard(os.path.join(w, f"cinetracker-{board}.kicad_pcb"))
    fit = lambda f: F[f.GetReference()].get("LCSC") and not f.IsExcludedFromPosFiles() and not f.IsDNP()
    fps = sorted((f for f in b.GetFootprints() if fit(f)), key=lambda f: rk(f.GetReference()))
    groups = {}
    for f in fps:
        g = groups.setdefault(F[f.GetReference()]["LCSC"], dict(refs=[], fp=str(f.GetFPID().GetLibItemName()), d=F[f.GetReference()]))
        g["refs"].append(f.GetReference())
    rows = sorted(groups.items(), key=lambda kv: rk(sorted(kv[1]["refs"], key=rk)[0]))
    bom = [["Comment", "Designator", "Footprint", "LCSC Part #"]] + \
          [[g["d"]["Description"], ",".join(sorted(g["refs"], key=rk)), g["fp"], code] for code, g in rows]
    cpl = [["Designator", "Mid X", "Mid Y", "Layer", "Rotation"]]
    for f in fps:
        r = f.GetReference(); pads = [p for p in f.Pads() if p.GetNumber() != ""]
        if r in JLC_BBOX_CENTRE.get(board, ()):
            xs = [p.GetPosition().x for p in f.Pads()]; ys = [p.GetPosition().y for p in f.Pads()]
            x, y = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
        elif any(p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH for p in f.Pads()):      # other THT parts: pad centroid
            x = sum(p.GetPosition().x for p in pads) / len(pads); y = sum(p.GetPosition().y for p in pads) / len(pads)
        else:
            x, y = f.GetPosition().x, f.GetPosition().y
        rot = JLC_ROT.get(board, {}).get(r, f.GetOrientationDegrees())
        cpl.append([r, f"{pcbnew.ToMM(x):.4f}mm", f"{-pcbnew.ToMM(y):.4f}mm", "Top" if f.GetLayer() == pcbnew.F_Cu else "Bottom", f"{rot % 360:.6f}"])
    for kind, data in (("BOM-JLCPCB", bom), ("CPL-JLCPCB", cpl)):
        with open(os.path.join(dst, f"{board}-{rev}-{kind}.csv"), "w", newline="") as fh: csv.writer(fh).writerows(data)
    return F, groups, b


def bom_full_check(board, rev, F, groups, b):
    """KiCad-derived columns of BOM-full: Qty/board, Designator, Description, Manufacturer, MPN, LCSC, Process."""
    p = os.path.join(HW, board, "fab", f"{board}-{rev}-BOM-full.csv")
    if not os.path.exists(p): return f"{board} BOM-full: no committed file to check"
    full = list(csv.reader(open(p)))[1:]; bad = []
    tht = lambda ref: any(x.GetAttribute() == pcbnew.PAD_ATTRIB_PTH for f in b.GetFootprints() if f.GetReference() == ref for x in f.Pads())
    for code, g in groups.items():
        refs = ",".join(sorted(g["refs"], key=rk)); d = g["d"]
        hit = [x for x in full if x[1] == refs]
        want = [str(len(g["refs"])), refs, d["Description"], d["Manufacturer"], d["MPN"], code]
        if not hit or hit[0][:6] != want or hit[0][7] != ("THT" if any(tht(r) for r in g["refs"]) else "SMT"): bad.append(refs)
    return f"{board} BOM-full KiCad columns: " + ("match" if not bad else f"MISMATCH {bad}")


def svg_body(pcb, layers, drill, colour=None):
    t = os.path.join(TMP, "body.svg")
    cli("pcb", "export", "svg", "--layers", layers, "--page-size-mode", "0", "--exclude-drawing-sheet", "--black-and-white",
        "--mode-single", "--drill-shape-opt", drill, "-o", t, pcb)
    s = open(t).read(); s = s[s.index("<g"):s.rindex("</svg>")]
    return s.replace("#000000", colour).replace("#000", colour) if colour else s


A4 = '<?xml version="1.0"?>\n<svg xmlns="http://www.w3.org/2000/svg" width="297mm" height="210mm" viewBox="0 0 297 210">\n<rect width="297" height="210" fill="white"/>\n'
TXT = '<text x="15" y="{y}" font-family="Helvetica, Arial, sans-serif" font-size="{s}"{b}>{t}</text>\n'
BAR = ('<line x1="15" y1="195" x2="65" y2="195" stroke="black" stroke-width="0.4"/>\n'
       '<line x1="15" y1="193" x2="15" y2="197" stroke="black" stroke-width="0.4"/><line x1="65" y1="193" x2="65" y2="197" stroke="black" stroke-width="0.4"/>\n'
       '<text x="35" y="192" font-family="Helvetica, Arial, sans-serif" font-size="2.6">50 mm</text>\n</svg>')
DX, DY = (297 - 80) / 2, 45


def head(t1, t2, t3):
    return A4 + TXT.format(y=15, s=4.2, b=' font-weight="bold"', t=t1) + TXT.format(y=22, s=2.6, b="", t=t2) + TXT.format(y=26.5, s=2.6, b="", t=t3)


def rsvg(svg, *args):
    p = os.path.join(TMP, "page.svg"); open(p, "w").write(svg); subprocess.run(["rsvg-convert", *args, p], check=True)


def fit_carrier(w, rev, dst):
    pcb = os.path.join(w, "cinetracker-carrier.kicad_pcb")
    svg = head(f"CineTracker carrier {rev} - 1:1 fit check (print at 100 %, no scaling)",
               "Board outline, silkscreen, fab and courtyard layers. The 50 mm bar must measure 50.0 mm on paper.",
               "Place the physical M8 sockets, BNC, Teensy 4.1 (USB to the left) and the 1x4 header on the outlines. Connector courtyards = body outlines from the vendor drawings.") \
        + f'<g transform="translate({DX} {DY})">{svg_body(pcb, "Edge.Cuts,F.SilkS,F.Fab,F.CrtYd", "0")}</g>\n' + BAR
    rsvg(svg, "-f", "pdf", "-o", os.path.join(dst, f"carrier-{rev}-1to1-fit-check.pdf"))


def fit_stack(wc, wp, crev, prev, dst, docdst):
    C = os.path.join(wc, "cinetracker-carrier.kicad_pcb"); P = os.path.join(wp, "cinetracker-poe.kicad_pcb")
    svg = head(f"CineTracker stack {crev} / PoE {prev} - 1:1 fit check (print at 100 %, no scaling)",
               f"Top view, carrier coordinates. Grey: carrier {crev} (outline, silkscreen, courtyards). Red: PoE board {prev} (outline, copper, silkscreen, courtyards).",
               "Panel edge at y = 64 for both boards. Connectors are drawn by their on-board courtyards. The 50 mm bar must measure 50.0 mm on paper.") \
        + f'<g transform="translate({DX} {DY})" opacity="0.9">{svg_body(C, "Edge.Cuts,F.SilkS,F.CrtYd", "0", "#9a9a9a")}</g>\n' \
        + f'<g transform="translate({DX} {DY})" opacity="0.85">{svg_body(P, "Edge.Cuts,F.Cu,F.SilkS,F.CrtYd", "1", "#d01010")}</g>\n' + BAR
    rsvg(svg, "-f", "pdf", "-o", os.path.join(dst, f"poe-{prev}-stack-1to1-fit-check.pdf"))
    vx, vy, vw, vh = DX - 2.9, DY - 2.9, 85.9, 73.9
    v = svg.replace('width="297mm" height="210mm" viewBox="0 0 297 210"', f'width="{vw}mm" height="{vh}mm" viewBox="{vx} {vy} {vw} {vh}"') \
           .replace('<rect width="297" height="210" fill="white"/>', f'<rect x="{vx}" y="{vy}" width="{vw}" height="{vh}" fill="white"/>')
    rsvg(v, "-w", "1200", "-b", "white", "-o", os.path.join(docdst, "stack-top-view.png"))


def poe_views(wp, rev, docdst):
    pcb = os.path.join(wp, "cinetracker-poe.kicad_pcb")
    for side, layers, extra in (("top", "F.Cu,F.SilkS,Edge.Cuts", []), ("bottom", "B.Cu,B.SilkS,Edge.Cuts", ["--mirror"])):
        s = os.path.join(TMP, side + ".svg")
        cli("pcb", "export", "svg", "--layers", layers, "--page-size-mode", "2", "--exclude-drawing-sheet", "--mode-single", *extra, "-o", s, pcb)
        subprocess.run(["rsvg-convert", "-w", "1200", "-b", "white", "-o", os.path.join(docdst, f"poe-{rev}-{side}.png"), s], check=True)


def main():
    if not REVS: sys.exit(__doc__)
    built = {}
    for board, rev in REVS.items():
        w = copy_board(board); fab = os.path.join(OUT, board, "fab"); doc = os.path.join(OUT, board, "documentation")
        os.makedirs(fab, exist_ok=True); os.makedirs(doc, exist_ok=True)
        gerbers(board, w, rev, fab)
        F, groups, b = bom_cpl(board, w, rev, fab)
        print(bom_full_check(board, rev, F, groups, b))
        cli("sch", "export", "pdf", "-o", os.path.join(doc, f"cinetracker-{board}-{rev}-schematic.pdf"), os.path.join(w, f"cinetracker-{board}.kicad_sch"))
        if board == "carrier": fit_carrier(w, rev, fab)
        if board == "poe": poe_views(w, rev, doc)
        built[board] = w
    if "poe" in REVS and "carrier" in REVS:
        fit_stack(built["carrier"], built["poe"], REVS["carrier"], REVS["poe"], os.path.join(OUT, "poe", "fab"), os.path.join(OUT, "poe", "documentation"))
    if MODE == "--out": print("written to", OUT); return
    for root, _, files in os.walk(OUT):
        for f in sorted(files):
            new = os.path.join(root, f); old = os.path.join(HW, os.path.relpath(new, OUT))
            if MODE == "--write": shutil.copyfile(new, old); continue
            print(f"{os.path.relpath(old, HW)}: {compare(old, new)}")
    if MODE == "--write": print("committed files overwritten from", OUT)


def compare(old, new):
    if not os.path.exists(old): return "NEW (no committed file)"
    if old.endswith(".zip"):
        a, b = zipfile.ZipFile(old), zipfile.ZipFile(new)
        if sorted(a.namelist()) != sorted(b.namelist()): return f"DIFFERENT file list {sorted(set(a.namelist()) ^ set(b.namelist()))}"
        stamp = re.compile(rb"CreationDate|^G04 Created by|DRILL file .* date", re.M)
        bad = [n for n in a.namelist() if [l for l in a.read(n).splitlines() if not stamp.search(l)] != [l for l in b.read(n).splitlines() if not stamp.search(l)]]
        return "same (timestamps ignored)" if not bad else f"DIFFERS in {bad}"
    if old.endswith(".csv"): return "same" if open(old).read() == open(new).read() else "DIFFERS"
    if old.endswith(".pdf"):
        t = lambda p: subprocess.run(["pdftotext", "-layout", p, "-"], capture_output=True, text=True).stdout
        return "same text" if t(old) == t(new) else "DIFFERS (text)"
    return "same" if open(old, "rb").read() == open(new, "rb").read() else "DIFFERS (binary; PNG/PDF renderer versions can change bytes)"


if __name__ == "__main__":
    main()
