# RADiCALsim1 — Claude/AI Reference Guide

> Read this first. It captures everything needed to work on this project without
> re-deriving context. Keep it updated when geometry, materials, or workflow change.

## What this is

Geant4 (v11.4.0, FTFP_BERT) replication of the **RADiCAL** (Radiation-hard
Innovative Calorimeter) shashlik module from **arXiv:2303.05580v3**. Full
calorimetry simulation: 120 GeV electrons into a LYSO/W sampling stack with
embedded quartz capillaries for energy (EJ309) and timing (LuAG:Ce WLS) readout.

- **Repo:** GitHub `Walker-Law/RADiCAL2026` (public). Git root is `/Users/macro-2/Research`.
- **Project dir:** `/Users/macro-2/Research/RADiCALsim1/`
- **Sibling:** `firstsim/` is an earlier prototype — reference only, not the active sim.

## Directory layout

```
RADiCALsim1/
  CMakeLists.txt
  setup_env.sh          # sources Geant4 + sets all 12 data paths (USE THIS)
  vis.mac               # geometry-only viewer (NO beamOn)
  run_batch.mac         # /run/initialize + printProgress + beamOn N
  src/
    DetectorConstruction.cc   # ALL geometry, materials, vis attributes
    PrimaryGeneratorAction.cc # beam: 120 GeV e-, Gaussian sigma=2.9mm, z=-100mm
    EventAction.cc            # per-event accumulation + fills all histograms
    SteppingAction.cc         # routes edep by volume name to EventAction
    RunAction.cc              # defines all histograms, opens/writes radical_output.root
  include/                    # matching headers
  build/                      # cmake build dir; radical_output.root lands here
```

## Build & run

```bash
cd /Users/macro-2/Research/RADiCALsim1/build
source /Users/macro-2/Research/RADiCALsim1/setup_env.sh   # <-- ALWAYS source first
make -j$(sysctl -n hw.logicalcpu)
./radical                 # opens OpenGL viewer (geometry only)
./radical run_batch.mac   # batch physics run -> build/radical_output.root
```

Geometry overlap check:
```bash
printf '/run/initialize\n/geometry/test/run\n' > /tmp/check.mac
./radical /tmp/check.mac 2>&1 | grep -iE "overlap|OK|Exception"
```

### CRITICAL gotcha — Geant4 data paths
The installed data versions do NOT match older hardcoded names. `setup_env.sh`
sets the correct ones. If you ever see `PART70001`, `had014`, `em0003`, or
"data file ... is not opened", a data path is wrong. Correct versions on this
machine (under `/Users/macro-2/Research/geant4-install/share/Geant4/data/`):
`G4ENSDFSTATE3.0`, `PhotonEvaporation6.1.2`, `RadioactiveDecay6.1.2`,
`G4PARTICLEXS4.2`, `G4PII1.3`, `RealSurface2.2`, `G4SAIDDATA2.0`, `G4ABLA3.3`,
`G4INCL1.3`, `G4EMLOW8.8`, `G4NDL4.7.1`.

## Geometry (DetectorConstruction.cc)

Stack = **29 LYSO (1.5mm) + 28 W (2.5mm) + 56 Tyvek (0.01mm)** = `stackZ` 114.06 mm.
- Pattern: `LYSO(0)|Tyvek|W(0)|Tyvek|LYSO(1)|...|LYSO(28)`. Even tile=LYSO, odd=W.
- Tiles are 14×14 mm (±7 mm half-width). Shared logical volumes placed by copy number.
- One LYSO+W period = 4.02 mm. **Center of LYSO layer L = L*4.02 + 0.75 mm from upstream face.**
- Convention: beam travels +z, so "upstream" = −z end, "downstream" = +z end.
- Housing: Delrin shell, 18mm outer / 14mm inner cavity, `housingHalfZ`=65mm.
- World: ±25mm transverse, ±200mm z. **Module only spans ±9mm — beam offsets >~7mm miss it.**

### Capillaries (5 holes drilled via G4SubtractionSolid)
- **Center (energy):** `centerHoleR`=0.45mm. EJ309 bore (r=0.20mm) + quartz tube.
  `eCap_outR = centerHoleR` so the tube fully fills the hole (no air gap).
- **4 corners (timing):** `cornerHoleR`=0.65mm. Quartz upstream rod + quartz tube wall +
  **LuAG:Ce WLS fiber** (r=0.45mm) at shower max + quartz downstream rod.
  Photodetectors `PD_Upstream`/`PD_Downstream` (Si, copy#=corner) at the two ends.
- WLS segmentation auto-centers on shower max: `upstreamLen = showerMaxDepth - wlsLen/2`.
  Current: `showerMaxDepth`=43.0mm (measured peak ≈ layer 10–11, NOT geometric middle),
  `wlsLen`=6.0mm (covers 40–46mm), `upstreamLen`=40mm, `downstreamLen`=68.06mm, `z_wls`=−14.03mm.
  Volume names: `Cap_Corner_Upstream` / `Cap_Corner_Downstream` (quartz rods),
  `TCapUpstream_Phys` / `TCapDownstream_Phys` placements.

### Materials
LYSO, Tungsten (W), Tyvek, Delrin (POM), fused quartz, EJ309 liquid scintillator,
**LuAG:Ce** (Lu₃Al₅O₁₂:Ce, 6.73 g/cm³ — Lu 61.5%, Al 15.8%, O 22.6%, Ce 0.1%).
(LuAG:Ce replaced the original DSB1 WLS polymer.)

### Visualization (for inspection)
Housing + all tiles + quartz tubes = **wireframe** w/ `SetForceAuxEdgeVisible(true)`.
LYSO=blue, W=red, Tyvek=white. The two ACTIVE scoring volumes are kept **solid**:
EJ309 bore=green, LuAG:Ce fibers=orange — so they pop against the wireframe lattice.

## Scoring & histograms (RunAction.cc → radical_output.root)

Volume name → EventAction routing (SteppingAction.cc): `LYSO`, `W_Absorber`,
`Cap_Center_EJ309`, `Cap_Corner_WLS`.

**24 TH1D (H1[0–23]) + 15 TH2D (H2[0–14])** — last entries are the test-beam line
detectors (see CERN test-beam line section below). Full histogram inventory:

| ID | Name | Description |
|----|------|-------------|
| H1[0] | ShowerProfile | Energy/layer (longitudinal) |
| H1[1] | TotalLYSO | Sampled LYSO energy (GeV), 5000 bins 0–25 |
| H1[2] | TotalW | W absorber energy |
| H1[3] | SamplingFraction | E_LYSO/(E_LYSO+E_W) |
| H1[4] | CenterCapEnergy | EJ309 liquid scintillator yield (MeV) |
| H1[5] | CornerWLSEnergy | LuAG:Ce WLS per corner (MeV) |
| H1[6] | DeltaT | First-photon ΔT downstream−upstream (ns), 4000 bins −0.2→0.6 |
| H1[7] | ShowerMaxLayer | Layer of shower maximum |
| H1[8] | ShowerCOG | Energy-weighted longitudinal COG (layers) |
| H1[9] | ShowerRMS | Longitudinal shower RMS width (layers) |
| H1[10] | CenterCapFraction | E_EJ309/E_LYSO |
| H1[11] | CornerWLSPerCorner | WLS energy bar chart (x=corner index 0–3) |
| H1[12] | ZResidual | z_reco−z_true residual (mm) |
| H1[13] | TotalCornerWLS | Sum of all 4 corner WLS energies (MeV) |
| H1[14] | Trig1Edep | Trigger scint 1 dE |
| H1[15] | Trig2Edep | Trigger scint 2 dE |
| H1[16] | MCPEdep | MCP radiator dE |
| H1[17] | PbGlassEnergy | Pb-glass tail-catcher energy (GeV) |
| H1[18] | WLS_minus_MCP | RADiCAL WLS time − MCP t0 (ns) |
| H1[19] | TOF_Trig1_MCP | Trig1→MCP TOF (ns) |
| H1[20] | ECombined | Tail-catcher-corrected E = E_LYSO + 0.18·E_PbGlass (GeV) |
| H1[21] | PhotonsDetected | Detected optical photons/event (N_p.e.) |
| H1[22] | DeltaT_CFD | **Waveform 5% CFD ΔT** (data-identical estimator), 800 bins −4→4 ns |
| H1[23] | PulseFWHM | Emulated pulse FWHM (ns); validate vs data ~8.3 ns |

| ID | Name | Description |
|----|------|-------------|
| H2[0] | DeltaT_vs_TrueZ | Timing calibration matrix |
| H2[1] | ZReco_vs_ZTrue | Position reco diagonal |
| H2[2] | LateralProfile | Integrated XY energy map (70×70, ±7mm) |
| H2[3] | CenterCapVsLYSO | EJ309 vs LYSO linearity |
| H2[4] | CornerWLSVsLYSO | WLS vs LYSO correlation |
| H2[5] | DeltaTVsLYSO | ΔT vs sampled energy |
| H2[6] | ShowerMaxVsLYSO | Shower depth vs energy |
| H2[7–12] | LateralProfile_Slice0..5 | XY maps at 6 depth slices (layers 0–4, 5–9, 10–14, 15–19, 20–24, 25–28); routed in EventAction via `depthSliceH2(layer)` lambda |
| H2[13] | LYSOvsPbGlass | Tail-catcher correlation |
| H2[14] | MCPtime_vs_WLStime | MCP t0 vs WLS arrival timing correlation |

Timing recon uses the ACTUAL recorded WLS hit z-positions (not a hardcoded
constant), so moving the WLS section flows through automatically.

Inspect output:
```bash
root -l -b build/radical_output.root -e 'gDirectory->ls(); gApplication->Terminate();'
```

## Optical photons (timing) — toggle via env var
`RADICAL_OPTICAL=1 ./radical ...` enables Cherenkov + LuAG:Ce scintillation +
light-guiding + upstream/downstream photodetectors (20% QE) → real photon-based
downstream−upstream ΔT timing (H1[6] DeltaT, H1[21] PhotonsDetected). **OFF by default** (gated in
radical.cc) because it is ~190× slower (~34 s/event @120 GeV vs ~0.18 s; >4000
p.e./event). When OFF, the optical material tables/PDs sit inert and **DeltaT is
empty** (timing is optical-only now — the old geometric ΔT proxy was removed from
H1[6]). Energy/σ-E and shower profiles work in both modes. GPU accel (Celeritas/
AdePT) doesn't cover optical photons; Opticks does but needs NVIDIA+OptiX (N/A on
this Mac). Best speed lever: cut LuAG yield ~10× (still ~400 p.e., good timing).

## REAL test-beam data (June 2026 comparison)
Located `/Users/macro-2/Research/RADiCAL/Data/`: RUN1211 (25 GeV), RUN1259/60/61
(150 GeV), ~30k events each, 2 GB files. Format: TTree `pulse` with
`timevalue[4096]` (5 GS/s, 1022 ns window) + `amplitude[36864]` = **9 ch × 4096**.
- **Pileup**: 2–6 pulses/window — must select in-time pulses (±15 ns around each
  channel's mode time: MODE[9]={75,75,125,95,396,406,85,115,115} ns).
- **Channel map (inferred)**: ch0–ch1 = the instrumented timing-capillary
  upstream/downstream pair (tightest ΔT, matching mode times). ch6/ch8 narrow
  (~4 ns FWHM) = MCP/trigger-like. ch4–ch5 NOT a capillary pair (8 ns ΔT core).
- **Saturation**: DRS4 clips at ~830 mV → 74% of capillary pulses saturated at
  150 GeV (3.8% at 25 GeV). Amplitude analysis only valid at 25 GeV; timing OK
  (leading edge intact; non-sat subset σ_t=553 ps consistent w/ 502 full).
- **CFD convention (per user): 5% of peak**, not 50%.
- **Measured timing (5% CFD, iterative ±2σ core)**: σ_t(25)=614 ps,
  σ_t(150)=470–488 ps across the 3 runs (mean 476). Pulse FWHM ≈ 8.3 ns.
  (For reference, 50% CFD gave 558 / 502 ps.)
- Analysis snippets in /tmp during session; reference macro: analysis/compare_data.C.

### RESULT of data-vs-sim comparison (June 2026) — 5% CFD convention
| E (GeV) | DATA σ_t | SIM 5% CFD | SIM first-photon |
|---------|----------|------------|------------------|
| 25  | 614 ps | 145 ps | 100 ps |
| 150 | 476 ps |  68 ps |  94 ps |
(50%-CFD first pass for reference: data 558/502, sim 485/393 ps.)

**KEY FINDING:** data σ_t is nearly THRESHOLD-INDEPENDENT (5%: 614/476 vs 50%:
558/502) while the noiseless sim collapses to its photostatistics floor at 5%.
⇒ The real detector's ~500 ps is NOT photostatistics-limited; it is dominated by
a pulse-wide systematic that shifts whole waveforms event-to-event — most likely
uncalibrated DRS4 inter-cell timing (raw `timevalue` used, no cell-by-cell
calibration; typically several-hundred-ps RMS), plus amplifier rise time/noise on
the 5% crossing. The sim says the light itself supports ~70–145 ps.
Graphs: build/plots/datacomp/ (sigma_vs_E.png, data_deltaT_{25,150}GeV.png,
data_waveform_example.png) via analysis/compare_graphs.C + /tmp/dataplots.C.

### Waveform emulation added for data comparison
First-photon ΔT (~95–111 ps) is idealized. Added **waveform emulation** in
EventAction (`pulseCFD()`): sums single-photon responses
SPR(t)=(1−e^{−t/1.0ns})·e^{−t/3.0ns} over ALL detected photon arrival times
(stored in fPhTUp/fPhTDown vectors up to kMaxStore=60000), samples at 0.2 ns
(DRS4-like 5 GS/s), applies **5% CFD** (user-confirmed convention — `thr = 0.05 * pk`).
H1[22] DeltaT_CFD = data-identical estimator. H1[23] PulseFWHM = FWHM to
validate against data ~8.3 ns. Comparison sim runs (40 evt optical, `build/datacomp/`):
`./build/datacomp_run.sh` (handles merge-safety + retry).

## Test-beam analysis config (analysis/plot_testbeam.C)
Replicates CERN test-beam plots. Run per energy file:
`root -l -b -q 'analysis/plot_testbeam.C("build/radical_output.root", 120)'`
→ writes 4 PNGs to build/plots/ (energy res, timing res, long/lat shower).

Locked conventions (per user, June 2026):
- **Energy estimator** = tail-catcher-corrected `ECombined` = E_LYSO + f_s·E_PbGlass
  (f_s=0.18). The −0.94 LYSO/PbGlass anti-correlation lets the Pb-glass recover
  forward leakage → tightens σ/E. Filled in EventAction §7 as H1[20].
- **Beam-acceptance cut**: ECombined filled only if module-reco E (E_LYSO/f_s) >
  E_PbGlass — removes halo events that missed the ±7 mm module and showered in the
  Pb-glass (a spurious sharp peak at ~0.18·E_beam). Keeps genuine leakage events.
- **Energy fit** = iterative Gaussian core, ±2σ, 4 iterations (excludes leakage tail).
- **Timing** = downstream−upstream ΔT (H1[6]); MCP reference cancels. Gaussian core fit → σ_t.
  CAVEAT: σ_t here is a GEOMETRIC proxy (spread of energy-deposit z within the 6 mm
  WLS) — no optical-photon/photostatistics/electronics modeled. Trends vs E are
  meaningful; absolute ps value is not the real resolution.
- Energy hist `TotalLYSO`/`ECombined` = 5000 bins 0–25 GeV; macro adaptively rebins
  to ~σ/5 per energy. `DeltaT` = 2500 bins 0–0.5 ns (0.2 ps/bin).
- 120 GeV result: σ/E ≈ 2.6%, μ ≈ 16.9 GeV sampled; σ_t ≈ 10.8 ps, ΔT ≈ 136 ps.

### Energy scan (DONE — June 2026, 1500 evt/point)
Driver: `./run_scan.sh` — runs 5,10,20,50,100,120 GeV, one process per energy
(beam energy via env var `RADICAL_BEAM_ENERGY_GEV`, MT-safe), with the merge-safety
loop (kill procs + rm output + validate ECombined integral + retry). Writes
`build/scan/radical_E{N}GeV.root`.
Analysis: `root -l -b -q analysis/scan_resolution.C` → fits every energy file and
builds `build/plots/{energy,timing}_resolution_curve.png` + shower_long_overlay.png.
It ALSO writes `build/scan/resolution_curves.root` holding the resolution curves as
ROOT objects: `EnergyResolution` + `TimingResolution` (TGraphErrors, fit TF1 stored
inside each) and a `scan` TTree (E, sigmaE_pct, sigmaT_ps). `run_scan.sh` calls this
analysis automatically at the end, so the curves .root refreshes on every scan.

Results (1500 evt/point, tail-catcher energy, downstream−upstream ΔT):
| E (GeV) | σ/E (%) | σ_t (ps) |
|---------|---------|----------|
| 5   | 6.42 | 17.2 |
| 10  | 5.40 | 16.1 |
| 20  | 3.91 | 14.9 |
| 50  | 3.22 | 13.2 |
| 100 | 2.78 | 12.0 |
| 120 | 2.62 | 11.0 |
Fits: **σ/E = 14.1%/√E ⊕ 2.38%**;  **σ_t = 33.9 ps/√E ⊕ 11.5 ps** (σ_t floor is the
geometric WLS-spread proxy — no photostatistics; see caveat above). ΔT mean ≈137 ps
stable across energy (WLS at fixed z), confirming the downstream−upstream observable.

## Beam (PrimaryGeneratorAction.cc)
120 GeV e⁻, momentum +z, Gaussian spot σ=2.9mm centered at **(0,0,−500mm)** —
upstream of the first trigger counter so the beam traverses the full test-beam line.
**Currently centered** (an earlier −25mm offset request was reverted — it missed
the ±7mm module entirely).

## CERN test-beam line (DetectorConstruction.cc §9)
Full beamline replicated from a test-beam photo (standard defaults; a photo gives
no exact metrology — all params are gathered in §9 for easy correction).
Beam travels +z; RADiCAL module stays centered at z=0. World enlarged to
±120mm transverse, ±650mm z.

| Element | volume name(s) | z-center | size | material |
|---------|----------------|----------|------|----------|
| Trigger scint 1 | `Trig1` | −400mm | 30×30×5mm | plastic scint (vinyltoluene) |
| Trigger scint 2 | `Trig2` | −350mm | 30×30×5mm | plastic scint |
| MCP window (timing) | `MCP_Radiator` | −250mm | 27×27×3mm | fused silica (G4_SILICON_DIOXIDE) |
| MCP body | `MCP_Body` | −247mm | 27×27×3mm | Al₂O₃ (kept thin, <0.05 X0 preshower) |
| RADiCAL module | (see above) | 0 | — | LYSO/W |
| Pb-glass calo | `PbGlass` | +320mm | 100×100×400mm | G4_GLASS_LEAD (~30 X0) |

- MCP `MCP_Radiator` records earliest hit time = **t0 timing reference**.
- Trigger counters record dE + earliest time (coincidence + TOF).
- Pb-glass is the downstream **tail catcher** (sees ~4% leakage of 120 GeV).
- Scoring is edep + particle-passage time (NO optical-photon tracking — consistent
  with how the RADiCAL capillaries work). Adding Cherenkov/optical is a future option.

### New histograms (beamline) — appended to existing set
- H1[14] Trig1Edep, H1[15] Trig2Edep, H1[16] MCPEdep, H1[17] PbGlassEnergy,
  **H1[18] WLS_minus_MCP** (RADiCAL WLS time − MCP t0, the key resolution plot),
  H1[19] TOF_Trig1_MCP.
- H2[13] LYSOvsPbGlass (tail-catcher correlation), H2[14] MCPtime_vs_WLStime.

Validated (50 evt): Trig dE ≈1 MeV (MIP), TOF Trig1→MCP =0.504ns (geom 0.500ns),
t_WLS−t_MCP =0.81ns (geom 0.79ns), Pb-glass ≈5.3 GeV leakage. All physical.

## Git / GitHub workflow
- **Auto-push hook** in `~/.claude/settings.json` (PostToolUse, Write|Edit matcher):
  every edit under `/Users/macro-2/Research/*` auto-commits + pushes. So source
  changes land on GitHub automatically — no manual commit needed.
- `.gitignore` excludes build dirs, `*.root`, geant4 source/install, `.DS_Store`,
  CMake cruft. Do NOT commit large geant4 tarballs (previously broke the push).
- Commit author shows as MACRO-2 (git identity not globally configured — cosmetic).

## Conventions & gotchas
- Geant4 v11.4.0 has **no** `G4StrUtil.hh` — use `str.find(...) != G4String::npos`.
- After renaming/moving the project, delete `build/CMakeCache.txt` and re-run cmake
  (it hardcodes the source path).
- Multithreaded run writes per-thread `radical_output_tN.root` then merges into
  `radical_output.root`; empty thread files are auto-deleted.
- **MERGE GOTCHA (important):** the G4 MT master merge intermittently fails,
  leaving ~1 event in the output (`ECombined`/`TotalLYSO` Integral ≈ 1 instead of
  ≈ #events; log shows `delete empty file ... has failed`). Two triggers:
  (1) a stale `./radical` or `root` process holding the file open, and
  (2) **overwriting an existing `radical_output.root`**. ALWAYS before a run:
  `ps aux | grep -E '/radical|root' | grep -v grep` + `kill -9` leftovers, AND
  `rm -f radical_output*.root`. Then VALIDATE via `Integral()` (not GetEntries)
  and retry if it dropped events. This is baked into the scan workflow.
- `./radical` with no arg = viewer; with a `.mac` arg = batch. vis.mac has no beamOn.
- Disk has been tight before — watch free space before large runs.

## Analysis scripts

| Script | Command | Output |
|--------|---------|--------|
| `analysis/plot_testbeam.C` | `root -l -b -q 'analysis/plot_testbeam.C("build/radical_output.root",120)'` | 4 PNGs in build/plots/ |
| `analysis/scan_resolution.C` | `root -l -b -q analysis/scan_resolution.C` | build/scan/resolution_curves.root + PNGs |
| `analysis/compare_data.C` | `root -l -b -q analysis/compare_data.C` | prints σ_t table (sim vs data) |
| `analysis/compare_graphs.C` | `root -l -b -q analysis/compare_graphs.C` | build/plots/datacomp/sigma_vs_E.png |
| `analysis/fix_titles.C` | one-time patch | replaces "front/back" → "upstream/downstream" in ROOT files |

Plots already produced (build/plots/datacomp/):
- `sigma_vs_E.png` — σ_t vs E_beam: DATA (black), SIM 5% CFD (red), SIM first-photon (blue)
- `data_deltaT_25GeV.png` / `data_deltaT_150GeV.png` — measured ΔT distributions
- `data_waveform_example.png` — raw DRS4 single-event waveform (ch0 blue, ch1 red)

## Open questions / future work
1. **DRS4 inter-cell calibration**: applying per-cell timing correction to raw data
   should move σ_t from ~500 ps toward the sim's ~70–145 ps prediction.
2. **Tune SPR τ_f**: sim emulated pulse FWHM ~17–19 ns vs data ~8.3 ns — adjust τ_F
   from 3ns toward ~1.5ns; τ_R from 1ns stays, or tune both to match data FWHM first.
3. **Add DRS4 noise floor** to sim emulation (~0.5–2 mV RMS, but much smaller than
   the inter-cell calibration effect).
4. **Optical yield reduction** (×10) to speed up scans while keeping photostatistics regime.

## Session history (high level)
Built full geometry → GitHub upload + auto-push hook → enriched histograms
(shower shape, timing, capillary, lateral) → DSB1 swapped to LuAG:Ce →
6 depth-sliced lateral profiles → vis switched to wireframe for inspection →
WLS shortened to 6mm → energy cap fills its hole → **WLS retargeted to measured
shower max (43mm / layer ~10.5)** after histogram showed peak is not at center →
energy scan (5–120 GeV, 1500 evt/point) → CERN test-beam data comparison (4 runs,
5% CFD convention) → waveform emulation (H1[22] DeltaT_CFD) → sim/data gap
identified as DRS4 calibration artifact → graphs generated.
