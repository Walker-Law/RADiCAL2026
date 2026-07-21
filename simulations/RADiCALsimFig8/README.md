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

The paper's Fig. 8 methodology: a **single downstream SiPM**, timing set by the
**measured rise time** of its pulse (not a first-photon or corner-subtraction
estimator). This sim adds:

- **H1[38] DeltaT_SingleDown** — downstream WLS pulse 5% CFD leading-edge time
  minus the fiber's energy-weighted signal time = the pure photostatistics
  single-SiPM σ_t (`analysis/plot_fig8.C`'s `coreSigmaPeak()` fits the
  **peak-centered** Gaussian core, excluding the LYSO 36 ns decay tail).
- **H1[39] PhotonsWLSDown** — downstream-only WLS photon count, the correct LY
  denominator for a single SiPM (NOT the all-corner/both-end `PhotonsWLS`, which
  is ~8× too high for this comparison).

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

## Corrected geometry (important — supersedes earlier runs)

An earlier version of this sim modeled the corner capillary's straight sections
as **hollow** (air-bore) quartz, based on a literal reading of the paper's "hollow
quartz tube" phrase. The paper's next sentence clarifies the bore is **"filled
and fused with quartz rods"** — the light guide is optically **solid**. The air-bore
version broke total-internal-reflection light transport (photons trapped in the
air core bounced pathologically), producing unphysical, non-monotonic σ_t vs LY
(σ_t *increasing* with more light). Any `optical_scan_*_ly*` data predating this
fix should be discarded and re-run.

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
