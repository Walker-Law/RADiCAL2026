# RADiCALsimSIMPLE — a RADiCAL timing simulation

A deliberately small model of the RADiCAL module, built to be **modified by hand**.
 It keeps the real physics (LYSO/W shower + 4-corner WLS
timing fibers) but throws away every layer of accreted complexity:
**no electronics, no beam-line, no waveform/CFD/DRS4, no SiPM saturation or
jitter.** The whole thing is ~6 short source files.

The timing observable is the **raw first-photon arrival-time difference** between
the two ends of a fiber — nothing else.

---

## What changed today (2026-07-28)

1. **Added the CERN H2 test-beam line**: two coincidence trigger counters, an
   MCP-PMT timing reference, and a Pb-glass tail-catcher — each behind its own
   on/off flag (`RADSIMPLE_WITH_TRIGGERS`, `RADSIMPLE_WITH_MCP`,
   `RADSIMPLE_WITH_PBGLASS`, all default ON). Pure truth energy-deposit/time
   quantities — no electronics.
2. **Added an optional central E-type fiber** (`RADSIMPLE_CENTER_ETYPE`,
   default OFF): the tile's central hole is now *always* drilled (matching the
   real, tested module), and can optionally be instrumented with a full-length
   WLS fiber + SiPM pair for future energy-measurement studies.
3. **Added a Gaussian beam spot** (`RADSIMPLE_BEAM_SPOT_MM`, default 2.9 mm) —
   required once the central hole is always present, since a zero-width beam
   at (0,0) can dive straight down it without ever showering.
4. **Renamed the flags for clarity** — old names still work as fallbacks:
   `RADSIMPLE_LYSO_SCALE`→`RADSIMPLE_LIGHT_SCALE`,
   `RADSIMPLE_OPT_MAXSTEP`→`RADSIMPLE_PHOTON_STEP_CAP`,
   `RADSIMPLE_ETYPE`→`RADSIMPLE_CORNER_ETYPE`.
5. **Grew the world volume** (70×70×620 mm) and moved the beam start to
   z = −450 mm to fit the new beamline; extended the output to 8 columns:
   `Elyso, Npe, dT, NpeCenter, tMCP, eTrig1, eTrig2, ePbGlass`.
6. **Fixed a CPU-throttling bug**: a hardcoded `/run/numberOfThreads 64` in the
   shared macros (a leftover perseverence workaround) was silently limiting
   curiosity's 512-core machine to ~12.5% utilization. Thread count now comes
   from a `RADSIMPLE_THREADS` env var read in `radsimple.cc`'s `main()`
   (default = all cores); removed from all three `.mac` files.
7. **Dropped `run.mac` to 5 000 events/energy** (from 10 000) for faster
   iteration — still tight enough (σ_E/E to ~±0.3%, σ_t to ~±1 ps).
8. **Gave the three beamline components distinct colors** in the geometry
   viewer: MCP = light blue, coincidence counters = darker blue, Pb-glass =
   light green (previously all three shared one green color).
9. **Deployed and validated on perseverence** from a bare starting state
   (Geant4 10.5.0 → miniforge + conda `g4` env matching curiosity); fixed a
   worker-thread hang from a missing photon step cap (now defaults to 20 000)
   and an `ENSDFSTATE` data-path naming mismatch between source-built and
   conda-forge Geant4.

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
| tile | 14 × 14 mm, with **five** capillary holes (4 corners + center, papers' Fig. 2) |
| stack | 29 LYSO (1.5 mm) + 28 W (2.5 mm) + 56 Tyvek foils (0.2032 mm) = **124.88 mm** |
| Tyvek | reflective (98%) foils between plates, with holes for the fibers |
| fibers | 4, at (±3.5, ±3.5) mm; quartz guide + **DSB1 15 mm at shower max (40.4 mm deep)** |
| fiber radius | 0.575 mm in a 0.65 mm hole (75 µm air gap → quartz-air TIR guiding) |
| SiPMs | thin Si discs at both stack ends, one per fiber end (8 total) |
| center hole | always drilled (papers: present but "unused in these studies"); instrument it with `RADSIMPLE_CENTER_ETYPE=1` |

Optional components (each on its own flag, see the table below):

| component | z | what it is / what it records |
|---|---|---|
| Trig1, Trig2 | −400, −350 mm | 2×2 cm² plastic coincidence counters (size per 2401 Fig. 11) → `eTrig1`, `eTrig2` (MeV) |
| MCP | −250 mm | fused-silica MCP-PMT window + thin ceramic body; earliest charged-particle time = event t0 → `tMCP` (ns) |
| Pb-glass | +320 mm | 10×10×40 cm tail catcher behind the module → `ePbGlass` (GeV, leakage) |
| central E-type | on axis | full-length WLS fiber in the center hole, own SiPM pair → `NpeCenter` |

No electronics or noise anywhere — every one of those is a pure truth quantity.

Beam: one electron per event along `+z` from z = −450 mm (upstream of the whole
beam line), energy set in the macro (`/gun/energy`), **Gaussian spot σ = 2.9 mm**
(`RADSIMPLE_BEAM_SPOT_MM`; 0 = pencil). The spot is required physics: the tiles
have a real central hole on the beam axis, and a zero-width beam at (0,0) slips
straight down it without showering — the pathology 2303.05580 cuts against.

---

## Files (read them in this order)

| file | what it does |
|------|--------------|
| `radsimple.cc` | main: run manager, physics (FTFP_BERT + optical), viewer/batch |
| `src/DetectorConstruction.cc` | **materials + geometry** — the physics inputs |
| `src/PrimaryGeneratorAction.cc` | the electron beam |
| `src/SteppingAction.cc` | at each step: sum LYSO energy, detect photons at SiPMs |
| `src/StackingAction.cc` | coherent Cherenkov thinning (same factor as the LYSO yield) |
| `src/EventAction.cc` | per event: form dT, fill histograms |
| `src/RunAction.cc` | define histograms + ntuple, write the output file |

Everything you'd want to change (a dimension, a material property, the readout
rule) is in `DetectorConstruction.cc` or `SteppingAction.cc`, both short.

---

## The idiot guide (start here)

**One-time setup, then three commands forever.** Everything below assumes you
start in this folder (`RADiCALsimSIMPLE/`).

**1. Build it** (redo `make` after ANY code edit; redo `cmake` too if you added
a new file or edited a `.mac` — cmake is what copies macros into `build/`):

```bash
mkdir -p build && cd build
source ../setup_env.sh          # Geant4 env. On curiosity: conda activate g4 FIRST
cmake .. -DCMAKE_PREFIX_PATH=$CONDA_PREFIX     # Mac/perseverence: use $(geant4-config --prefix) if no conda
make -j
```

**2. Run it** (from this folder, not `build/`):

```bash
bash run_simple.sh                     # the standard sweep: 5k events x 6 energies (~5 h on a 512-thread cluster, measured)
nohup bash run_simple.sh &             # same, survives SSH drops (clusters)
bash run_simple.sh run_short.mac       # any other macro
```

**No redirect needed** — the log is already at `build/logs/run_simple.log`
(repo-wide convention, see [simulations/README_LOGGING.md](../README_LOGGING.md)).
Watch it with:

```bash
tail -f build/logs/run_simple.log
```

Any flag goes in front: `RADSIMPLE_CENTER_ETYPE=1 bash run_simple.sh`.
Raw `tail -f` is mostly thread-startup noise at first — filter with
`grep "\-\-> Event" build/logs/run_simple.log | tail` for real progress. Each
finished energy writes its `E<N>GeV.root`; the run is done when `E120GeV.root`
exists and `pgrep radsimple` returns nothing.

Driving the binary directly still works and is unchanged — it just doesn't log
itself:

```bash
cd build && ./radsimple                # opens the 3D viewer, fires 2 showers
cd build && ./radsimple run.mac        # you name the log destination yourself
```

**3. Analyze it** (from this folder, not `build/`):

```bash
root -l -b -q analysis/scan.C                     # data in build/
root -l -b -q 'analysis/scan.C("build/short")'    # data pulled from another cluster
```

Prints the σ_t / efficiency / σ_E tables and writes every plot under
`<dir>/plots/`. Single file: `root -l -b -q 'analysis/fit.C("build/E50GeV.root")'`.

**Cluster round-trip:** run on the cluster (its repo copy, `git pull` first),
then from the Mac `bash pull_results.sh` (curiosity → `build/`) or
`bash pull_results.sh perseverence` (→ `build/short/`), then analyze locally.
Plots are NEVER made on the clusters — perseverence has no working ROOT.

**The three classic mistakes** (all made here at least once, ask me how I know):
1. Editing `run.mac` and rerunning without `cmake ..` — the binary executes the
   stale copy in `build/`, silently. Always `cmake ..` after macro edits.
2. Trusting file *timestamps* to decide if a run is fresh — check event counts
   (`fit.C` prints them) or the config lines at the top of the log.
3. Reading `tail -f` thread-startup spam as "stalled" — filter for
   `--> Event` lines instead; only 1-3 pegged threads in `top -H` with the rest
   idle means actually stuck.

`run.mac` sweeps `E = 5, 10, 25, 50, 100, 120 GeV`, 5 000 events each, writing
one file per energy: `E5GeV.root`, `E10GeV.root`, ... `E120GeV.root`. It uses
Geant4's built-in `/analysis/setFileName` command — add or remove a
`/analysis/setFileName` + `/gun/energy` + `/run/beamOn` block to change the
energy list, no C++ needed. (`run_short.mac` is the same sweep at 1 000
events/energy for a quick cross-check run.)

Each file's histograms: `Elyso` (GeV), `Npe` (photons/event), `dT` (ns,
4-corner mean), `dTc` (per corner), plus the `ev` ntuple below.

### What's in the ntuple (one row per event)

Stored rich enough that **any event-level graph is a `TTree::Draw` away — no
rerun needed.** (2026-07-28 extension; files before that date have only the
first 8 columns.)

| column | type | meaning |
|--------|------|---------|
| `Elyso` | double | GeV summed over all 29 LYSO plates (truth dE/dx) |
| `Npe` | double | detected photons, 4 corner fibres, 8 SiPMs |
| `dT` | double | ns, 4-corner mean first-photon t(down)−t(up); **−999 = no timing** |
| `NpeCenter` | double | central E-type light (0 unless `RADSIMPLE_CENTER_ETYPE=1`) |
| `tMCP` | double | MCP particle-arrival t0, ns (−1 = off/missed) |
| `eTrig1`, `eTrig2` | double | trigger-counter deposits, MeV |
| `ePbGlass` | double | Pb-glass tail-catcher deposit, GeV (leakage) |
| `x`, `y` | double | this event's primary position, mm (beam-spot truth) |
| `Ew` | double | GeV summed over all 28 W plates (sampling-fraction studies) |
| `Elayer` | vector[29] | GeV per LYSO layer — longitudinal profile, shower max/COG per event |
| `NpeCorner` | vector[4] | photons per corner — asymmetry, beam-spot starvation |
| `tUp`, `tDn` | vector[4] | first-photon time per corner per end, ns (−999 = none) — rebuild dT any way you like, or reference against `tMCP` |

Example draws, no rerun required:

```cpp
ev->Draw("Elayer[8]");                          // energy in layer 8
ev->Draw("Sum$(Elayer*Iteration$)/Sum$(Elayer)"); // shower COG (layers)
ev->Draw("Npe:sqrt(x*x+y*y)");                  // light vs beam offset
ev->Draw("(tDn[2]-tUp[2])/2 - (tDn[0]-tUp[0])/2"); // corner-corner timing
```

**Not stored by default:** individual photon arrival times. First-photon
timing is fully rebuildable from `tUp`/`tDn`, but a *different* estimator
(Nth-photon, CFD emulation) needs every photon: run with
`RADSIMPLE_STORE_PHOTON_TIMES=1` to add `phT` (every detected photon's time)
and `phId` (its channel, corner + 4·isDown). That's the one payload that
meaningfully grows files — see sizes below.

### How much data is stored

Per 5000-event file: **~2.5 MB** default (≈0.5 KB/event; the ntuple stores ~52
doubles per event and physics doubles barely compress). A 6-energy sweep is
**~15 MB**; even a hundred sweeps is no threat to an SSD. With
`RADSIMPLE_STORE_PHOTON_TIMES=1` a file grows with Npe — measured ~6 KB/event
at 10 GeV, scaling roughly linearly with energy: **~1 GB per 6-energy sweep**.
Fine occasionally, not as an always-on default.

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

## Two different energy resolutions — don't confuse them

`scan.C` reports **both**, because they answer different questions:

| | what it is | compare to |
|---|---|---|
| **`Npe` resolution** | spread of **detected light** — the WLS signal from the ~3 tiles at shower max | **the papers.** This is their observable: they sum 8 low-gain SiPM amplitudes |
| **`Elyso` resolution** | spread of total dE/dx over **all 29 LYSO plates** | **nothing published.** A truth quantity nobody measured |

2401.01747 §5.1.3 is explicit that its energy number "does not represent the
energy resolution that would result from … all 29 LYSO:Ce layers." So the
`Elyso` number is the stack's *intrinsic sampling* resolution — a clean
characterization of this geometry, but **not** comparable to either paper, nor
to the ~10 %/√E design goal (which is for a 3×3 array, a different detector).

## Results (2026-07-25 — 10 000 events/energy, light scale 1e-2)

> **These predate the 2026-07-28 geometry update** (beam line + always-drilled
> central hole + 2.9 mm beam spot — previously a pencil beam and no hole).
> Trends and conclusions stand; exact numbers will shift slightly on rerun.

| E (GeV) | σ_t (ps) | eff | σ/E, `Npe` (measured) | σ/E, `Elyso` (truth) |
|---|---|---|---|---|
| 5   | 38.7 ± 0.7 | 100 % | 20.34 ± 0.25 | 5.09 ± 0.06 |
| 10  | 29.9 ± 0.4 | 100 % | 14.39 ± 0.17 | 3.66 ± 0.04 |
| 25  | 21.0 ± 0.2 | 100 % | 10.51 ± 0.12 | 2.30 ± 0.03 |
| 50  | 17.2 ± 0.2 | 100 % | **9.63 ± 0.12** | 1.65 ± 0.02 |
| 100 | 15.2 ± 0.2 | 100 % | 10.43 ± 0.14 | 1.23 ± 0.01 |
| 120 | 14.6 ± 0.2 | 100 % | 10.88 ± 0.15 | 1.12 ± 0.01 |

- **Measured (`Npe`): σ_E/E = 40.5 %/√E ⊕ 7.31 %** (fit ≤ 50 GeV) vs the paper's
  **52.04 %/√E ⊕ 31.62 %/E ⊕ 9.31 %**. Same order, constant term within ~2 %.
  (Their `b/E` term is electronic noise, which this sim does not model at all.)
- **The resolution stops improving above ~50 GeV and gets 13 % worse by 120 GeV.**
  This is real, not noise (9.63 ± 0.12 → 10.88 ± 0.15). The DSB1 window is a
  **fixed 15 mm at 40.4 mm depth**, but shower max walks deeper with energy
  (2401.01747 Fig. 7: layers 8–10 at 25 GeV → 11–13 at 125 GeV), so the sampled
  fraction degrades. The paper flags exactly this limitation ("adequate although
  not optimized … will be corrected in future work"). Because the papers' fit
  form is monotonically decreasing it *cannot* describe this, which is why the
  fit above is restricted to the monotonic region and the turn-up is quoted
  separately.
- **Truth (`Elyso`): 11.4 %/√E ⊕ 0.44 %** — intrinsic sampling resolution of the
  stack; independent of photon thinning. See the caveat table above.
- **Efficiency** = fraction of events yielding a ΔT at all. `EventAction` only
  fills `dT` when some corner sees light at **both** ends, so dim events vanish
  silently and the survivors are the *brighter* ones — a σ_t quoted at low
  efficiency is biased optimistic. 100 % everywhere here, so no bias; `scan.C`
  prints it and flags any energy below 99 %.
- **σ_t = 84.4/√E ⊕ 12.5 ps** at 1e-2 thinned light — *superseded.* This run
  predates the coherent-thinning fix: Cherenkov was generated at its **full
  physical rate** while scintillation was 100× thinned, so the first-photon
  race was won by prompt Cherenkov rather than the 36 ns-gated WLS light the
  real device times on (the inverted-ratio trap documented in RADiCALsimDSB).
  **Fixed 2026-07-25**: `StackingAction.cc` now thins Cherenkov by the same
  `RADSIMPLE_LYSO_SCALE`, so ALL light carries one coherent factor and the mix
  is physical at any thinning. The timing row of the table above is the
  pre-fix (Cherenkov-dominated) number — kept for the record; the 100k-event
  rerun with the fix supersedes it. σ_E/E is unaffected (no photons involved).

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

## Flags (environment variables — set before the command, e.g. `FLAG=0 ./radsimple run.mac`)

**Physics / performance:**

| var | default | meaning |
|-----|---------|---------|
| `RADSIMPLE_OPTICAL` | 1 | 0 = skip optical photons entirely (fast, energy-only) |
| `RADSIMPLE_LIGHT_SCALE` | 1e-2 | **coherent photon thinning of ALL light** — LYSO scintillation at the source and Cherenkov at stacking, one factor, so the light mix stays physical. (old name `RADSIMPLE_LYSO_SCALE` still accepted) |
| `RADSIMPLE_PDE` | 0.36 | SiPM detection efficiency |
| `RADSIMPLE_BEAM_SPOT_MM` | 2.9 | Gaussian beam-spot σ; 0 = pencil (warning: a pencil beam dives down the central hole) |
| `RADSIMPLE_PHOTON_STEP_CAP` | 20000 | kill any photon after this many steps (anti-hang safety valve; costs no measurable light). (old name `RADSIMPLE_OPT_MAXSTEP` accepted) |
| `RADSIMPLE_THREADS` | all cores | worker threads. Lower it only if a host's ntuple merge misbehaves at high counts (symptom: ~200-byte per-thread files, empty merged output). **Never put `/run/numberOfThreads` in a macro** — it runs after this and silently throttles the machine |
| `RADSIMPLE_STORE_PHOTON_TIMES` | **0** | 1 = also store EVERY detected photon's arrival time (`phT`/`phId` columns) — needed only to re-derive timing with a different estimator offline. ~1 GB per sweep instead of ~15 MB |

**Optional components (1 = present):**

| var | default | component |
|-----|---------|-----------|
| `RADSIMPLE_WITH_TRIGGERS` | 1 | the two 2×2 cm² coincidence counters |
| `RADSIMPLE_WITH_MCP` | 1 | the MCP timing reference (its window is also ~real preshower material) |
| `RADSIMPLE_WITH_PBGLASS` | 1 | the Pb-glass tail catcher |
| `RADSIMPLE_CENTER_ETYPE` | **0** | full-length WLS fiber + SiPMs in the central hole. Default OFF because the papers' tested module left that hole empty — turn it on to study the future energy option |
| `RADSIMPLE_CORNER_ETYPE` | 0 | turn the 4 **corner** fibers into full-length E-type (energy config; replaces the 15 mm timing window). (old name `RADSIMPLE_ETYPE` accepted) |

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
- **Test-beam line electronics** — the triggers, MCP, and Pb-glass are all
  modeled geometrically (`RADSIMPLE_WITH_TRIGGERS`/`_WITH_MCP`/`_WITH_PBGLASS`)
  and record pure truth energy/time, but none of them have gain, noise, or
  a coincidence/trigger logic layer.
- **DSB1 self-scintillation** — off, so the light is 100% WLS.
- **Side/end Tyvek wrap** — only inter-plate foils; sideways light is lost
  (lower efficiency, doesn't change the timing physics). Add wrap volumes in
  `Construct()`.
- **183 mm rod extensions** — SiPMs sit right at the stack ends.

None of these change the *shape* of the timing physics; they change absolute
light level and add real-detector degradation. Start here to understand the
system, then layer complexity back in one piece at a time.
