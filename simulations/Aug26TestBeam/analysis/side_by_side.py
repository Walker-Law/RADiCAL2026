#!/usr/bin/env python3
"""side_by_side.py - put the simulation next to the beam test, parsed from both
sources so no number is hand-transcribed.

  simulation  build/plots/emulate/emulate_summary.txt   (analysis/tb26_emulate.C)
  experiment  ~/Research/radical-t10-2026/Output/...    (the collaboration's own macros)

Run from Aug26TestBeam/:   python3 analysis/side_by_side.py
"""
import re, os, sys

EXP = os.path.expanduser("~/Research/radical-t10-2026/Output")
SIM = "build/plots/emulate/emulate_summary.txt"

def parse_scan(path):
    """EnergyScan*_summary.txt -> {E: (peak, peakErr, sigOverE, sigErr, tMedian, tMedErr)}"""
    out = {}
    for line in open(path):
        m = re.match(r"\s*(\d+) GeV \(run [^)]*\):.*peak\s+(-?[\d.]+)\s+\+/-\s+(-?[\d.]+),"
                     r"\s*sigma\s+(-?[\d.]+)\s*=>\s*sigma/E\s+(-?[\d.]+)\s*\+/-\s*(-?[\d.]+)\s*%"
                     r".*t-MEDIAN\s+(-?[\d.]+)\s*\+/-\s*(-?[\d.]+)", line)
        if m:
            E = float(m.group(1))
            out[E] = dict(peak=float(m.group(2)), peakE=float(m.group(3)),
                          res=float(m.group(5)), resE=float(m.group(6)),
                          tmed=float(m.group(7)), tmedE=float(m.group(8)))
    return out

def parse_sim(path):
    """emulate_summary.txt -> per material {E: dict} for both formats"""
    mats, cur, mode = {}, None, None
    for line in open(path):
        m = re.match(r"=== (\w+): EnergyScan format", line)
        if m: cur, mode = m.group(1), "scan"; mats.setdefault(cur, {}); continue
        m = re.match(r"=== (\w+): DiagDiff format", line)
        if m: cur, mode = m.group(1), "diag"; mats.setdefault(cur, {}); continue
        if cur is None: continue
        if mode == "scan":
            m = re.match(r"\s*(\d+) GeV \(sim\):.*peak\s+(-?[\d.]+)\s+\+/-\s+(-?[\d.]+),"
                         r"\s*sigma\s+(-?[\d.]+)\s*=>\s*sigma/E\s+(-?[\d.]+)\s*\+/-\s*(-?[\d.]+)\s*%"
                         r".*t-MEDIAN\s+(-?[\d.]+)\s*\+/-\s*(-?[\d.]+)", line)
            if m:
                E = float(m.group(1))
                mats[cur].setdefault(E, {}).update(
                    peak=float(m.group(2)), peakE=float(m.group(3)),
                    res=float(m.group(5)), resE=float(m.group(6)),
                    tmed=float(m.group(7)), tmedE=float(m.group(8)))
        else:
            m = re.match(r"\s*([\d.]+)\s+(\d+)\s+(-?[\d.]+)\s+(-?[\d.]+)\s+"
                         r"(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)\s*\|\s*"
                         r"(-?[\d.]+)\s+(-?[\d.]+)\s*\|\s*(-?[\d.]+)\s+(-?[\d.]+)", line)
            if m:
                E = float(m.group(1))
                mats[cur].setdefault(E, {}).update(
                    n4=int(m.group(2)), intr=float(m.group(5)), intrE=float(m.group(6)),
                    perfect=float(m.group(8)), photon=float(m.group(10)))
    return mats

def parse_diag(path):
    out = {}
    for line in open(path):
        if line.startswith("#"): continue
        p = line.split()
        if len(p) >= 6:
            try: out[float(p[0])] = dict(n4=int(p[1]), meanIncl=float(p[2]),
                                         intr=float(p[4]), intrE=float(p[5]))
            except ValueError: pass
    return out

sim  = parse_sim(SIM)
exp  = {"dsb1": parse_scan(f"{EXP}/scan_dsb1/EnergyScanDSB1_summary.txt"),
        "luag": parse_scan(f"{EXP}/scan/EnergyScan_summary.txt")}
expd = {"dsb1": parse_diag(f"{EXP}/summary/DiagDiff_DSB1.txt"),
        "luag": parse_diag(f"{EXP}/summary/DiagDiff_LuAG.txt")}

ENER = [1, 3, 5, 7, 9, 11]
def rel(a, b): return f"{100*(a/b-1):+.0f}%" if (b and a) else "  -  "

for mat, label in (("dsb1", "DSB1"), ("luag", "LuAG:Ce")):
    print(f"\n{'='*104}\n{label}\n{'='*104}")
    print(f"{'E':>3}  {'RESPONSE (SumLG peak, ADC-eq)':^34}   {'INTRINSIC sigma_t (ps, reference-free)':^36}")
    print(f"{'GeV':>3}  {'sim':>10} {'measured':>10} {'diff':>8}   {'sim':>13} {'measured':>13} {'ratio':>7}")
    for E in ENER:
        s, e, ed = sim.get(mat, {}).get(E, {}), exp[mat].get(E, {}), expd[mat].get(E, {})
        if not s: continue
        sp, ep = s.get("peak", 0), e.get("peak", 0)
        si, ei = s.get("intr", -1), ed.get("intr", -1)
        note = ""
        if E == 1: note = "  (both响 fits rail at the SMIN+30 limit)"
        print(f"{E:>3}  {sp:>10.0f} {ep:>10.0f} {rel(sp,ep):>8}   "
              f"{si:>8.0f}+-{s.get('intrE',0):<4.0f} {ei:>8.0f}+-{ed.get('intrE',0):<4.0f} "
              f"{(si/ei if ei>0 and si>0 else 0):>6.1f}x")
    print(f"\n{'E':>3}  {'REFERENCE-INCLUDED shower time (ps)':^36}   {'sigma_E/E (%)':^26}")
    print(f"{'GeV':>3}  {'sim +110ps ref':>15} {'measured':>13} {'diff':>6}   {'sim':>11} {'measured':>11}")
    for E in ENER:
        s, e = sim.get(mat, {}).get(E, {}), exp[mat].get(E, {})
        if not s or "tmed" not in s: continue
        print(f"{E:>3}  {s['tmed']:>10.0f}+-{s['tmedE']:<3.0f} {e.get('tmed',0):>8.0f}+-{e.get('tmedE',0):<3.0f} "
              f"{rel(s['tmed'], e.get('tmed',0)):>6}   {s.get('res',0):>10.1f} {e.get('res',0):>10.1f}")
