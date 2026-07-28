# RADiCALsimWrap — does an outer reflective wrap help?

A fork of [`RADiCALsimSIMPLE`](../RADiCALsimSIMPLE/README.md) that asks exactly
one question: **the papers only put Tyvek *between* the LYSO/W layers — does
adding a reflective wrap on the *outside* of the assembled module (the faces
you'd touch handling it) buy detected light, and what does it cost in timing?**

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
cd ~/Research/RADiCAL2026/simulations/RADiCALsimWrap && nohup bash run_wrap_scan.sh > scan.log 2>&1 &
```

Watch it (each config prints its own elapsed time as it finishes):

```bash
tail -f ~/Research/RADiCAL2026/simulations/RADiCALsimWrap/scan.log
```

**Configs run in priority order, so you can stop early.** `tyvek` — the actual
question — finishes first. If the ETA is longer than you want, just
`pkill -f radwrap` once you have the configs you care about; everything
already written to `results/` stays valid.

Then back on the Mac:

```bash
cd ~/Research/RADiCAL2026/simulations/RADiCALsimWrap && bash pull_wrap_results.sh perseverence && bash stage_control.sh ../RADiCALsimSIMPLE/build && root -l -b -q analysis/wrap_scan.C
```

---

## How long it takes (measured, not guessed)

Calibrated 2026-07-28 on one core: **1.24 core-seconds per event per GeV** with
no wrap, and a reflective wrap costs about **2.2x** that (measured: 149 s vs
323 s for 12 events at 10 GeV). Runtime scales roughly linearly with beam
energy and event count.

Sanity check on real cluster hardware: that constant predicts 1.0 h for
SIMPLE's 5000-event × 6-energy sweep on curiosity's 512 cores, which is what it
actually took — so it transfers, it isn't just a laptop number.

The defaults (2500 events, 3 energies, 7 configs) come to roughly **34 h on 64
cores**. `run_wrap_scan.sh` prints its own ETA from the actual core count at
launch. To cut it down, run fewer configs:

```bash
RADWRAP_CONFIGS="tyvek black esr" bash run_wrap_scan.sh
```

> An earlier draft of this README claimed the wrap was ~100x slower. **That was
> wrong** — I misread a normal-speed run as pathological before measuring it.
> The real cost is ~2.2x.

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
- A wrap in **optical contact** (no air gap) **destroys that free TIR** and
  replaces a perfect mirror with a ~98% one, which can **lose** light on net.
  Real Tyvek isn't index-matched — it rests against the surface with
  microscopic air voids — so a small nonzero gap is the physical case.
  `RADWRAP_GAP_MM=0` exists precisely so this trap can be *measured* instead of
  argued about (that's the `tyvek_contact` config).

---

## The configs

Edit the `CONFIG_TABLE` in `run_wrap_scan.sh` to add more. Listed in the order
they run — most informative first.

| config | sides | ends | R | finish | gap | where the number comes from |
|---|---|---|---|---|---|---|
| `tyvek` | ✓ | | 0.98 | diffuse | 0.1 mm | same value this project already uses for the inter-layer foils |
| `black` | ✓ | | 0.02 | diffuse | 0.1 mm | absorbing bound; also the fast sanity check (runs at no-wrap speed) |
| `esr` | ✓ | | 0.985 | specular | 0.1 mm | 3M ESR / VM2000, manufacturer spec |
| `tyvek_ends` | ✓ | ✓ | 0.98 | diffuse | 0.1 mm | Tyvek on all 6 faces |
| `tyvek_contact` | ✓ | | 0.98 | diffuse | **0** | the TIR-destroying trap described above |
| `delrin` | ✓ | | 0.60 | diffuse | 0.1 mm | **estimate, not a measurement** — stands in for the Delrin housing the module already sits in. Read as "some diffuse reflection happens," not a quotable number |
| `mylar` | ✓ | | 0.90 | specular | 0.1 mm | aluminized mylar, typical vendor spec |
| `none` | | | – | – | – | the control — **staged, not run** (see below) |

Diffuse vs. specular is a real distinction that's easy to get backwards in
Geant4's UNIFIED model: with `finish=ground` and no specular constants set, the
Lambertian remainder is 1 (fully diffuse), which is correct for Tyvek. Specular
must be requested explicitly (`finish=polished` + `SPECULARSPIKECONSTANT=1`) —
that's what `RADWRAP_FINISH=specular` does.

---

## The control is staged, not re-run

`RADWRAP_SIDES` and `RADWRAP_ENDS` both **default to 0**, so `radwrap` with no
flags builds geometry **byte-identical to `radsimple`**. A RADiCALsimSIMPLE
sweep you already have *is* this study's `none` config — there's nothing to
convert.

```bash
bash stage_control.sh ../RADiCALsimSIMPLE/build
```

Two things to check before trusting a comparison (both in the script's header):

1. **Seeds differ.** `run_wrap_scan.sh` pins `/random/setSeeds` so its configs
   share common-mode shower fluctuations; SIMPLE's `run.mac` sets no seeds. So
   the control is a **statistical** baseline — compare means and widths within
   their errors, not event by event. Fine at thousands of events.
2. **Physics flags must match.** `RADSIMPLE_LIGHT_SCALE`,
   `RADSIMPLE_BEAM_SPOT_MM`, `RADSIMPLE_PDE` etc. have the same names and
   defaults in both sims. If the SIMPLE run overrode any, it isn't
   apples-to-apples.

Event counts do **not** need to match — different statistics just means
different error bars. Only the **energies** need to overlap. The wrap scan
defaults to 3 energies (10/50/120 GeV) while SIMPLE's `run.mac` ran 6; the
analysis takes the union and prints `-` where a config has no data.

---

## Reading the output

`analysis/wrap_scan.C` prints one table per energy plus a **VERDICT** block at
the end (light / σ_t / energy-resolution change vs. no wrap, at whichever
energy has the most comparable configs). Negative σ_t or energy-resolution
change = **better**; negative light = **worse**.

Rows are flagged automatically when they can't be trusted:

- `[LOW STATS, do not trust]` — fewer than 100 events; the Gaussian core fit is
  meaningless.
- `[sigma_t BIASED: dim events dropped]` — timing efficiency below 99%. Events
  where no corner saw light at both ends are silently dropped, so the survivors
  are the *brighter* ones and σ_t reads optimistic. This matters most for dim
  configs like `black`.

Plots land in `results/plots/` — one observable per figure, configs as the
series: `npe_vs_E.png`, `npe_gain_vs_E.png`, `sigma_t_vs_E.png`,
`sigma_E_Npe_vs_E.png`, and `fits/dT_<config>_E<N>GeV.png` for the histogram
behind every σ_t point.

**σ_t absolute values are meaningless here** — light is thinned 100x
(`RADSIMPLE_LIGHT_SCALE=1e-2`), so σ_t reads ~10x worse than true light. The
config-to-config *relative* comparison is the result.

---

## Flags

Normally set for you by `run_wrap_scan.sh`; listed for when you drive the
binary directly.

| var | default | meaning |
|-----|---------|---------|
| `RADWRAP_SIDES` | **0** | wrap the 4 long faces |
| `RADWRAP_ENDS` | **0** | also cap the upstream/downstream faces (placed beyond the SiPMs, so they catch only light that already missed a photodetector) |
| `RADWRAP_REFLECTIVITY` | 0.98 | 0–1 |
| `RADWRAP_FINISH` | `diffuse` | `diffuse` (Lambertian, Tyvek) or `specular` (ESR/mylar) |
| `RADWRAP_GAP_MM` | 0.1 | air gap between stack and wrap; `0` = optical contact (kills TIR) |
| `RADWRAP_THICK_MM` | 0.2032 | wrap thickness (one 0.008" foil) |

All of SIMPLE's flags (`RADSIMPLE_LIGHT_SCALE`, `RADSIMPLE_THREADS`, …) work
identically here.

---

## What is deliberately not modeled

- No mechanical realism for the wrap — no seams, wrinkles, or tape. It's an
  idealized uniform skin.
- No wavelength dependence of the wrap's reflectivity (flat across
  350–800 nm), the same simplification already used for the inter-layer foils.
- Nothing else about SIMPLE's geometry changed, on purpose — so any measured
  effect is attributable to the outer wrap alone.
