# RADiCALsimSIMPLE — a RADiCAL timing simulation

A deliberately small model of the RADiCAL module, built to be **modified by hand**.
 It keeps the real physics (LYSO/W shower + 4-corner WLS
timing fibres) but throws away every layer of accreted complexity:
**no electronics, no beam-line, no waveform/CFD/DRS4, no SiPM saturation or
jitter.** The whole thing is ~6 short source files.

The timing observable is the **raw first-photon arrival-time difference** between
the two ends of a fibre — nothing else.

---

## The light chain (the whole point)

```
 e- ─► LYSO/W stack ─► LYSO scintillates (420 nm blue)
                          │
                          ▼  a blue photon reaches a corner fibre
                       quartz guide ──► DSB1 (15 mm at shower max)
                          │                 │  absorbs blue, re-emits GREEN (495 nm) = "OpWLS"
                          ▼                 ▼
                     guided by TIR ──► SiPM (PD) at each end ──► record FIRST photon time
```

1. the shower deposits energy in the LYSO plates,
2. LYSO scintillates → 420 nm photons,
3. a photon reaching a corner fibre enters the quartz; at the 15 mm **DSB1**
   section (placed at shower max) it is absorbed and re-emitted at 495 nm
   (Geant4 process `OpWLS`),
4. the green photon is guided by total internal reflection to the SiPM at each end,
5. we keep the **earliest** arrival time at each end and form
   `dT = t(down) − t(up)`, averaged over the 4 corners.

The single-ended timing resolution is `σ_t = σ(dT) / 2` (the ÷2 because dT is a
symmetric down−up difference about the shower-max source).

> **Design choice:** the DSB1 fibre is a **pure wavelength shifter** (no self-
> scintillation). So the fibre light is unambiguously the LYSO→WLS chain — this
> also sidesteps the self-scint/WLS composition ambiguity that complicated the
> full DSB sim.

---

## Geometry (all in `DetectorConstruction.cc`, constants in the header)

| part | value |
|------|-------|
| tile | 14 × 14 mm |
| stack | 29 LYSO (1.5 mm) + 28 W (2.5 mm) + 56 Tyvek foils (0.2032 mm) = **124.88 mm** |
| Tyvek | reflective (98%) foils between plates, with holes for the fibres |
| fibres | 4, at (±3.5, ±3.5) mm; quartz guide + **DSB1 15 mm at shower max (40.4 mm deep)** |
| fibre radius | 0.575 mm in a 0.65 mm hole (75 µm air gap → quartz-air TIR guiding) |
| SiPMs | thin Si discs at both stack ends, one per fibre end (8 total) |

Beam: one electron per event, `+z`, set the energy in the macro with `/gun/energy`.

---

## Files (read them in this order)

| file | what it does |
|------|--------------|
| `radsimple.cc` | main: run manager, physics (FTFP_BERT + optical), viewer/batch |
| `src/DetectorConstruction.cc` | **materials + geometry** — the physics inputs |
| `src/PrimaryGeneratorAction.cc` | the electron beam |
| `src/SteppingAction.cc` | at each step: sum LYSO energy, detect photons at SiPMs |
| `src/EventAction.cc` | per event: form dT, fill histograms |
| `src/RunAction.cc` | define histograms + ntuple, write the output file |

Everything you'd want to change (a dimension, a material property, the readout
rule) is in `DetectorConstruction.cc` or `SteppingAction.cc`, both short.

---

## Build & run

```bash
cd RADiCALsimSIMPLE && mkdir -p build && cd build
source ../setup_env.sh                       # Geant4 environment
cmake .. -DCMAKE_PREFIX_PATH=$CONDA_PREFIX && make -j

./radsimple                                  # OpenGL viewer (fires 2 showers)
./radsimple run.mac                          # batch energy sweep (see below)
```

`run.mac` sweeps `E = 5, 10, 25, 50, 100, 120 GeV`, 500 events each, writing one
file per energy: `E5GeV.root`, `E10GeV.root`, ... `E120GeV.root`. It uses
Geant4's built-in `/analysis/setFileName` command — add or remove a
`/analysis/setFileName` + `/gun/energy` + `/run/beamOn` block to change the
energy list, no C++ needed.

Each file's histograms: `Elyso` (GeV), `Npe` (photons/event), `dT` (ns,
4-corner mean), `dTc` (per corner), plus an `ev` ntuple `(Elyso, Npe, dT)`.

Analyze one file: `root -l -b -q 'analysis/fit.C("build/E50GeV.root")'`
→ prints `σ_t = σ(dT)/2` and `σ/E`. Analyze the whole sweep:
`for f in build/E*GeV.root; do root -l -b -q "analysis/fit.C(\"$f\")"; done`

---

## Knobs (environment variables)

| var | default | meaning |
|-----|---------|---------|
| `RADSIMPLE_OPTICAL` | 1 | 0 = skip optical photons (fast, energy only) |
| `RADSIMPLE_LYSO_SCALE` | 1e-2 | **photon thinning** — tracks this fraction of LYSO light |
| `RADSIMPLE_PDE` | 0.36 | SiPM detection efficiency |

**Why thinning exists:** full LYSO light is ~5×10⁸ photons per 120 GeV event —
untrackable. We track a fraction `f = RADSIMPLE_LYSO_SCALE`. The photon-counting
part of σ_t scales as `√f`, so a thinned run extrapolates to true light by that
factor (this is the same "scale ladder" idea as the full sim). At `f = 1e-2` we
run ~100× photon-starved, so σ_t is ~10× worse than the true-light value.

---

## What is deliberately left out (and where you'd add it)

- **All electronics** — no gain, saturation, noise, shaping, digitization, CFD,
  or DRS4. Timing is pure first-photon. *(The full model of these lives in
  `../RADiCALsimDSB`.)*
- **SiPM SPTR jitter** — the sensor is ideal. Add a Gaussian in `RecordPhoton`.
- **Test-beam line** — no triggers, MCP, or Pb-glass tail-catcher.
- **DSB1 self-scintillation** — off, so the light is 100% WLS.
- **Side/end Tyvek wrap** — only inter-plate foils; sideways light is lost
  (lower efficiency, doesn't change the timing physics). Add wrap volumes in
  `Construct()`.
- **183 mm rod extensions** — SiPMs sit right at the stack ends.

None of these change the *shape* of the timing physics; they change absolute
light level and add real-detector degradation. Start here to understand the
system, then layer complexity back in one piece at a time.
