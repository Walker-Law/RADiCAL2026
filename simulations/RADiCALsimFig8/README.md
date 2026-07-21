# RADiCALsimFig8 — Fig. 8 recreation (timing vs detected light yield)

Geant4 (v11.4.0) recreation of **arXiv:2401.01747 Fig. 8**: single-downstream-SiPM
timing resolution vs detected light yield (npe/MeV) for a 50 GeV e⁻ shower, using
the paper's rise-time (CFD) methodology. Forked from `RADiCALsimDSB` (identical
detector) with one additional histogram for the single-ended observable.

## Detector — ONE capillary only

Same LYSO/W shashlik stack as `RADiCALsimDSB`, but the capillary configuration is
the paper's Fig 8 setup, quoted verbatim:

> *"The simulation assumed only one SiPM readout at the downstream end of a T-type
> capillary inserted through the **center** of the module."*

- **One T-type timing capillary, at the module centre:** **solid** quartz rods
  (the paper's "hollow quartz tube" core is filled and fused with quartz rods, so
  the light guide is optically solid) + a **DSB1** WLS filament (900 µm dia,
  15 mm, at shower max).
- **One SiPM, downstream end only.** No upstream SiPM — this is a single-ended
  measurement, so the (DW−UP)/2 corner trick does not apply.
- **No corner capillaries and no EJ309 energy capillary.**

Two geometry constraints that are easy to get wrong and silently ruin the result:

1. **Do not pool multiple capillaries.** Filling the observable once per corner
   merges brightly- and dimly-illuminated channels into a **bimodal** distribution
   whose width is set by pulse rise time, not photostatistics — giving a
   light-yield-independent σ_t floor (~1.45 ns) that no estimator can remove.
2. **Keep the air gap.** The drilled hole (0.65 mm r) is deliberately larger than
   the capillary OD (0.575 mm r). That ~75 µm air gap provides the quartz→air
   total internal reflection. Sizing the hole to the OD puts quartz against LYSO
   (n=1.81), killing TIR — detected light drops from ~277 p.e. to ~1 p.e.

## What makes this a "Fig. 8" sim

The paper's measurement procedure, quoted:

> *"When the leading edge of the high-gain pulses from a given channel exceeded
> the threshold, the timing for that channel was determined and compared with the
> reference timing provided by the MCP tube."*

So: a **fixed absolute threshold** on the leading edge, referenced to the **MCP**
(a fast external clock, σ_t ≈ 10–20 ps). This sim implements exactly that:

- **H1[38] DeltaT_SingleDown** — `leadingEdgeFixed(downstream pulse, 2.5 p.e.) −
  t_MCP`, filled **once per event**. A fixed *absolute* threshold is essential: a
  5% CFD threshold is a fixed *fraction* of each pulse's own peak, so it samples
  the same photon-starved sliver of the leading edge at every light yield and
  cannot improve with light. Threshold tunable via `RADICAL_HG_THRESH_PE`.
- **H1[39] PhotonsWLSDown** — detected WLS p.e. at that single SiPM, the LY
  numerator. Filled **every** event including zeros; gating on `>0` would make the
  mean conditional on detecting light and inflate the LY axis at the dim points.
- `plot_fig8.C` fits the **dominant prompt peak** (`promptPeakSigma`). The late
  ~5 ns tail is a low-light-fluctuation population that shrinks as LY rises.
  Points where <90% of events clear the threshold are flagged **threshold-starved**
  and excluded — with fewer p.e./event than the threshold, only upward
  fluctuations register and the width is selection bias, not a measurement.

## Study: timing vs light yield

`run_fig8_sweep.sh [NEVT]` sweeps `RADICAL_LYSO_SCINT_SCALE` (and
`RADICAL_SCINT_YIELD`) across `1e-4 3e-4 1e-3 3e-3 1e-2 2e-2` at fixed
**50 GeV**, `NEVT` events per point (default 2000), writing
`build/scan/optical_scan_<NEVT>_ly<scale>/optical_E50GeV.root`. SPTR is off
(`RADICAL_SPTR_PS=0`) so the curve is the clean photostatistics limit; Cherenkov
is heavily thinned (`RADICAL_QUARTZ_CHER_KEEP=0.01`) since it doesn't enter the
WLS-only single-ended timing.

`analysis/plot_fig8.C(NEVT)` reads each point, computes LY = detected downstream
p.e. / fiber energy (MeV), fits σ_t via the peak-centered Gaussian core, and
overlays the paper's law **σ_t = 485 ps/√LY** on a log-log plot →
`build/plots/fig8_timing_vs_LY.png`.

## Result (July 21 2026, 1000 evt/point)

| LY (npe/MeV) | σ_t | in prompt peak | cleared threshold |
|---|---|---|---|
| 0.6 | 593 ps | 52% | 37% — starved, excluded |
| 1.6 | 1313 ps | 63% | 90% — starved, excluded |
| 4.8 | 838 ps | 74% | 97% |
| 13.8 | 465 ps | 49% | 96% |
| 46.5 | 370 ps | 69% | 97% |
| 91.7 | 279 ps | 77% | 97% |

**sim: σ_t = 1704 ps/√LY ⊕ 227 ps**  vs  **paper: 485 ps/√LY**

The shape is correct — monotonic and ~1/√LY — and the floor fell from ~1450 ps to
227 ps once the geometry was reduced to a single capillary. The absolute scale
remains ~3.5× above the paper. Open items, cheapest first:

1. Only 49–77% of events sit in the prompt peak, i.e. the 2.5 p.e. threshold is
   still marginal — try `RADICAL_HG_THRESH_PE=1.0`.
2. `leadingEdgeFixed` digitizes the waveform at 0.2 ns → ~58 ps quantization floor.
3. The LYSO **36 ns** decay gates the WLS light and softens the leading edge —
   likely the irreducible contribution.
4. DSB1's light yield is unpublished (a free parameter), so the LY↔timing
   normalization carries its own uncertainty.

## Geometry corrections (supersede earlier runs)

Two errors were found and fixed; data predating them is invalid and was discarded:

- **Hollow vs solid capillary.** An earlier version modeled the straight sections
  as hollow (air-bore) quartz from a literal reading of "hollow quartz tube". The
  paper's next sentence clarifies the bore is *"filled and fused with quartz
  rods"* — the light guide is optically **solid**. The air core broke TIR and gave
  non-monotonic σ_t (worse timing with *more* light).
- **Four pooled capillaries.** See the pooling/bimodality warning above — this was
  the dominant artifact and is why the σ_t floor sat at ~1.45 ns for so long.

## Build & run

```bash
git clone https://github.com/Walker-Law/RADiCAL2026.git
cd RADiCAL2026/simulations/RADiCALsimFig8
source setup_env.sh
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=$CONDA_PREFIX -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX
make -j$(nproc)
cd ..

# Fig 8 light-yield sweep (50 GeV, 6 yield points, all cores)
bash run_fig8_sweep.sh 1000        # events per point
```

## Analysis

```bash
root -l -b -q 'analysis/plot_fig8.C(1000)'   # arg = events/point used in the sweep

# Per-energy (here: per-yield-point) histogram + Gaussian-fit overlays
root -l -b -q 'analysis/scan_resolution.C("build/scan/optical_scan_1000_ly1e-2","optical")'
```

`scan_resolution.C` (shared with the other sims) also saves the raw reconstructed-
energy and timing histograms with their fitted Gaussian core overlaid, per energy
point, to `build/plots/fits/` — useful for inspecting the fit behind any resolution
number rather than trusting the summary curve alone. Since this sim's data is a
fixed-50-GeV light-yield sweep rather than an energy scan, that resolution *curve*
isn't meaningful here — `plot_fig8.C` is the real analysis for this study.

## References

1. V. Beresovskyi et al., *"RADiCAL: a Radiation-hard Innovative Calorimeter"*, arXiv:[2303.05580](https://arxiv.org/abs/2303.05580) (2023).
2. C. Perez-Lara et al., *"Study of time resolution measurements and prospects for energy resolution of an ultra-compact sampling calorimeter (RADiCAL) module at EM shower maximum over the energy range 25 GeV–150 GeV"*, Nucl. Instrum. Methods Phys. Res. A **1068** (2024) 169737, arXiv:[2401.01747](https://arxiv.org/abs/2401.01747).
