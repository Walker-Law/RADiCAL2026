# RADiCALsimSIMPLE — a RADiCAL timing simulation

A deliberately small model of the RADiCAL module, built to be **modified by hand**.
 It keeps the real physics (LYSO/W shower + 4-corner WLS
timing fibers) but throws away every layer of accreted complexity:
**no electronics, no beam-line, no waveform/CFD/DRS4, no SiPM saturation or
jitter.** The whole thing is ~6 short source files.

The timing observable is the **raw first-photon arrival-time difference** between
the two ends of a fiber — nothing else.

---

## The light chain (the whole point)

```
 e- ─► LYSO/W stack ─► LYSO scintillates (420 nm blue)
                          │
                          ▼  a blue photon reaches a corner fiber
                         quartz guide ──► DSB1 (15 mm at shower max)
                          │                 │  absorbs blue, re-emits GREEN (495 nm) = "OpWLS"
                          ▼                 ▼
                     guided by TIR ──► SiPM (PD) at each end ──► record FIRST photon time
```

1. the shower deposits energy in the LYSO plates,
2. LYSO scintillates → 420 nm photons,
3. a photon reaching a corner fiber enters the quartz; at the 15 mm **DSB1**
   section (placed at shower max) it is absorbed and re-emitted at 495 nm
   (Geant4 process `OpWLS`),
4. the green photon is guided by total internal reflection to the SiPM at each end,
5. we keep the **earliest** arrival time at each end and form
   `dT = t(down) − t(up)`, averaged over the 4 corners.

The single-ended timing resolution is `σ_t = σ(dT) / 2` (the ÷2 because dT is a
symmetric down−up difference about the shower-max source).

> **Design choice:** the DSB1 fiber is a **pure wavelength shifter** (no self-
> scintillation). So the fiber light is unambiguously the LYSO→WLS chain — this
> also sidesteps the self-scint/WLS composition ambiguity that complicated the
> full DSB sim.

---

## Geometry (all in `DetectorConstruction.cc`, constants in the header)

| part | value |
|------|-------|
| tile | 14 × 14 mm |
| stack | 29 LYSO (1.5 mm) + 28 W (2.5 mm) + 56 Tyvek foils (0.2032 mm) = **124.88 mm** |
| Tyvek | reflective (98%) foils between plates, with holes for the fibers |
| fibers | 4, at (±3.5, ±3.5) mm; quartz guide + **DSB1 15 mm at shower max (40.4 mm deep)** |
| fiber radius | 0.575 mm in a 0.65 mm hole (75 µm air gap → quartz-air TIR guiding) |
| SiPMs | thin Si discs at both stack ends, one per fiber end (8 total) |

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
→ prints `σ_t = σ(dT)/2` and `σ/E`.

Analyze the whole sweep: `root -l -b -q analysis/scan.C` → `build/plots/`:

| plot | what it shows |
|------|---------------|
| `sigma_t_vs_E.png` | timing resolution σ_t vs E, fitted to `a/√E ⊕ b` |
| `sigma_E_vs_E.png` | energy resolution σ_E/E vs E (%), fitted to `a/√E ⊕ b` |
| `fits/dT_E<N>GeV.png` | the ΔT distribution **at each energy** with its Gaussian-core fit — the histogram each σ_t point came from |
| `fits/Elyso_E<N>GeV.png` | the LYSO-energy spread **at each energy** with its fit — the histogram each σ_E/E point came from |

Every point is a two-pass Gaussian **core** fit (full-range fit, then refit
within µ ± 2σ) so leakage tails don't inflate the widths. σ_E/E is width ÷ mean
of the energy histogram **at one fixed beam energy** — resolution, not linearity.
Note σ_t depends on the photon thinning (`RADSIMPLE_LYSO_SCALE`); σ_E/E does not
(it is pure dE/dx, no optical photons involved).

(For the energy fits, `scan.C` rebuilds each per-energy histogram from the
unbinned `ev` ntuple rather than using the stored `Elyso` H1 — the H1's fixed
100 MeV bins leave only 2–3 bins across the 5 GeV peak and quantize the fit.
Found 2026-07-25 when 10k-event statistics exposed a χ²/ndf ≈ 26 energy curve;
the rebuild dropped it to ≈ 0.6 and moved the 5 GeV point 6.6 % → 5.1 %.)

---

## Results (2026-07-25 — 10 000 events/energy, `RADSIMPLE_LYSO_SCALE=1e-2`)

| E (GeV) | σ_t (ps) | σ_E/E (%) |
|---|---|---|
| 5   | 38.7 ± 0.7 | 5.09 ± 0.06 |
| 10  | 29.9 ± 0.4 | 3.66 ± 0.04 |
| 25  | 21.0 ± 0.2 | 2.30 ± 0.03 |
| 50  | 17.2 ± 0.2 | 1.65 ± 0.02 |
| 100 | 15.2 ± 0.2 | 1.23 ± 0.01 |
| 120 | 14.6 ± 0.2 | 1.12 ± 0.01 |

Fits (both χ²/ndf ≈ 1, every point on the curve):

- **σ_E/E = 11.4 %/√E ⊕ 0.44 %** — *solid.* Pure dE/dx sampling, independent of
  photon thinning, and close to the RADiCAL design target of ~10 %/√E. This is
  the honest sampling resolution of the LYSO/W stack itself.
- **σ_t = 84.4/√E ⊕ 12.5 ps** at 1e-2 thinned light — *fits beautifully but
  carries an open composition caveat:* Cherenkov light (LYSO + quartz rods) is
  generated at its **full physical rate** here while scintillation is thinned
  100×, so the race for "first photon" is likely won by **prompt Cherenkov**
  rather than the 36 ns-gated WLS light the real device times on — the exact
  inverted-ratio trap documented in RADiCALsimDSB. Until the composition is
  fixed (thin Cherenkov coherently, as DSB's `StackingAction` does) or the
  first-photon origin is tagged and checked, do **not** √f-extrapolate this
  number to true light or compare it to the paper's 256 ps/√E.

---

## Running on the cluster

Build and run `run.mac` on curiosity exactly as above (same commands, just
over SSH). To pull the results back to this Mac without hand-writing an rsync
command each time:

```bash
cd RADiCALsimSIMPLE
bash pull_results.sh
```

This syncs `build/E*GeV.root` and `build/plots/` from the cluster copy of this
repo into the matching local paths, so `root -l -b -q analysis/scan.C` (or
`fit.C` on any one file) works locally right after. Host/port/remote path are
hardcoded at the top of the script — edit them there if the cluster changes.

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
