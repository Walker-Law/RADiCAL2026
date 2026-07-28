# RADiCAL2026

Research Geant4 simulation repository for the RADiCAL project.
Based on [[1]](https://arxiv.org/abs/2303.05580) and [[2]](https://arxiv.org/abs/2401.01747).

## References

1. V. Beresovskyi et al., *"RADiCAL: a Radiation-hard Innovative Calorimeter"*, arXiv:[2303.05580](https://arxiv.org/abs/2303.05580) (2023).
2. C. Perez-Lara et al., *"Study of time resolution measurements and prospects for energy resolution of an ultra-compact sampling calorimeter (RADiCAL)"*, NIM A **1068** (2024) 169737, arXiv:[2401.01747](https://arxiv.org/abs/2401.01747).

## Repository layout

```
RADiCAL/                     Test-beam data and ROOT analysis
  Data/                      DRS4 waveform files (RUN1211, RUN1259–1261)
  Analysis/                  ROOT macros for data analysis
  legacy/                    Older analysis code (reference)

simulations/                 Geant4 simulations
  RADiCALsimDSB/             ★ Primary full-module sim — DSB1 WLS fiber (fast,
                               3.5 ns decay). Solid quartz capillary rods (the
                               paper's "hollow tube" core is filled and fused
                               with quartz — an earlier air-bore model broke
                               light transport and was reverted), optically
                               active LYSO→DSB1 WLS chain, dual-gain SiPM readout
                               (onsemi MicroFJ-30035), full CERN test-beam line,
                               DRS4 waveform emulation. Actively developed.
  RADiCALsimLuAG/            Identical geometry with LuAG:Ce WLS fiber (slow,
                               60 ns decay) — material comparison against DSB.
  RADiCALsimFig8/            Focused recreation of ref [2] Fig. 8: single
                               downstream-SiPM timing resolution vs detected
                               light yield (npe/MeV), 50 GeV e⁻, rise-time (CFD)
                               method. LY sweep via run_fig8_sweep.sh.
  RADiCALsimHoleScan/        Light output at the capillary ends vs the diameter
                               of the tile holes (1.2–2.0 mm sweep, 50 GeV); all
                               five capillaries scale to fill their hole. Center
                               EJ309 capillary made optically active (with its
                               own end PDs) so it counts as the 5th light source
                               alongside the 4 corner WLS caps. run_hole_scan.sh.
  RADiCALsimLadder/          Photostatistics scale-ladder timing study (reuses the
                               RADiCALsimDSB binary): sweeps a coherent light factor
                               f, fits σ_t²(f)=A²/f+B² to separate photon-counting
                               from the light-independent floor, extrapolates to true
                               light. Found the sim over-collects light ~3–4×,
                               over-saturating the SiPM — see its README.
  RADiCALsimSIMPLE/          ★ Standalone sim — a simple, hand-readable
                               model of the LYSO/W + 4-corner WLS light chain with
                               NO electronics (no CFD/DRS4/gain/saturation/jitter):
                               timing = raw first-photon (t_down − t_up)/2. Own
                               CMakeLists/binary (radsimple), not a fork of DSB.
                               Now also models the CERN H2 test-beam line
                               (trigger counters, MCP-PMT timing reference,
                               Pb-glass tail-catcher) and the tile's central
                               capillary hole (always drilled, per the papers;
                               optionally instrumented as a 5th E-type channel)
                               — all as truth quantities, all behind flags so
                               they can be switched off. Gaussian beam spot
                               (2.9 mm σ) required once the central hole is
                               always present. run.mac sweeps 5–120 GeV at
                               5k events/energy; analysis/scan.C fits σ_t(E)
                               and σ_E/E(E) with per-energy fit plots.
                               pull_results.sh syncs cluster output back. See
                               its own README for the full flag table and an
                               idiot-proof usage guide.
  RADiCALphotonorigin/       Position→SiPM mapping study (where the beam hits vs
                               which corner SiPM lights up; S-curve reconstruction).
  RADiCALopticalcrosstalk/   Optical-photon trajectory viewer, colored by
                               originating corner capillary (crosstalk visual).
  H3generation/              Standalone generation study.
  firstsim/                  Early prototype (reference only).
```

The four `RADiCALsim{DSB,LuAG,Fig8,HoleScan}` directories share the same core
Geant4 code (they were forked from a common base) and differ in the WLS
material, geometry parametrization, observables, and study focus. `RADiCALsimDSB`
is the canonical, most up-to-date version; the others inherit fixes from it.

## Quick start

```bash
cd simulations/RADiCALsimDSB
source setup_env.sh            # sources Geant4 (auto-detects conda `g4` env)
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=$CONDA_PREFIX \
         -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX
make -j$(nproc)
./radical ../run_batch.mac    # 120 GeV electrons

# energy/timing scan across energies (embarrassingly parallel, all cores):
RADICAL_OPTICAL=1 RADICAL_ENERGIES="5 10 25 50 100 120" \
  bash ../run_scan.sh 2000 1
```

Optical-photon tracking is enabled with `RADICAL_OPTICAL=1` and is far slower
than the energy-only mode; the scan splits events into many single-thread chunks
across all cores and hadd-merges them per energy.

See [simulations/RADiCALsimDSB/README.md](simulations/RADiCALsimDSB/README.md)
and its `CLAUDE.md` for full detector, physics, and workflow documentation.

**Vis tip:** bare `./radical` opens the geometry viewer with no particle track
drawn — `vis_purple.mac` (the macro it auto-runs) no longer auto-fires
`/run/beamOn 1`. (Trajectories are controlled by `/tracking/storeTrajectory`,
not `/vis/viewer/set/hiddenMarker` — that command only hides step-point markers
occluded by solid geometry, not the track lines themselves; and since the old
macro fired its event before the interactive prompt even appeared, there was no
way to type a fix in time anyway.) Type `/run/beamOn 1` yourself whenever you
want to see optical-photon tracks (purple).

## Methodology note: light yield is a prediction, not a tuning knob

`RADICAL_LYSO_SCINT_SCALE` (and `RADICAL_SCINT_YIELD`) are **Monte Carlo thinning
factors**, not physics parameters. Full LYSO light is ~5×10⁸ optical photons per
event at 120 GeV — untrackable — so the sim tracks a fraction and the result must
be corrected **analytically**: the photostatistical part of σ scales as √(thinning),
while geometric/systematic floors do not. Setting the scale to make a plot match a
published curve is fitting the answer, and it silently breaks the other observable.

The detected light yield is instead something the simulation **predicts**, from
LYSO's datasheet 33200 ph/MeV, the sim's own computed light transport and trapping,
and the SiPM datasheet PDE. That prediction is currently **~50 npe/MeV** (stable
across 25–150 GeV). Note the "~25 npe/MeV" figure in older project notes is *not*
from the paper — the paper quotes no measured light yield.

Because the sim runs ~100× thinned, its photostatistics are ~10× worse than the
real detector, so agreement obtained at a thinned setting must be extrapolated
before it can be claimed. `run_scale_ladder.sh` + `analysis/plot_scale_ladder.C`
do this properly: they run a ladder of coherent light multipliers and fit
σ²(f) = A²/f + B², separating the light-dependent (photostatistics) term from the
light-independent (shower-sampling/geometric) floor, so the true-light answer is an
extrapolation rather than a knob. See `simulations/RADiCALsimDSB/CLAUDE.md`
("LIGHT YIELD IS A PREDICTION, NOT A KNOB") for the full reasoning and the
decision rule.

## Analysis: per-energy fit overlays

`analysis/scan_resolution.C` (present in all four sims) saves, for every energy
point in a scan, the raw reconstructed-energy and timing histograms with their
actual Gaussian-core fit drawn on top (μ, σ, σ/E or σ_t annotated) to
`build/plots/fits/energy_fit_E<N>GeV.png` and `timing_fit_<variant>_E<N>GeV.png`.
This makes the fit behind any point on a resolution curve directly inspectable,
rather than only the aggregate curve/table — e.g. to check whether the Gaussian
core fit is excluding a leakage tail sensibly, or to see the shape of the
distribution a σ/E number was drawn from.

## Key runtime knobs (RADiCALsimDSB / Fig8 / HoleScan)

| Env var | Purpose |
|---------|---------|
| `RADICAL_OPTICAL` | 1 = track optical photons (timing); 0 = energy/shower only (fast) |
| `RADICAL_BEAM_ENERGY_GEV` | Beam energy per run |
| `RADICAL_ENERGIES` | Space-separated energy list for `run_scan.sh` |
| `RADICAL_HOLE_DIAM_MM` | (HoleScan only) common tile-hole diameter; all capillaries scale to fill it |
| `RADICAL_LYSO_SCINT_SCALE` | Scale LYSO scintillation yield (tractability; default 1e-3) |
| `RADICAL_SCINT_YIELD` | Scale DSB1 self-scintillation yield |
| `RADICAL_ROD_SIGMA_ALPHA_DEG` | Quartz light-guide surface roughness (RMS microfacet slope, deg). 0 = polished/legacy (trapping ceiling); default 1.3 leaks near-critical rays for a realistically lossy guide. Lower capture → less SiPM saturation |
| `RADICAL_EJ309_SCINT_SCALE` | (HoleScan only) Scale EJ309 scintillation yield (center capillary) |
| `RADICAL_QUARTZ_CHER_KEEP` | Binomial-thin quartz Cherenkov (restore real scint:Cher ratio) |
| `RADICAL_SIPM_NPIX` | SiPM microcell count (default 5676 = MicroFJ-30035) |
| `RADICAL_SIPM_QE` | Flat SiPM PDE override; unset (default) = MicroFJ-30035 PDE(λ) curve. `0.36` reproduces pre-2026-07-22 runs |
| `RADICAL_WFM_REALISM` | `0` = legacy ideal pulse path (pre-2026-07-23). Default on: pulses built in mV with gain, microcell saturation, CR shaping, noise, DRS4 clip+quantization before the 5% CFD |
| `RADICAL_SPE_MV` | High-gain mV per true photoelectron (waveform gain). Tune so H1[39] clipped fraction matches data (3.8% @25 GeV, 74% @150 GeV) |
| `RADICAL_AMP_TAU_D_NS` | CR high-pass (AC-coupling) time constant; default 12 ns reproduces the measured 8.3 ns pulse FWHM |
| `RADICAL_ELEC_NOISE_MV` | White noise per 0.2 ns sample (default 0.5 mV) — the physical noise-over-slope timing term |
| `RADICAL_DRS4_VMAX_MV` / `RADICAL_DRS4_BITS` | Digitizer rail (830 mV) and quantization bits (12) |
| `RADICAL_SIPM_TAU_REC_NS` | SiPM microcell recovery time for the saturation model (default 0 = no recovery) |
| `RADICAL_WFM_PE_SCALE` | True-pe per detected photon override (default = 1/`RADICAL_LYSO_SCINT_SCALE`) |
| `RADICAL_SPTR_PS` | SiPM single-photon time resolution RMS (ps) |
| `RADICAL_OPT_MAXSTEP` / `RADICAL_OPT_TMAX` | Optical-photon step / time caps (tractability) |
| `RADICAL_MAX_OPT_PHOTONS` | Per-event optical-photon budget cap (raise for bright/large-hole events) |
