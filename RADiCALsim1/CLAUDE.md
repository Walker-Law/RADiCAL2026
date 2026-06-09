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
- One LYSO+W period = 4.02 mm. **Center of LYSO layer L = L*4.02 + 0.75 mm from front face.**
- Housing: Delrin shell, 18mm outer / 14mm inner cavity, `housingHalfZ`=65mm.
- World: ±25mm transverse, ±200mm z. **Module only spans ±9mm — beam offsets >~7mm miss it.**

### Capillaries (5 holes drilled via G4SubtractionSolid)
- **Center (energy):** `centerHoleR`=0.45mm. EJ309 bore (r=0.20mm) + quartz tube.
  `eCap_outR = centerHoleR` so the tube fully fills the hole (no air gap).
- **4 corners (timing):** `cornerHoleR`=0.65mm. Quartz front rod + quartz tube wall +
  **LuAG:Ce WLS fiber** (r=0.45mm) at shower max + quartz back rod.
- WLS segmentation auto-centers on shower max: `frontLen = showerMaxDepth - wlsLen/2`.
  Current: `showerMaxDepth`=43.0mm (measured peak ≈ layer 10–11, NOT geometric middle),
  `wlsLen`=6.0mm (covers 40–46mm), `frontLen`=40mm, `backLen`=68.06mm, `z_wls`=−14.03mm.

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

20 TH1D (H1[0–19]) + 15 TH2D (H2[0–14]) — last entries are the test-beam line
detectors (see CERN test-beam line section below). Core module histograms:
- H1: ShowerProfile, TotalLYSO, TotalW, SamplingFraction, CenterCapEnergy,
  CornerWLSEnergy, DeltaT, ShowerMaxLayer, ShowerCOG, ShowerRMS,
  CenterCapFraction, CornerWLSPerCorner, ZResidual, TotalCornerWLS.
- H2[0–6]: DeltaT_vs_TrueZ, ZReco_vs_ZTrue, LateralProfile (integrated XY),
  CenterCapVsLYSO, CornerWLSVsLYSO, DeltaTVsLYSO, ShowerMaxVsLYSO.
- **H2[7–12]: LateralProfile_Slice0..5** — XY maps at 6 depth slices
  (LYSO layers 0–4, 5–9, 10–14, 15–19, 20–24, 25–28). Routed in EventAction via
  `depthSliceH2(layer)` lambda. Each 70×70 bins over ±7mm.

Timing recon uses the ACTUAL recorded WLS hit z-positions (not a hardcoded
constant), so moving the WLS section flows through automatically.

Inspect output:
```bash
root -l -b build/radical_output.root -e 'gDirectory->ls(); gApplication->Terminate();'
```

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
- `./radical` with no arg = viewer; with a `.mac` arg = batch. vis.mac has no beamOn.
- Disk has been tight before — watch free space before large runs.

## Session history (high level)
Built full geometry → GitHub upload + auto-push hook → enriched histograms
(shower shape, timing, capillary, lateral) → DSB1 swapped to LuAG:Ce →
6 depth-sliced lateral profiles → vis switched to wireframe for inspection →
WLS shortened to 6mm → energy cap fills its hole → **WLS retargeted to measured
shower max (43mm / layer ~10.5)** after histogram showed peak is not at center.
