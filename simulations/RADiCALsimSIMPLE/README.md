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

**Two different populations of light reach the SiPMs, and telling them apart is
the single most important thing to understand about this simulation.**

```
 e- ─► LYSO/W stack ─► LYSO scintillates (420 nm blue)          ┐
                          │                                      │  SLOW path
                          ▼  a blue photon reaches a corner fiber│  = 99% of light
                         quartz guide ──► DSB1 (15 mm)           │  arrives ~12 ns
                          │                 │ absorbs blue,      │
                          │                 │ re-emits GREEN     │
                          ▼                 ▼ (495 nm) = "OpWLS" ┘
                     guided by TIR ──► SiPM at each end
                          ▲
 shower particle ─────────┘   Cherenkov made IN the quartz/DSB1  ┐  FAST path
 crossing the fiber            — no absorption/re-emission step   │  = 1% of light
                                                                  ┘  arrives ~2 ns
```

**The slow path (99%, the real signal).** Shower energy → LYSO scintillates at
420 nm → a blue photon reaches a corner fiber → the 15 mm **DSB1** section at
shower max absorbs it and re-emits green at 495 nm (Geant4 process `OpWLS`) →
guided by total internal reflection to a SiPM at each end. It is *slow* because
it inherits LYSO's 36 ns emission time before DSB1's own 3.5 ns re-emission.

**The fast path (1%, a trap).** A charged shower particle crossing the quartz
or DSB1 makes **Cherenkov light directly in the fiber** — no absorption/
re-emission, so it is essentially prompt. Measured: prompt ⟨t⟩ = 2.0 ns vs
WLS ⟨t⟩ = 12.3 ns — a **~10 ns head start.**

### Why the naive estimator breaks — and why we don't use it anymore

An early version of this sim timed on the **earliest** photon at each end.
That is a **minimum**, and a minimum is dominated by whichever population
happens to arrive first — even a rare one. At `RADSIMPLE_LIGHT_SCALE=1e-2`
only ~0.9 prompt photons reach each SiPM per event: a Poisson coin flip.
Corners that catch one read ~3.7 ns early, corners that miss read late, and
the 4-corner mean of that two-valued quantity produced **5 discrete spikes,
0.93 ns apart** — a comb, not a peak. A Gaussian fit across it returned
confident nonsense (43 ps at one energy, 867 ps at the next). This was a
**thinning artifact**, not detector physics — at full light every SiPM
catches ~90 prompt photons and the comb would vanish — but it also raised an
unresolved question: does a *first-WLS-photon* estimator even correspond to
anything a real device measures?

### The architecture (2026-08-02): stop guessing, store everything, replay the real trigger

**This simulation is a light recorder, not a timing algorithm.** Every
detected photon's arrival time, which SiPM face it hit, and which of the two
populations made it — the complete record, the last thing that exists before
any electronics touches the signal — is stored in the ntuple, always, no flag
required (`phT`, `phId`, `phWls`). That record is the **perfect waveform**.

From it, the sim computes exactly one trigger, chosen to match the real
experiment rather than to be numerically convenient: an **electronics-free 5%
CFD**. With no amplifier there is no analog pulse to take "5% of peak" on, so
the light-only equivalent is a **quantile**: `t05` = the arrival time of the
5%-of-light point at that SiPM (the ⌈0.05·N⌉-th photon, N = photons detected
on that face that event). `dTcfd = t05(down) − t05(up)`, averaged over the 4
corners that have light at both ends; `σ_t = σ(dTcfd)/2`.

A 5%-quantile rides on ~N/20 photons rather than 1, so it is far more
mean-like than a single minimum — it does not chase a rare early population,
and it is unimodal at any light level without needing to know in advance
which population is "real". It is also the direct light-level analog of the
test-beam analysis's own 5% CFD convention, so `dTcfd` is comparable to real
data in a way a first-photon time never was.

**Every other estimator is now an offline analysis choice, not a rerun.**
Want first-WLS-photon back, for comparison? Filter `phWls==1`, take the min
per corner-end from `phT`. Want a different CFD fraction, or SPTR jitter
applied before triggering? Same data, different offline cut. The sim's job
stopped being "pick the right estimator" and became "record enough that the
estimator question never needs a rerun again."

> **Open question this does NOT resolve:** whether the prompt-Cherenkov
> precursor crosses the 5% threshold itself. At true light a 120 GeV event
> puts ~890 prompt photons on a SiPM alongside ~110,600 WLS ones; because the
> prompt population is ~6× narrower in time, its *peak rate* is roughly 2–5%
> of the WLS peak — close enough to the threshold that it can't be waved away.
> This is exactly what the stored waveform is FOR: build the real pulse shape
> from `phT`/`phWls` and check directly, rather than assume.

> **Design choice, unchanged:** the DSB1 fiber is a **pure wavelength shifter**
> (no self-scintillation), so the slow-path light is unambiguously the
> LYSO→WLS chain. The prompt Cherenkov above is a separate population, born in
> the fiber material rather than emitted by it.

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
| `src/EventAction.cc` | per event: store every photon (the waveform), compute the 5% CFD, fill histograms |
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
root -l -b -q 'analysis/scan.C("build/archive_perseverence")'    # data pulled from another cluster
```

Prints the σ_t / efficiency / σ_E tables and writes every plot under
`<dir>/plots/`. Single file: `root -l -b -q 'analysis/fit.C("build/E50GeV.root")'`.

**Cluster round-trip:** run on the cluster (its repo copy, `git pull` first),
then from the Mac `bash pull_results.sh` (curiosity → `build/`) or
`bash pull_results.sh perseverence` (→ `build/archive_perseverence/`), then analyze locally.
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

Each file's histograms: `Elyso` (GeV), `Npe` (photons/event), `dTcfd` (5% CFD,
4-corner mean — **the timing observable**), plus the `ev` ntuple below.

### What's in the ntuple (one row per event)

Stored rich enough that **any event-level graph is a `TTree::Draw` away — no
rerun needed** — and now including the complete photon-by-photon record, so
**any timing estimator** is a `TTree::Draw` away too. 19 branches. (Schema has
grown three times: 8 → 15 columns on 2026-07-28, WLS-tagged timing on
2026-07-29, replaced by the CFD + always-on waveform on 2026-08-02. Older
files have fewer columns; every analysis macro falls back gracefully and says
so on the terminal.)

**Timing — read "The light chain" above before using these:**

| column | type | meaning |
|--------|------|---------|
| **`dTcfd`** | double | ✅ **the timing observable.** ns, 4-corner mean of the 5% CFD (see above). −999 = no timing |
| `t05Up`, `t05Dn` | vector[4] | the 5% CFD time per corner per end, ns (−999 = none) — rebuild `dTcfd` any way you like |
| `phT` | vector | **every detected photon's arrival time, ns — the perfect waveform.** Always stored, no flag needed |
| `phId` | vector | which channel-end each `phT` entry belongs to: 0–3 = corners up, 4–7 = corners down, 8/9 = center up/down |
| `phWls` | vector | 1 = that photon was WLS-created, 0 = prompt Cherenkov — lets you rebuild the old first-photon estimators, test other CFD fractions, or apply SPTR smearing offline |
| `NpeWLS` | double | how many of `Npe` were WLS-created (typically ~99%) |

**Energy and light:**

| column | type | meaning |
|--------|------|---------|
| `Elyso` | double | GeV summed over all 29 LYSO plates (truth dE/dx) |
| `Npe` | double | detected photons, 4 corner fibres, 8 SiPMs (all populations) |
| `NpeCorner` | vector[4] | photons per corner — asymmetry, beam-spot starvation, **position reconstruction** |
| `NpeCenter` | double | central E-type light (0 unless `RADSIMPLE_CENTER_ETYPE=1`) |
| `Elayer` | vector[29] | GeV per LYSO layer — longitudinal profile, shower max/COG per event |
| `Ew` | double | GeV summed over all 28 W plates (sampling-fraction studies) |

**Beam line and beam truth:**

| column | type | meaning |
|--------|------|---------|
| `x`, `y` | double | this event's primary position, mm — **required for the fiducial cut** |
| `tMCP` | double | MCP particle-arrival t0, ns (−1 = off/missed) |
| `eTrig1`, `eTrig2` | double | trigger-counter deposits, MeV |
| `ePbGlass` | double | Pb-glass tail-catcher deposit, GeV — leakage, and a **measurable shower-depth proxy** |

Example draws, no rerun required:

```cpp
ev->Draw("Elayer[8]");                          // energy in layer 8
ev->Draw("Sum$(Elayer*Iteration$)/Sum$(Elayer)"); // shower COG (layers)
ev->Draw("Npe:sqrt(x*x+y*y)");                  // light vs beam offset
ev->Draw("t05Dn[2]-t05Up[2]");                  // corner-2 timing, any way you slice it

// build a DIFFERENT estimator from the stored waveform, no rerun:
ev->Draw("Min$(phT*(phId==0 && phWls==1) + 1e9*!(phId==0 && phWls==1))");
   // -> first-WLS-photon time at corner 0, up end (the old dTwls, reconstructed)
```

### How much data is stored

Per 5000-event file: **~2.5 MB** for the scalar/vector columns (≈0.5 KB/event
— physics doubles barely compress). The waveform (`phT`/`phId`/`phWls`) grows
with `Npe`: measured ~6 KB/event at 10 GeV, f=1e-2, scaling with energy and
light level — **~1 GB per 6-energy sweep** at the default thinning, more at
higher `RADSIMPLE_LIGHT_SCALE`. It is stored **unconditionally now** (there is
no "cheap" mode without it) because the whole point of this architecture is
that no estimator question should ever require a rerun.

Analyze one file: `root -l -b -q 'analysis/fit.C("build/E50GeV.root")'`
→ prints `σ_t = σ(dTcfd)/2` and `σ/E`.

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

## Results (2026-07-30 — 15 000 events/energy, light scale 1e-2)

> ⚠️ **Predates the 5% CFD estimator switch (2026-08-02).** σ_t below uses the
> first-WLS-photon estimator (`dTwls`, now removed — see "The light chain"
> above for why). Energy numbers are estimator-independent and still current.
> These use the beam fiducial cut and the 5% Pb-glass containment veto.
> **`σ_t` is measured at 1% light** — see the note under the table for what
> that means. A `dTcfd` rerun of this table is queued.

| E (GeV) | σ_t (ps) | eff | σ/E, `Npe` (measured) | σ/E, `Elyso` (truth) |
|---|---|---|---|---|
| 5   | 306.4 ± 4.6 | 100 % | 23.03 ± 0.35 | 5.54 ± 0.09 |
| 10  | 148.4 ± 2.3 | 100 % | 16.56 ± 0.25 | 3.97 ± 0.06 |
| 25  |  66.1 ± 0.9 | 100 % | 11.75 ± 0.18 | 2.76 ± 0.04 |
| 50  |  43.1 ± 0.7 | 100 % | 10.68 ± 0.19 | 2.22 ± 0.03 |
| 100 |  28.8 ± 0.5 | 100 % | **10.56 ± 0.19** | 1.86 ± 0.03 |
| 120 |  26.2 ± 0.4 | 100 % | 11.03 ± 0.18 | 1.77 ± 0.03 |

> **Why σ_t here is ~26 ps but the project quotes a ~8 ps floor.** These are at
> `RADSIMPLE_LIGHT_SCALE=1e-2` — 1% of the real light — measured with the
> now-retired first-WLS-photon estimator. More light means better timing, and
> `RADiCALsimLightScan` measured how much better by running a ladder of light
> levels: the light-independent floor is **B = 8.24 ± 0.61 ps at 120 GeV**. The
> scaling is *not* the naive 1/√N — a first-photon estimator is a **minimum**
> (an order statistic), measured exponent 1.41 ± 0.05. The switch to the 5%
> CFD (this file's "The light chain" section) exists partly *because* of this:
> a quantile is closer to mean-averaging behavior and is the estimator that
> actually matches how the real device is analyzed. Do not extrapolate these
> numbers by hand; see `../RADiCALsimLightScan/`.

- **Measured (`Npe`): σ_E/E = 46.9 %/√E ⊕ 7.82 %** (fit ≤ 50 GeV) vs the paper's
  **52.04 %/√E ⊕ 31.62 %/E ⊕ 9.31 %**. Same order, constant term within ~1.5 %.
  (Their `b/E` term is electronic noise, which this sim does not model at all.)
- **The resolution stops improving above ~100 GeV and gets 4 % worse by 120 GeV.**
  This is real, not noise (10.56 ± 0.19 → 11.03 ± 0.18). The DSB1 window is a
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
  fills a timing value when some corner has light at **both** ends, so dim
  events vanish silently and the survivors are the *brighter* ones — a σ_t
  quoted at low efficiency is biased optimistic. 100 % everywhere here, so no
  bias; `scan.C` prints it and flags any energy below 99 %.
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

The 5% CFD fraction itself is **not** a runtime flag — it is a fixed constant
(`EventAction::kCfdFrac`) that defines the stored `dTcfd`/`t05Up`/`t05Dn`
columns. Testing a *different* fraction does not need a code change or a
rerun: it is a `TTree::Draw` quantile computation on the always-stored
`phT`/`phId`/`phWls` waveform (see the ntuple section above).

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

---

## Changelog

Newest first. Dates are when the change landed, not when it was written up.

### 2026-08-02

- **Documented the two light populations** (see "The light chain" above).
  The README previously described only the WLS path and never mentioned
  `dTwls` at all — a reader had no way to know which of the two timing
  columns to use, or why there were two.
- **Documented the four WLS-timing ntuple columns** (`dTwls`, `NpeWLS`,
  `tUpWLS`, `tDnWLS`). They existed in the code since 07-29 but were missing
  from the schema table, which listed 15 of the 19 branches.
- Added `RADSIMPLE_OUT_SUBDIR` so non-standard runs write to their own
  subfolder and cannot overwrite production data.

### 2026-07-29

- **Process-tagged timing (`dTwls`)** — the big one. Every detected photon is
  tagged by creator process, and timing is computed from WLS photons only.
  Fixes the multi-modal all-light `dT` described above. Added `NpeWLS` and
  per-corner `tUpWLS`/`tDnWLS`.
- **Beam fiducial cut** in the analysis (>1.5 mm from any capillary hole,
  r < 3.5 mm). Events whose beam went down a hole or missed the 14 mm tile
  were contaminating every width; the cut restores 100% timing efficiency
  and proper 1/sqrt(E) energy scaling. The real experiment cuts the same way
  (arXiv:2303.05580 sec 3).
- **Rich ntuple** (8 -> 15 columns): per-layer energy, per-corner light and
  times, primary x/y, tungsten deposit. Enough that most new plots are a
  `TTree::Draw` away instead of a rerun.
- `run_simple.sh N` generates its macro, eliminating the stale-macro trap.

### 2026-07-28

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
