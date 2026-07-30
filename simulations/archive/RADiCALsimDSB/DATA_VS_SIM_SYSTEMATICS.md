# Systematic-Process Comparison — Test-Beam Data Analysis vs. RADiCALsimDSB

> **Scope.** A side-by-side of *how each pipeline turns raw information into
> energy- and timing-resolution numbers* — the "systematic process," not the
> physics inputs. Left column = the real CERN May-2023 test-beam analysis in the
> `RADiCAL/` repo (`RADiCAL/Analysis/`). Right column = the Geant4 simulation in
> `simulations/RADiCALsimDSB/`. Reference papers: arXiv:2303.05580 (earlier beam
> test) and arXiv:2401.01747 / NIM A 1068 (2024) 169737 (primary).
>
> **Status: documentation only.** No code changed. Every claim below is anchored
> to a `file:line` you can open and verify.
>
> _Written 2026-07-08._

---

## 1. The two pipelines at a glance

| | Data repo (`RADiCAL/`) | Sim (`RADiCALsimDSB/`) |
|---|---|---|
| Input | Real DRS4 waveforms (2 × CAEN DT5742, 5 GS/s, 1024 samples/ch) | Geant4 FTFP_BERT hits (dE/dx + optional optical photons) |
| Raw → observables | `Analysis/processRun.C` + `WaveformUtils.h` | `src/EventAction.cc` (`EndOfEventAction`) |
| Per-energy fit | `Analysis/timingEnergyBins.C`, `analyzeResolution.C` | `analysis/plot_testbeam.C` |
| Scan / curves | `Analysis/compareEnergies.C` + `harvestResults.C` | `analysis/scan_resolution.C` |
| Cuts (SSOT) | `Analysis/SelectionCuts.h` | hardcoded constants in `EventAction.cc` |
| Systematics | `Analysis/systematicUncertainties.C` | *(none)* |
| Scale | ~19,000 lines, 40+ macros | ~2,000 lines analysis + G4 action classes |

Both pipelines are structurally similar at the top (extract per-event
observables → iterative Gaussian-core fit → `a/√E ⊕ b`). They diverge on
**what the energy observable is, how the tail-catcher is used, and how much
detector realism sits between "light produced" and "number fitted."**

---

## 2. Stage-by-stage comparison

### 2.1 Digitization / raw model

| | Data | Sim |
|---|---|---|
| Sampling | Real DRS4 switched-capacitor, 0.200 ns nominal cell | Analytic pulse sampled at exactly 0.2 ns (`pulseCFD`, `EventAction.cc:73`) |
| Cell-width non-uniformity | Present (~5–15%); **stop cell recovered + trained correction** (`DRS4Calibration.h`) | None |
| Electronic noise / pedestal | Real; per-channel pedestal RMS stored (`processRun.C:264`) | None (noiseless) |
| Saturation | DRS4 clips ~830–950 mV; **flagged & excluded** (`WaveformUtils.h:215`, `SelectionCuts.h:148`) | None modeled |
| Spikes | Pedestal-window spike flag (`WaveformUtils.h:220`) | None |

**Key point.** The data pipeline spends most of its complexity *undoing the
digitizer*. The sim has no digitizer — its `pulseCFD` emulation is an idealized
"what the light itself supports." This single asymmetry explains most of the
timing-floor difference (see §3.2).

### 2.2 Pulse / observable extraction

| | Data (`WaveformUtils.h`) | Sim (`EventAction.cc`) |
|---|---|---|
| Pedestal | mean of samples 3–52 | n/a (pulse starts at 0) |
| Amplitude | pedestal − min (inverted) | pulse peak of summed single-photon responses |
| CFD | **6 fractions** 3/5/10/20/30/50%, linear-interpolated | single fraction, **5%** (`EventAction.cc:96`) |
| LED + TOT | yes (`ledTime`, `totTime`) | no |
| Charge integral | window `[imin−15, imin+200]` | no (energy is truth dE/dx) |

CFD fraction is **aligned at 5%** (data adopted 5% as its base after study —
`timingEnergyBins.C:368`; sim uses 5%). The data keeps the other five fractions
available for method comparison (`timingMethods.C`); the sim commits to one.

### 2.3 Energy estimator — **the biggest divergence**

| | Data | Sim |
|---|---|---|
| Observable | `sum_lg` = Σ of **8 low-gain capillary WLS-light peaks** (`processRun.C:283`) | `TotalLYSO` = **full dE/dx in 29 LYSO layers** (`EventAction.cc:153`) |
| Tail-catcher (Pb-glass) | **Containment veto**: reject if `sum_pb > 0.30·sum_lg` (`SelectionCuts.h:167`) | **Added back**: `ECombined = E_LYSO + 0.18·E_PbGlass` (`EventAction.cc:360`) |
| What it measures | sparse fiber **light collection** | idealized **calorimetric** deposit |
| Reported σ/E | ~11–19% across 25–150 GeV (`analyzeResolution.C:62`) | 14.1%/√E ⊕ 2.38% (≈2.6% @150) (`CLAUDE.md` scan table) |

These are **not the same measurement**:

- The data's energy is the light seen by a handful of point-like fibers — it is
  dominated by lateral shower sampling and containment, hence the large ~11–19%.
- The sim's `ECombined` is the total energy in the absorber stack with forward
  leakage *restored* by the Pb-glass — the idealized limit, hence ~2.6%.
- The two pipelines treat Pb-glass in **opposite directions**: the data *cuts*
  leaky events; the sim *recovers* them (exploiting the −0.94 LYSO/PbGlass
  anti-correlation, `EventAction.cc:356`).

The sim's `PhotonsWLS` photon-count estimator (`scan_resolution.C:87`) is the
first genuine analog of the data's light-based energy, and is the right object
to compare against the paper's Fig-17 shower-max resolution.

### 2.4 Timing estimator — aligned in spirit, different in construction

Both use the **MCP-free double-ended corner trick** `(DW−UP)/2`, where
`σ_t = σ(ΔT)/2` because `ΔT = (L − 2z)/v_g`.

| | Data (`timingEnergyBins.C`) | Sim (`EventAction.cc` §6b) |
|---|---|---|
| Headline observable | Method A `(mean{ch0–3} − mean{ch4–7})/2`, CFD-5% (`:283`) | per-corner `(t_down − t_up)/2`, first photon (`:272`) |
| Corner averaging | **4 corners averaged per event** (√4 noise reduction) | **per corner, pooled** (single-corner resolution) |
| Reference cancellation | MCP + DRS4 inter-group timebase cancel (see §3.3) | beam t0 cancels; MCP is perfect truth anyway |
| Also computed | Method B `(DW+UP)/2` (MCP1-ref), M7 walk-corrected, single-ch | `DeltaT_CFD` (H1[22], 5% CFD), scint-only (H1[24/25]), WLS-only (H1[29]) |

Two construction differences make even "matched" σ_t values not yet
apples-to-apples:

1. **Averaging.** The data averages the four corners *before* differencing (one
   value/event); the sim histograms each corner's own `down−up` (four
   values/event). The data therefore carries a ~√4 per-event averaging the sim's
   `H1[6]` does not — the sim number is a *single-corner* resolution.
2. **Estimator flavor.** The sim's default timing curve reads `DeltaT` = optical
   **first-photon** leading edge; the data's is a **5% CFD on the summed
   waveform**. The sim has a matching `DeltaT_CFD` (H1[22]), but the scan curve
   defaults to first-photon (`scan_resolution.C:55`).

### 2.5 Position, beam, fiducial

| | Data | Sim |
|---|---|---|
| Beam | Real SPS H2 e⁻, momentum spread ~0.1–0.2% | Gaussian pencil σ=2.9 mm at z=−500 mm (`PrimaryGeneratorAction.cc`) |
| Tracking | Delay-line wire chambers, `x = k(t_R−t_L)` (`ChannelConfig.h:59`) | none |
| Fiducial | **data-derived centroid** per run, r=2 mm (E) / 3 mm (t) (`SelectionCuts.h:125`) | acceptance cut `E_module_reco > E_PbGlass` (`EventAction.cc:369`) |

The data recomputes the beam centroid from signal-weighted tracks per run
(`ScanRunCenters`) and cuts around it; the sim relies on a centered Gaussian and
a Pb-glass halo veto. There is no WC-style position resolution term in the sim.

### 2.6 Selection cuts

- **Data:** single source of truth in `SelectionCuts.h` — WC-ok → MCP quality
  (200–750 mV) → fiducial → containment (30%) → per-channel HG/LG. Every
  threshold is named and commented with its physics rationale.
- **Sim:** a few inline constants (`kSamplingFrac = 0.18`, acceptance cut,
  core-fit ±2σ). No fiducial/MCP/containment cut hierarchy, because most of
  those failure modes (halo, saturation, bad reference) don't exist in the sim.

### 2.7 Resolution fitting — **aligned**

Both use an iterative Gaussian **core** fit (±2σ, ~4 iterations) to reject
non-Gaussian leakage tails, then fit `σ = √(a²/E + b²)`:

- Data: `PlotUtils.h` core fits; `timingEnergyBins.C`, `analyzeResolution.C`.
- Sim: `coreFit()` in `plot_testbeam.C:16` and `scan_resolution.C:12`.

The sim additionally adaptively rebins the energy histogram to ~σ/5
(`plot_testbeam.C:42`). This stage is the most faithfully mirrored of all.

### 2.8 Systematic uncertainties — **present in data, absent in sim**

The data budget (`systematicUncertainties.C`) re-runs the full timing analysis
under **8 cut variations** (fiducial ±0.5 mm, containment ±0.05, MCP lo/hi ±50
mV, HG threshold +5 mV) and quadrature-sums the shifts, using a truncation-bias-
corrected robust core σ. The sim reports statistical fit errors only; there is
no cut-variation or fit-model systematic.

---

## 3. Master divergence summary

| Aspect | Data | Sim | Aligned? |
|---|---|---|---|
| Energy observable | fiber WLS light (`sum_lg`) | LYSO dE/dx (`TotalLYSO`) | ✗ |
| Pb-glass role | containment **veto** | leakage **add-back** | ✗ (opposite) |
| CFD fraction | 5% base | 5% | ✓ |
| Timing estimator | `(DW−UP)/2`, 4-corner avg/event | `(down−up)/2`, per-corner | ~ (avg differs) |
| Timing flavor | 5% CFD on waveform | first-photon (default) | ~ (H1[22] matches) |
| MCP reference | split MCP, per-group jitter subtracted | perfect truth time | ~ |
| DRS4 cell/stop-cell | recovered + corrected | none | ✗ |
| Saturation/noise/spike | modeled + flagged | none | ✗ |
| Beam + fiducial | real beam, WC, data centroid | Gaussian + Pb-glass veto | ✗ |
| Cut architecture | SSOT header | inline constants | ✗ |
| Systematic budget | 8-variation quadrature | none | ✗ |
| Core Gaussian fit | ±2σ iterative, `a/√E⊕b` | ±2σ iterative, `a/√E⊕b` | ✓ |

---

## 4. Numerical scoreboard (as currently recorded)

| Quantity | Data repo | Sim (RADiCALsimDSB) | Paper (2401.01747) |
|---|---|---|---|
| σ_t @150 GeV | **≈37 ps** | first-photon ≈94 ps; 5% CFD ≈68 ps; scint-only ≈19 ps¹ | — |
| σ_t vs E fit | ≈181/√E ⊕ 34.9 ps | all-light 33.9/√E ⊕ 11.5 ps (geom); scint-only 90.1/√E ⊕ 17.1 ps | **256/√E ⊕ 17.5 ps** |
| σ/E | 11–19% (fiber light) | 14.1/√E ⊕ 2.38% (calorimetric) | — |
| Shower-max σ/E | — | dE/dx & photon-count estimators | 9.31 ⊕ 52.04/√E ⊕ 31.62/E (Fig 17) |
| MCP/ref floor | ~71 ps per DRS4 group (`mcpJitter.C`) | 0 (perfect) | — |

¹ Sim σ_t values are from `CLAUDE.md` (prior runs; a higher-statistics
validation run may supersede these).

### 4.1 The "500 ps vs 37 ps" reconciliation

`CLAUDE.md` records the sim team's own quick data analysis as
**σ_t ≈ 476–614 ps**, attributed to uncalibrated DRS4 timing, implying a ~10×
sim/data gap. **The `RADiCAL/` repo resolves this:** its MCP-free `(DW−UP)/2`
estimator keeps 3 of 4 corners **within a single DRS4 group**, so both the MCP
jitter and the inter-group timebase error cancel by construction
(`mcpJitter.C`: MCP1/MCP2 are one signal split across two mezzanines; per-group
floor ≈71 ps, and the within-group corners sit *below* it). With the right
estimator the real data is **≈37 ps at 150 GeV (≈181/√E ⊕ 35 ps)** — the same
ballpark as the paper (256/√E ⊕ 17.5 ps) and the sim's scint-only prediction.
The apparent 10× gap was the estimator, not the light.

The residual difference that *is* physical: the data's **constant term (~35 ps)**
exceeds both the paper (17.5 ps) and the sim scint-only (17 ps). That excess is
the real DRS4/electronics timing floor the sim does not model.

---

## 5. What "apples-to-apples" would require (not done here)

For a like-for-like data↔sim comparison, the sim side would need to:

1. **Energy:** compare the sim's **`PhotonsWLS`** (light-based) to the data's
   `sum_lg`, and/or switch `ECombined` to treat Pb-glass as a **veto** rather
   than an add-back — matching `SelectionCuts.h:167`.
2. **Timing averaging:** average the 4 corners **per event** before differencing
   (mirror `timingEnergyBins.C:283`) so the sim reports a 4-corner σ_t, not a
   single-corner one.
3. **Timing flavor:** use `DeltaT_CFD` (H1[22], 5% CFD) as the headline sim
   observable, not first-photon `DeltaT`.
4. **DRS4 realism:** inject cell-width/stop-cell/noise/saturation into the sim's
   waveform emulation to reproduce the ~35 ps electronics floor — or explicitly
   label the sim as the idealized "light-limited" reference.
5. **Systematics:** run the sim under the same 8 cut variations for a comparable
   uncertainty band.

Each is a discrete change; none has been made. This document records the gap.

---

## 6. File index (for verification)

**Data (`RADiCAL/Analysis/`):** `SelectionCuts.h`, `WaveformUtils.h`,
`ChannelConfig.h`, `DRS4Calibration.h`, `processRun.C`, `timingEnergyBins.C`,
`analyzeResolution.C`, `systematicUncertainties.C`, `mcpJitter.C`.

**Sim (`simulations/RADiCALsimDSB/`):** `src/EventAction.cc`,
`analysis/plot_testbeam.C`, `analysis/scan_resolution.C`, `CLAUDE.md`.
