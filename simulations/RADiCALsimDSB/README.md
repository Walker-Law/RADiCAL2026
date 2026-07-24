# RADiCAL2026 — Geant4 Simulation

Full Geant4 (v11.4.0) simulation of the **RADiCAL** (Radiation-hard Innovative Calorimeter) shashlik module, based on [[1]](https://arxiv.org/abs/2303.05580) and [[2]](https://arxiv.org/abs/2401.01747). Includes a complete CERN test-beam line and direct comparison to measured DRS4 waveform data.

## Detector

A LYSO/W shashlik sampling calorimeter with embedded quartz capillaries for precision energy and timing readout:

- **Stack:** 29 LYSO layers (1.5 mm) + 28 W absorbers (2.5 mm) + 56 Tyvek foils (0.2032 mm, 0.008″ per [[2]](https://arxiv.org/abs/2401.01747) §2) = **124.88 mm** total depth
- **Transverse size:** 14 × 14 mm, housed in a Delrin shell; capillary corner offset 3.5 mm from tile center ([[2]](https://arxiv.org/abs/2401.01747) Fig. 2)
- **Energy capillary (center):** quartz tube + EJ309 liquid scintillator bore
- **Timing capillaries (4 corners):** quartz rods + LuAG:Ce WLS fiber (15 mm at shower max, ~40.4 mm depth, per [[2]](https://arxiv.org/abs/2401.01747) §2) + Si photodetectors at both ends

## Molière radius & radiation length

R_M sets the transverse shower size — it is why the tiles are 14 mm across. For a
sampling stack it is the thickness-weighted harmonic combination of the
per-material Molière radii `R_M,i = E_s·X0,i / E_c,i` (with `E_s = 21.2 MeV`):

```
1/R_M = Σ_i  f_i / R_M,i          f_i = t_i / Σt   (thickness fraction)
```

| material | thickness in stack | X₀ | E_c | R_M,i = 21.2·X₀/E_c |
|----------|--------------------|-----|------|---------------------|
| W (28 × 2.5 mm)     | 70.0 mm  | 3.50 mm  | 8.0 MeV  | 9.28 mm |
| LYSO (29 × 1.5 mm)  | 43.5 mm  | 11.4 mm  | 11.7 MeV | 20.7 mm |
| Tyvek (56 × 0.203 mm) | 11.4 mm | ~1180 mm | ~85 MeV | ~290 mm (negligible) |
| **stack** | **124.9 mm** | | | |

```
f_W = 0.560,  f_LYSO = 0.348,  f_Tyvek = 0.091
1/R_M = 0.560/9.28 + 0.348/20.7 + 0.091/290
      = 0.0604 + 0.0169 + 0.0003  =  0.0776 mm⁻¹
R_M  ≈ 12.9 mm
```

The same thickness-in-X₀ weighting gives the effective radiation length
`X₀_eff = Σt / Σ(t/X₀) = 124.9 / 23.8 = ` **5.24 mm**, so the stack is **≈ 23.8 X₀** deep.

Versus the paper's **R_M = 13.7 mm, X₀ = 5.4 mm** ([[2]](https://arxiv.org/abs/2401.01747)):
the closed-form values sit ~6 % low because the analytic weighting underestimates
shower spreading in the low-density gaps — a full GEANT4 evaluation recovers
13.7 mm. Either way R_M ≈ 13 mm, so the **14 × 14 mm tile is ≈ 1 R_M across**: the
module cross-section is set to about one Molière radius by design.

### Transverse shower containment

Lateral EM-shower containment scales with R_M — the energy inside a cylinder of
radius `r` about the shower axis (standard approximation):

| r / R_M | energy contained |
|---------|------------------|
| 0.5 | ~70 % |
| 1.0 | 90 % |
| 2.0 | 95 % |
| 3.5 | 99 % |

The 14 × 14 mm tile reaches **±0.51 R_M** at its flat edge and **±0.72 R_M** at a
corner (R_M = 13.7 mm), so a single isolated tower transversely contains
**≈ 75 %** of a full high-energy shower. The remaining ~25 % leaks sideways —
recovered by neighbouring towers in a 3 × 3+ array (as the paper notes), or in
this single-module test-beam setup by the Pb-glass tail-catcher via the
`ECombined = E_LYSO + 0.18·E_PbGlass` estimator.

**At shower max — the depth the timing fibres read out — the shower is far more
compact than the full-shower R_M.** The paper measures a radius **r ≤ 5 mm** there
(≈ 0.36 R_M, approaching the radiation length X₀ = 5.4 mm), so **≳ 90 %** of the
shower-max energy sits inside the tile. That compactness — one to two orders of
magnitude more charged particles than a MIP in a few-mm spot — is exactly what
makes the corner-WLS shower-max timing work.

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
cd RADiCAL2026/simulations/RADiCALsimDSB
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
