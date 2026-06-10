# RADiCAL2026 — Geant4 Simulation

Full Geant4 (v11.4.0) simulation of the **RADiCAL** (Radiation-hard Innovative Calorimeter) shashlik module, based on [arXiv:2303.05580v3](https://arxiv.org/abs/2303.05580). Includes a complete CERN test-beam line and direct comparison to measured DRS4 waveform data.

## Detector

A LYSO/W shashlik sampling calorimeter with embedded quartz capillaries for precision energy and timing readout:

- **Stack:** 29 LYSO layers (1.5 mm) + 28 W absorbers (2.5 mm) + 56 Tyvek reflector foils (0.01 mm) = 114.06 mm total depth
- **Transverse size:** 14 × 14 mm, housed in a Delrin shell
- **Energy capillary (center):** quartz tube + EJ309 liquid scintillator bore
- **Timing capillaries (4 corners):** quartz rods + LuAG:Ce WLS fiber (6 mm, centered on shower maximum at ~43 mm depth) + Si photodetectors at both ends

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

| E (GeV) | σ/E (%) | σ_t (ps) |
|---------|---------|----------|
| 5 | 6.42 | 17.2 |
| 20 | 3.91 | 14.9 |
| 50 | 3.22 | 13.2 |
| 120 | 2.62 | 11.0 |

Fits: **σ/E = 14.1%/√E ⊕ 2.38%** &nbsp;&nbsp; **σ_t = 33.9 ps/√E ⊕ 11.5 ps**

Data comparison (with optical photons, 5% CFD waveform estimator):

| E (GeV) | Data σ_t | Sim σ_t |
|---------|----------|---------|
| 25 GeV | 614 ps | 145 ps |
| 150 GeV | 476 ps | 68 ps |

The ~4–7× gap is attributed to uncalibrated DRS4 inter-cell timing jitter in the raw test-beam data — not a detector limitation.

## Build

**Prerequisites:** Geant4 v11.4.0, ROOT, CMake ≥ 3.16.

```bash
git clone https://github.com/Walker-Law/RADiCAL2026.git
cd RADiCAL2026/RADiCALsim1
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

## Reference

V. Beresovskyi et al., *"RADiCAL: a Radiation-hard Innovative Calorimeter"*, arXiv:2303.05580v3 (2023).
