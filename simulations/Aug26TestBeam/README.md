# Aug26TestBeam — simulating the August 2026 CERN T10 beam test

A Geant4 model of exactly what was in the beam at CERN's T10 line in the last
week of August 2026: the RADiCAL module read out from the **upstream end
only** (the downstream sensors and their readout card were removed), with three
different filament materials in the corner capillaries, at electron beam
energies of 1 to 11 GeV. It is a copy of `RADiCALsimSIMPLE` with the readout,
materials, and beam changed to match; everything not mentioned below is the
SIMPLE model unchanged.

Source of truth for what was run: the run manifest spreadsheet
`RADiCAL - August 2026 - CERN TEST BEAM.xlsx` (Run Manifest, Channel and
Capillary Configuration, Beam Configurations sheets). Decisions taken with the
beam-test lead on 2026-09-11 are recorded inline below.

---

## What was in the beam, and what this simulates

| | in the beam test | in this simulation |
|---|---|---|
| module | the 14 x 14 mm, 29 LYSO + 28 tungsten plate module | same, unchanged from SIMPLE |
| corner capillaries | all four T-type: 15 mm filament at the shower-max depth, quartz rod either side | same; the window was **not** moved for the low-energy beam (see "Shower max" below) |
| centre capillary | drilled, empty | drilled, empty |
| readout | one silicon photomultiplier per corner at the **upstream** end, each with a low-gain and a high-gain channel; **nothing** at the downstream end | one sensor per corner at the upstream end; the downstream fibre face is an open quartz end in air; no electronics, so the two gain channels collapse into one photon record |
| filaments | configuration A: LuAG:Ce (T112, T113, T114, T116); B: DSB1 (T091, T093, T096, T099); C: EJ199 (T131 to T134) | `RADSIMPLE_CAPILLARY=LUAG` or `DSB1`; **EJ199 is refused until its optical properties are known** |
| beam | T10 secondary beam, electron-tagged by two threshold Cherenkov counters; 1, 3, 5 GeV positive (positrons), 7, 9, 11 GeV negative (electrons) | electrons at every energy (shower physics is charge-symmetric); Gaussian spot and momentum spread, see "Beam" |
| trigger | two scintillator counters ScintA, ScintB | two 2 x 2 cm² plastic counters, on by default |
| timing reference | a micro-channel-plate detector was in the line but is **not** used in the analysis | off by default (`RADSIMPLE_WITH_MCP=1` restores the SIMPLE model) |
| tail catcher | none | none (the SIMPLE lead glass is removed) |

Energies actually taken, per the manifest: **LuAG:Ce 1, 3, 5, 7, 9, 11 GeV;
DSB1 1, 3, 5, 7, 9, 11 GeV; EJ199 1, 3, 5, 7, 9 GeV.** The run script uses
exactly those lists.

---

## The T10 beam (from CERN's 2025 characterization, arXiv:2507.02567)

A secondary beam from 24 GeV/c protons on a beryllium target; 0.5 to 11.5
GeV/c; momentum spread selectable from 0.6% to 15% by collimators; about 8.6%
of a radiation length of material along the line before the experimental zone.
Composition matters a lot at these energies:

| momentum | electron fraction, negative beam | positron fraction, positive beam |
|---|---|---|
| 1 GeV/c | 87% | 81% |
| 3 GeV/c | 29% | 18% |
| 5 GeV/c | 11% | 4% |
| 7 to 11 GeV/c | 2 to 3% | below 2% |

The rest is pions. So the two threshold Cherenkov counters (CERN name: XCET)
did the electron selection, and the electron-tagged samples at 7 to 11 GeV are
a few hundred to a thousand events out of the 20 to 45 thousand recorded. The
simulation shoots pure electrons, which corresponds to the tagged sample. No
spot size is published for T10; see "Beam" below.

---

## Materials

**LYSO, quartz, DSB1, Tyvek:** byte-for-byte the verified SIMPLE tables,
including the 2026-08-08 chromatic-dispersion curves. DSB1 is a pure
wavelength shifter (absorbs 400 to 449 nm, re-emits at 495 nm, 3.5 ns).

**LuAG:Ce** (cerium-doped lutetium aluminium garnet, the ceramic filament) is
new here and modelled as **both** a wavelength shifter and a scintillator, with
literature values, every one flagged in `src/DetectorConstruction.cc`:

| property | value used | source / confidence |
|---|---|---|
| composition, density | Lu 61.5%, Al 15.8%, O 22.6%, Ce 0.1%; 6.73 g/cm³ | crystal datasheets (same as the older `RADiCALsimLuAG`) |
| refractive index | Cauchy fit through n(633 nm) = 1.842, n(450 nm) ≈ 1.863; n(530 nm) = 1.851 | literature; the dispersion slope is approximate |
| absorption for shifting | two Gaussian cerium bands, 450 nm (±20 nm) and 345 nm; absorption length 0.5 mm at 450 nm, ~1.5 mm at LYSO's 420 nm, transparent above ~500 nm | literature band positions; the peak strength is a 0.1 to 0.2% cerium estimate |
| emission | Gaussian at 530 nm, 40 nm wide, used for both shifted light and self-scintillation | literature: 520 to 540 nm peak |
| decay time | 60 ns for both processes | literature 55 to 70 ns. **Seventeen times slower than DSB1** — expected to dominate the timing difference |
| scintillation yield | 25 000 photons per MeV deposited in the filament | literature 20 000 to 26 000 |
| shift quantum efficiency | 0.7 | literature 0.6 to 0.7 for ceramic material. DSB1 keeps Geant4's default of 1.0, as in SIMPLE — a stated asymmetry |

**EJ199** is a wavelength shifter whose composition, emission, absorption,
decay time, and refractive index are not yet known. `RADSIMPLE_CAPILLARY=EJ199`
aborts with a message saying so. Add its table in `DefineMaterials()` when the
data arrives; nothing else needs to change.

---

## Observables

Every detected photon is stored (the "perfect waveform": `phT`, `phId`,
`phOrigin`), and one electronics-free trigger is computed from it, exactly as
in SIMPLE: `t05[k]`, the arrival time of the 5%-of-light photon at corner k.
This is a light-level proxy, not the experiment's estimator: the beam test
times each corner with a leading-edge crossing at 15% of the predicted
high-gain pulse peak on the digitised waveform (its "srCFD"; see "Comparing
with the beam-test data" below, and `analysis/tb26_emulate.C` for the exact
reproduction).

With no downstream sensor and no timing reference in the analysis, the old
"downstream minus upstream" difference does not exist. Two timing observables
replace it:

| column | definition | what it means |
|---|---|---|
| `dTpair` | difference of the two diagonal corner-pair means, `(t05[0]+t05[3])/2 − (t05[1]+t05[2])/2` — the beam test's own reference-free estimator (`radical-t10-2026/macros/DiagDiff.C`) | **realizable** with exactly the channels read out; cancels the event start time and, to first order, a beam-position shift in x and in y; its spread equals the single-corner resolution, and half of it is the intrinsic four-corner-average resolution (the experiment's `sigma_intr`) |
| `tUpMean` | mean of the four `t05[k]`, absolute | what a **perfect** external time reference would see; an upper bound on any reference-based measurement |

Corner index convention: 0 = (+,+), 1 = (+,−), 2 = (−,+), 3 = (−,−), so the
diagonals are 0–3 and 1–2.

Photon origin codes (`phOrigin`, and per-event counts `NpeCher`, `NpeDirect`,
`NpeWLS`, `NpeFil`): 0 Cherenkov; 1 LYSO scintillation that reached the sensor
unshifted; 2 shifted in the filament; 3 the filament's own scintillation
(LuAG:Ce only). This is what separates "DSB1 versus LuAG:Ce" into physics:
how much light each collects, and from which process.

Energy is the four-corner detected-photon sum `Npe` (the measured quantity, as
in the papers) and the truth `Elyso`. Light yield is the mean `Npe`.

---

## Shower max at 1 to 11 GeV

The 15 mm filament window is centred at 40.4 mm depth, the 120 GeV shower-max
position, spanning 33 to 48 mm. For 1 to 11 GeV electrons in this stack the
shower maximum sits at roughly 21 to 33 mm — entirely upstream of the window.
It was confirmed that the window was **not** moved for this beam test, so the
mismatch is real and is part of what these simulations measure. Expect the
light yield and the timing to reflect a filament that samples the falling edge
of the shower, not its peak.

---

## Beam

| parameter | value | status |
|---|---|---|
| particle | electron, every energy | confirmed (charge-symmetric showers) |
| energies | from the manifest, per material | confirmed |
| spot | Gaussian, σ = 2.9 mm in x and y (`RADSIMPLE_BEAM_SPOT_MM`) | **placeholder**: no T10 measurement exists; the beam files say only "focus optimized XBPF +3.0 m" |
| momentum spread | Gaussian, σ = 1% (`RADSIMPLE_BEAM_DP`) | literature: the T10 paper's runs kept the acceptance collimator at ±1%; the RADiCAL setting was not recorded |
| positions of counters and module | the SIMPLE (H2) spacings: counters at −400 and −350 mm | **placeholder**: T10 distances were not recorded |
| material upstream of the module | the two trigger counters only | the Cherenkov-counter windows (~3% of a radiation length) and ~2 m of air are **not** modelled; a percent-level radiative tail on the beam energy is missing |

---

## Build and run

Build once (redo `make` after any code edit):

```bash
cd Aug26TestBeam
mkdir -p build && cd build
source ../setup_env.sh          # on curiosity: conda activate g4 FIRST
cmake .. -DCMAKE_PREFIX_PATH=$CONDA_PREFIX
make -j
```

Run one material at the manifest energies (from `Aug26TestBeam/`, not `build/`):

```bash
bash run_tb26.sh LUAG                 # LuAG:Ce, 1 3 5 7 9 11 GeV, 2000 events each, TRUE light
bash run_tb26.sh DSB1
nohup bash run_tb26.sh LUAG &         # on the cluster: survives an SSH drop, logs itself
tail -f build/logs/run_tb26.log
```

Output goes to `build/rootfiles/<material>/E<N>GeV.root`; the live log is in
`build/logs/run_tb26.log`. The default light fraction is **1.0, true light**:
at 1 to 11 GeV that is affordable on the cluster, and it removes the
extrapolation step that every earlier timing number in this project needed.

Cost, from the measured constants (5.63 core-seconds per event per GeV at
f = 0.01 on curiosity; 0.052 MB per event per GeV per unit f of stored
waveform):

| what | cluster wall time | disk |
|---|---|---|
| one material, 2000 events × {1,3,5,7,9,11} GeV, f = 1.0 | ~22 h | ~3.7 GB |
| same at f = 0.5 | ~11 h | ~1.9 GB |
| smoke test, 20 events at 5 GeV, f = 0.01, on the Mac | ~1 min | negligible |

The script prints its own estimate at launch. `RADSIMPLE_LIGHT_SCALE=0.5`
halves everything if a day per material is too long.

Smoke test (proves the whole chain in a minute, on the Mac):

```bash
RADSIMPLE_LIGHT_SCALE=1e-2 bash run_tb26.sh LUAG 20
```

Pull cluster output back (from the Mac; copies every material folder):

```bash
bash pull_tb26.sh
```

---

## Analyze

```bash
root -l -b -q analysis/tb26.C
```

Prints, per material and energy: fiducial events, light yield, energy
resolution, both timing resolutions, and the light composition; fits the
stochastic-plus-constant form to energy and timing; writes three overlay plots
to `build/plots/` (`tb26_lightyield_vs_E.png`, `tb26_energyres_vs_E.png`,
`tb26_timing_vs_E.png`) and every fitted histogram under `build/plots/fits/`.
Adaptive binning and two-pass core fits throughout; points under 50 fiducial
events are skipped, and timing errors above 30% are flagged.

These are **light-level** numbers. For numbers that can be put next to the
experiment's tables, see the next section.

---

## Comparing with the beam-test data

**Where things are.** The 27 raw waveform files of the campaign (17 GB) and
their DAQ metadata are in `RADiCAL2026/data/2026-08-CERN_T10/` (git-ignored).
The collaboration's analysis repository, `jwwetzel/radical-t10-2026`, is
cloned at `~/Research/radical-t10-2026/` with `data/download/run_N.root`
linked to the raw files, so its macros run unchanged from that directory
(`root -l -b -q 'macros/DiagDiff.C+(1)'` reproduces the published DSB1 table
byte for byte; checked 2026-09-14). Its published results are in
`Output/scan*/EnergyScan*_summary.txt` and `Output/summary/DiagDiff_*.txt`.

**What the experiment records.** Per event, 18 waveforms of 1024 samples at
0.2 ns (CAEN DT5742, two DRS4 groups). Each corner's silicon photomultiplier
is read twice: a fast **high-gain** chain (timing; ~6.5 ns full width at half
maximum, AC-coupled, clips 780 mV above baseline) and a slow **low-gain**
chain (energy; an integrator peaking about 50 ns after the fast pulse). Two
threshold Cherenkov counters tag electrons; a micro-channel plate gives the
time reference.

**Observable map.** What the experiment publishes, and the simulation's
counterpart:

| experiment (radical-t10-2026) | algorithm | simulation counterpart |
|---|---|---|
| response and σ/E (`EnergyScan*.C`) | sum of the four low-gain **peak amplitudes** of tagged, on-module events; Gaussian core fit ±1.7σ, iterated three times | `tb26_emulate.C`: same sum on emulated low-gain waveforms, same fit. (`Npe` from `tb26.C` is the total light, not the peak of an integrated pulse — comparable only in shape, not in value) |
| shower-time σ, "t-MEDIAN" (`EnergyScan*.C`) | per corner: leading-edge crossing at 15% of the low-gain-**predicted** high-gain peak (guards: above 20 mV, below 0.9 × clip wall), linear interpolation; median over ≥ 2 corners, minus the micro-channel-plate time at 20%; width by `tebSigma` (truncated, debiased, Gaussian cross-check). Includes the reference (~110 ps) | `tb26_emulate.C`: same crossing on emulated high-gain waveforms; the reference is perfect, so a 110 ps Gaussian jitter is added to the reported "meanIncl" column and the perfect-reference value is printed beside it |
| `sigma_intr` (`DiagDiff.C`) | all four corners required; `(t_TL+t_BR)/2 − (t_TR+t_BL)/2`, width halved | `dTpair`/2 (light level, `tb26.C`) and the identical waveform-level quantity in `tb26_emulate.C` |
| electron selection | both Cherenkov counters above threshold; electron purity falls above ~7 GeV, 11 GeV excluded from fits | pure electrons by construction — the 9 and 11 GeV data points carry a purity caveat the simulation does not |
| "on-module" | ΣLG above a floor `SMIN` per material and energy (there is no tracker) | the same `SMIN` gate in `tb26_emulate.C`. `tb26.C`'s truth fiducial cut (3.5 mm) is **not** the experiment's selection; the experiment's σ/E is position-smearing dominated by its own account |

**The emulation.**

```bash
root -l -b -q 'analysis/tb26_emulate.C("build/rootfiles", 1.0)'
```

turns the stored photons of every event into the two waveforms per corner and
runs the experiment's functions verbatim (`pulseOf`, `leTime`, the wall-aware
transfer calibration, the 15% crossing with its guards, `tebSigma`, the core
fit, the `SMIN` gate, copied from `EnergyScan.C` and `DiagDiff.C` with their
constants), then prints both tables in the experiment's format. The second
argument is the light scale the files were produced with; thinned files test
the pipeline only. Inputs the simulation does not contain, each declared in
the macro with its source:

| input | value | source |
|---|---|---|
| impulse responses of the two chains | `analysis/response_kernels.txt` | fitted jointly with a free two-component light model to the measured mean pulse shapes of runs 33 (DSB1), 27 (LuAG), 42 (EJ199), 7 GeV; the header of the file has the model |
| gains, ADC-equivalent per photon | calibrated at run time on the DSB1 5 GeV file: ΣLG peak 6769 and transfer slope 2.9 (run 37) | so DSB1's absolute scale is fixed by construction; LuAG's response, all energy dependences and all widths are predictions |
| noise | high gain 6.6 mV, low gain 1.6 mV, white | pre-pulse RMS, run 33 |
| clip wall | 780 mV above baseline | measured 99.5% walls 3143–3197 ADC-equivalent |
| pulse placement | light onset at sample 22, high-gain peak near sample 62 | measured medians 58–66; the baseline window (samples 0–39) is then contaminated by the pulse foot exactly as in the data |
| corner mapping | simulation 0,1,2,3 → caps TL, TR, BL, BR | so the diagonals {0,3},{1,2} are the experiment's (4,7),(5,6) |

Emulated mean pulse shapes are written to `build/plots/emulate/meanshape_*.txt`
in the same format as the measured ones (`build/plots/data_vs_sim/shape_*.txt`),
so the comparison can be redone after any change to the light model.

**First finding (2026-09-14), from the pulse shapes alone.** The measured
7 GeV pulses of all three materials are reproduced only with 25–40% prompt
light (DSB1 39%, LuAG 26%, EJ199 32% in a two-component fit with one shared
electronics model). The simulated light is 4% prompt for DSB1 (99% is
wavelength-shifted LYSO scintillation, with its 40 ns decay) and 2% for LuAG
(half of it the filament's own 60 ns scintillation). Passed through the same
electronics, the simulated pulses carry tails the data does not have
(`build/plots/data_vs_sim/pulse_shapes_data_vs_sim.png`), and the emulated
high-gain-to-low-gain transfer ratio — the experiment's prompt-versus-slow
balance — is about half the measured one for LuAG. The light model, not the
electronics, is what the data is testing; see ROADMAP Discovery 19.

**Still not comparable, and known.** The beam spot is a placeholder and the
experiment has no tracker, so on-module fractions are the only handle; the
electron purity at 9 and 11 GeV is the experiment's problem, not the
simulation's; EJ199 exists in the data (runs 39–44) and not yet in the
simulation; the gains are anchored on one data point.

---

## Knobs

| variable | default | meaning |
|---|---|---|
| `RADSIMPLE_CAPILLARY` | **none — required** | `LUAG` or `DSB1` (`EJ199` refused for now) |
| `RADSIMPLE_LIGHT_SCALE` | 1e-2 in the binary, **1.0 in the run script** | coherent thinning of all light |
| `RADSIMPLE_ENERGIES` | per material, from the manifest | override the energy list |
| `RADSIMPLE_OUT_SUBDIR` | material name | output folder under `build/rootfiles/` |
| `RADSIMPLE_BEAM_SPOT_MM` | 2.9 | Gaussian beam-spot σ; 0 = pencil (do not: it dives down the central hole) |
| `RADSIMPLE_BEAM_DP` | 0.01 | Gaussian relative momentum spread |
| `RADSIMPLE_PDE` | 0.36 | flat sensor detection efficiency — same for 495 nm and 530 nm light, a stated approximation |
| `RADSIMPLE_WITH_TRIGGERS` | 1 | the two scintillator counters |
| `RADSIMPLE_WITH_MCP` | **0** | the micro-channel-plate reference (off: not used in the analysis) |
| `RADSIMPLE_THREADS` | all cores | worker threads |
| `RADSIMPLE_PHOTON_STEP_CAP` | 20000 | anti-hang per-photon step cap |
| `RADSIMPLE_OPTICAL` | 1 | 0 = no light at all (fast energy-only checks) |

---

## Files

| file | what it is |
|---|---|
| `src/DetectorConstruction.cc` | the module, the selectable filament, the upstream-only sensors, the beam line |
| `src/PrimaryGeneratorAction.cc` | the beam: spot and momentum spread |
| `src/SteppingAction.cc` | photon detection at the upstream sensors, origin tagging, energy tallies |
| `src/EventAction.cc` | the 5% quantile per corner, `dTpair`, `tUpMean`, the ntuple row |
| `src/RunAction.cc` | histograms and the ntuple schema |
| `run_tb26.sh` | run one material at the manifest energies (generates its macro) |
| `pull_tb26.sh` | copy cluster output to the Mac, all material folders |
| `analysis/tb26.C` | the three comparisons, overlaid across materials |
| `macros/smoke.mac`, `macros/vis.mac` | a one-minute check; the geometry viewer |

---

## Known gaps, in order of how much they could matter

1. **EJ199 is not simulated.** Configuration C waits on its optical properties.
2. **Shower max is upstream of the filament window** at these energies (see
   above). Real, not a modelling gap — but it means the 120 GeV intuition does
   not transfer.
3. **Beam spot and beamline distances are H2 placeholders.** A measured T10
   spot or the counter positions would replace two guesses.
4. **Sensor efficiency is flat**, so the 495 nm DSB1 light and the 530 nm
   LuAG:Ce light are counted equally; the real sensor curve tilts slightly
   against the longer wavelength. Also the shift quantum efficiency is 0.7 for
   LuAG:Ce and 1.0 for DSB1 — both literature-based, but they enter the light
   yield comparison directly.
5. **Upstream material is incomplete** (no Cherenkov-counter windows, no air
   column): a percent-level low-energy tail on the beam is missing.
6. **LuAG:Ce absorption strength and decay time are literature, not measured**
   for these specific filaments. The timing comparison is sensitive to the
   60 ns figure in particular.

---

## Changelog

### 2026-09-11
- Created from `RADiCALsimSIMPLE`: upstream-only readout (downstream sensors
  removed, downstream fibre end open), run-time filament choice (LuAG:Ce or
  DSB1; EJ199 refused pending data), no lead glass, micro-channel-plate
  reference off by default, beam with Gaussian momentum spread, manifest
  energies 1 to 11 GeV, true-light default for cluster running.
- New observables `dTpair` (realizable, reference-free) and `tUpMean` (ideal
  reference); per-photon origin code replaces the two-way shifted/prompt tag.
- `analysis/tb26.C`: light yield, energy resolution, and timing versus energy,
  overlaid across materials.
- Smoke-tested locally for both materials (240 events at 5 GeV, 1% light):
  pipeline, origin tags, and analysis all work. One bug found and fixed: the
  LuAG:Ce emission table must extend to 800 nm (as DSB1's does), otherwise a
  rare red Cherenkov photon absorbed in the filament has no re-emission energy
  at or below its own and Geant4 aborts with a fatal wavelength-shifter
  exception. The shift absorption is now truly transparent above 500 nm too.
