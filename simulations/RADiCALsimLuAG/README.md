# RADiCAL2026 — Geant4 Simulation

Full Geant4 (v11.4.0) simulation of the **RADiCAL** (Radiation-hard Innovative Calorimeter) shashlik module, based on [[1]](https://arxiv.org/abs/2303.05580) and [[2]](https://arxiv.org/abs/2401.01747). Includes a complete CERN test-beam line and direct comparison to measured DRS4 waveform data.

## Detector

A LYSO/W shashlik sampling calorimeter with embedded quartz capillaries for precision energy and timing readout:

- **Stack:** 29 LYSO layers (1.5 mm) + 28 W absorbers (2.5 mm) + 56 Tyvek foils (0.2032 mm, 0.008″ per [[2]](https://arxiv.org/abs/2401.01747) §2) = **124.88 mm** total depth
- **Transverse size:** 14 × 14 mm, housed in a Delrin shell; capillary corner offset 3.5 mm from tile center ([[2]](https://arxiv.org/abs/2401.01747) Fig. 2)
- **Energy capillary (center):** quartz tube + EJ309 liquid scintillator bore
- **Timing capillaries (4 corners):** quartz rods + LuAG:Ce WLS fiber (15 mm at shower max, ~40.4 mm depth, per [[2]](https://arxiv.org/abs/2401.01747) §2) + Si photodetectors at both ends

## Test-beam line

| Element | z position | Purpose |
|---------|-----------|---------|
| Trigger scintillators × 2 | −400, −350 mm | Beam coincidence + TOF |
| MCP fused-silica radiator | −250 mm | t₀ timing reference |
| RADiCAL module | 0 mm | Module under test |
| Pb-glass calorimeter | +320 mm | Tail catcher / leakage recovery |

## Physics

- **Beam:** 120 GeV electrons (default), configurable 5–120 GeV via `RADICAL_BEAM_ENERGY_GEV`
- **Physics list:** FTFP_BERT
- **Optical photons:** Cherenkov + LuAG:Ce scintillation + WLS + boundary transport (off by default; `RADICAL_OPTICAL=1` to enable — ~190× slower)
- **Energy estimator:** `E_combined = E_LYSO + 0.18 · E_PbGlass` (tail-catcher correction)
- **Timing estimator:** downstream − upstream ΔT with DRS4-style waveform emulation (5% CFD)

## Results

Energy scan (1500 events/point, no optical):

| E (GeV) | σ/E (%) | σ_t = σ(ΔT)/2 (ps) |
|---------|---------|---------------------|
| 5 | 6.42 | 8.6 |
| 20 | 3.91 | 7.5 |
| 50 | 3.22 | 6.6 |
| 120 | 2.62 | 5.5 |

σ_t = σ(ΔT)/2 per the (DW−UP)/2 corner trick: ΔT = t_down − t_up = (L−2z)/v_g cancels MCP, DRS4 timebase, and beam-arrival jitter in the subtraction; dividing by 2 recovers the physical timing resolution.

Fits: **σ/E = 14.1%/√E ⊕ 2.38%** &nbsp;&nbsp; **σ_t = 16.9 ps/√E ⊕ 5.7 ps**

Optical scan (1000 events/point, LuAG:Ce scintillation, FTFP_BERT + EMopt4 + 0.1mm cuts, σ_t = σ(ΔT)/2):

| E (GeV) | LY (npe/MeV) | σ_t (ps) | Theory floor (ps) | Geom excess (ps) |
|---------|-------------|---------|-------------------|-----------------|
| 5 | 855 | 67.2 | 17.8 | 64.8 |
| 10 | 771 | 61.1 | 18.7 | 58.1 |
| 20 | 608 | 58.8 | 21.1 | 54.9 |
| 50 | 306 | 57.1 | 29.7 | 48.7 |
| 100 | 168 | 55.2 | 40.1 | 38.0 |
| 120 | 147 | 55.5 | 42.9 | 35.2 |

σ_t lies above the photostatistics floor due to geometric shower-depth spread in the 15 mm WLS fiber. Geometric excess decreases with energy (65→35 ps) as showers become more reproducible in depth.

Data comparison (with optical photons, 5% CFD, σ_t = σ(ΔT)/2):

| E (GeV) | Data σ_t (DRS4-uncorrected) | Sim CFD σ_t |
|---------|----------------------------|-------------|
| 25 GeV | 307 ps | ~72 ps |
| 150 GeV | 238 ps | ~34 ps |

The ~4–7× gap is attributed to uncalibrated DRS4 inter-cell timing jitter in the raw test-beam data — not a detector limitation.

## Build

**Prerequisites:** Geant4 v11.4.0, ROOT, CMake ≥ 3.16.

```bash
git clone https://github.com/Walker-Law/RADiCAL2026.git
cd RADiCAL2026/simulations/RADiCALsim1
source setup_env.sh          # set Geant4 data paths
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Run

```bash
cd build
source ../setup_env.sh

# Batch run (2500 events, 120 GeV)
./radical ../run_batch.mac

# OpenGL geometry viewer
./radical

# Energy scan (5–120 GeV, 1500 evt/point)
./run_scan.sh

# With optical photons (slow — ~34 s/event at 120 GeV)
RADICAL_OPTICAL=1 RADICAL_BEAM_ENERGY_GEV=25 ./radical ../run_batch.mac
```

## Analysis

```bash
# Energy + timing resolution curves
root -l -b -q analysis/scan_resolution.C

# Test-beam plots for a single energy file
root -l -b -q 'analysis/plot_testbeam.C("build/radical_output.root", 120)'

# Sim vs data comparison table
root -l -b -q analysis/compare_data.C

# Sim vs data summary plot (sigma_vs_E.png)
root -l -b -q analysis/compare_graphs.C
```

Output histograms (24 TH1D + 15 TH2D) are written to `build/radical_output.root`. See `CLAUDE.md` for the full histogram inventory and technical reference.

## References

1. V. Beresovskyi et al., *"RADiCAL: a Radiation-hard Innovative Calorimeter"*, arXiv:[2303.05580](https://arxiv.org/abs/2303.05580) (2023).
2. C. Perez-Lara et al., *"Study of time resolution measurements and prospects for energy resolution of an ultra-compact sampling calorimeter (RADiCAL) module at EM shower maximum over the energy range 25 GeV–150 GeV"*, Nucl. Instrum. Methods Phys. Res. A **1068** (2024) 169737, arXiv:[2401.01747](https://arxiv.org/abs/2401.01747).
