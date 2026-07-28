# RADiCALsimWrap — does an outer Tyvek wrap help?

A fork of [`RADiCALsimSIMPLE`](../RADiCALsimSIMPLE/README.md) that asks exactly
one question: **the papers only put Tyvek *between* the LYSO/W layers — does
wrapping the *outside* of the assembled module (the faces you'd touch handling
it) in Tyvek buy detected light, and what does it cost in timing?**

Everything else is identical to SIMPLE and works the same way. Read
[SIMPLE's README](../RADiCALsimSIMPLE/README.md) first; this file covers only
what's different.

---

## Run it on perseverence (copy-paste, in order)

```bash
cd ~/Research/RADiCAL2026 && git pull
```

```bash
cd ~/Research/RADiCAL2026/simulations/RADiCALsimWrap && mkdir -p build && cd build && source ../setup_env.sh && cmake .. -DCMAKE_PREFIX_PATH=$CONDA_PREFIX && make -j
```

```bash
cd ~/Research/RADiCAL2026/simulations/RADiCALsimWrap && nohup bash run_wrap_scan.sh &
```

No redirect needed — the script logs itself to `build/logs/` (repo-wide
convention, see [simulations/README_LOGGING.md](../README_LOGGING.md)). Watch
the **Geant4** output, which is where the `--> Event N starts` progress lines
are:

```bash
tail -f ~/Research/RADiCAL2026/simulations/RADiCALsimWrap/build/logs/tyvek_geant4.log
```

The script's own log (config banner, ETA, per-stage timing) is alongside it at
`build/logs/run_wrap_scan.log`.

Then back on the Mac:

```bash
cd ~/Research/RADiCAL2026/simulations/RADiCALsimWrap && bash pull_wrap_results.sh perseverence && bash stage_control.sh ../RADiCALsimSIMPLE/build && root -l -b -q analysis/wrap_scan.C
```

---

## What it runs

**One configuration**: Tyvek (R = 0.98 diffuse — the same value the
inter-layer foils already use) on the module's 4 exposed side faces, with a
0.1 mm air gap. 2500 events at the **same 6 energies as SIMPLE's `run.mac`**
(5/10/25/50/100/120 GeV), so the comparison against the control is
point-for-point.

**The control is staged, not re-run.** `RADWRAP_SIDES` defaults to 0, so
`radwrap` with no flags builds geometry **byte-identical to `radsimple`** — the
RADiCALsimSIMPLE sweep you already have *is* the no-wrap control:

```bash
bash stage_control.sh ../RADiCALsimSIMPLE/build
```

Two caveats before trusting a comparison (details in the script header): the
control was run with different random seeds (a statistical baseline, not
event-by-event — fine at thousands of events), and its physics flags
(`RADSIMPLE_LIGHT_SCALE` etc.) must not have been overridden.

## How long it takes (measured on the cluster this time)

**~5–6 h on 512 cluster threads** for the default run (2500 events × 6
energies with the wrap). From measured numbers: cluster logical cores do
**6.0 core-s per event per GeV** (curiosity file-timestamp calibration,
2026-07-28 — linear in energy to <1%), and the wrap costs **~2.2x** over
no-wrap (measured: 149 s vs 323 s for 12 events). Runtime is ∝ events ×
ΣE, so E50+E100+E120 are 87% of the total — trim high energies for speed.

> Two earlier claims here were wrong, in opposite directions: "the wrap is
> ~100x slower" (it's 2.2x) and "the sweep takes ~1.2 h" (that used the Mac's
> per-core speed — cluster cores are ~5x slower for optical stepping). Both
> numbers above are now from direct measurement.

---

## Why this is a real question, not a formality

arXiv:2401.01747 §2: Tyvek sheets "were inserted **between successive layers**
to act as reflective spacers." arXiv:2303.05580 §II: tiles are "separated by
laser-cut sheets of Tyvek, and placed in a milled delrin housing." **Neither
paper puts a reflector on the outside of the assembled stack.**

Why adding one might *not* obviously help:

- LYSO has **n = 1.81**, so at a bare LYSO/air side face any photon beyond the
  critical angle (~33.6°) is **already totally internally reflected — free, at
  100%.** A wrap can only recover the *escape cone*.
- A wrap in **optical contact** (no air gap) **destroys that free TIR** and can
  *lose* light on net. Real Tyvek isn't index-matched — it rests against the
  surface with microscopic air voids — so the sim uses a small air gap
  (`RADWRAP_GAP_MM`, default 0.1 mm).

---

## Reading the output

`analysis/wrap_scan.C` prints one table per energy (tyvek vs none) plus a
**VERDICT** block: light / σ_t / energy-resolution change vs no wrap. Negative
σ_t or energy-resolution change = **better**; negative light = **worse**.

Rows are flagged automatically when they can't be trusted:
`[LOW STATS, do not trust]` (fewer than 100 events) and
`[sigma_t BIASED: dim events dropped]` (timing efficiency below 99% — the
surviving events are the brighter ones, so σ_t reads optimistic).

Plots land in `results/plots/`: `npe_vs_E.png`, `npe_gain_vs_E.png`,
`sigma_t_vs_E.png`, `sigma_E_Npe_vs_E.png`, and
`fits/dT_<config>_E<N>GeV.png` for the histogram behind every σ_t point.

**σ_t absolute values are meaningless here** — light is thinned 100x
(`RADSIMPLE_LIGHT_SCALE=1e-2`), so σ_t reads ~10x worse than true light. The
wrap-vs-control *relative* comparison is the result.

---

## Flags (set by run_wrap_scan.sh; listed for driving the binary directly)

| var | default | meaning |
|-----|---------|---------|
| `RADWRAP_SIDES` | **0** | wrap the 4 long faces |
| `RADWRAP_ENDS` | **0** | also cap the upstream/downstream faces (beyond the SiPMs) |
| `RADWRAP_REFLECTIVITY` | 0.98 | 0–1 |
| `RADWRAP_FINISH` | `diffuse` | `diffuse` (Lambertian, Tyvek) or `specular` (foil-like) |
| `RADWRAP_GAP_MM` | 0.1 | air gap between stack and wrap; `0` = optical contact (kills TIR) |
| `RADWRAP_THICK_MM` | 0.2032 | wrap thickness (one 0.008" foil) |

All of SIMPLE's flags (`RADSIMPLE_LIGHT_SCALE`, `RADSIMPLE_THREADS`, …) work
identically here. The reflectivity/finish/gap knobs exist so a different wrap
material can be tried later by just setting env vars — no code change.

The output ntuple is SIMPLE's full schema (per-layer energies, per-corner
light and times, beam-spot x/y, optional `RADSIMPLE_STORE_PHOTON_TIMES=1`
photon dump) — see SIMPLE's README "What's in the ntuple". Files produced
before 2026-07-28 (including the first tyvek run) have only the original 8
columns; `wrap_scan.C` works with either.

---

## What is deliberately not modeled

- No mechanical realism for the wrap — no seams, wrinkles, or tape. It's an
  idealized uniform skin.
- No wavelength dependence of the wrap's reflectivity (flat across
  350–800 nm), the same simplification already used for the inter-layer foils.
- Nothing else about SIMPLE's geometry changed, on purpose — so any measured
  effect is attributable to the outer wrap alone.
