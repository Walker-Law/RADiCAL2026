# RADiCALsimWrap — does an outer reflective wrap help?

A fork of [`RADiCALsimSIMPLE`](../RADiCALsimSIMPLE/README.md) with exactly one
physics question added: **the papers only put Tyvek *between* the LYSO/W
layers — does adding a reflective wrap on the *outside* of the assembled
module (the faces you'd actually touch handling it) buy detected light, and
what does that light cost in timing?**

Everything else — the LYSO/W stack, the 4 corner WLS timing fibres, the
first-photon timing observable, the CERN H2 beamline, all the flags — is
identical to SIMPLE and works the same way. If you haven't read
[`RADiCALsimSIMPLE/README.md`](../RADiCALsimSIMPLE/README.md), read that
first; this file only covers what's different.

---

## Why this is even a question

arXiv:2401.01747 §2: Tyvek sheets "were inserted **between successive
layers** to act as reflective spacers." arXiv:2303.05580 §II: tiles are
"separated by laser-cut sheets of Tyvek, and placed in a milled delrin
housing." Neither paper puts a reflector on the *outside* of the assembled
stack — the outer boundary is whatever the Delrin housing does (unspecified),
or nothing.

Why it might matter, and why it's not obvious it helps:

- LYSO has **n = 1.81**, so at a bare LYSO/air side face, any photon beyond the
  **critical angle (~33.6°) is already totally internally reflected — for
  free, at 100%.** A wrap can only ever recover the escape cone (the photons
  that *would* have left).
- If the wrap sits in **optical contact** with the stack (no air gap), it
  **destroys that free TIR** and replaces a perfect mirror with a merely
  ~98% one — which can **lose** light on net. Real Tyvek isn't index-matched;
  it sits against a surface with microscopic air voids, so a small physical
  gap is the realistic case, and `RADWRAP_GAP_MM=0` is offered specifically to
  let you measure this trap rather than argue about it.
- A highly reflective wrap **traps** the escape-cone light that does get out,
  bouncing it for many more boundary crossings before it's absorbed or found
  by a SiPM — which is good for light yield but, as the timing probe below
  found, genuinely costly in simulation wall-clock time (see "A real finding
  before any physics result" below).

---

## The idiot guide

**1. Build it:**

```bash
mkdir -p build && cd build
source ../setup_env.sh
cmake .. -DCMAKE_PREFIX_PATH=$CONDA_PREFIX     # or $(geant4-config --prefix) if no conda
make -j
cd ..
```

**2. Probe the timing FIRST** (see the warning below — do not skip this):

```bash
bash time_probe.sh
```

Prints seconds/event for every wrap configuration and an extrapolated
full-sweep time estimate. Use it to pick an event count in step 3.

**3. Run the sweep** (locally for a quick look, or on perseverence for real
statistics):

```bash
bash run_wrap_scan.sh              # defaults: 500 events, 25 GeV only — a quick look
bash run_wrap_scan.sh 5000         # match RADiCALsimSIMPLE's run.mac stats (check the probe first!)
nohup bash run_wrap_scan.sh 5000 > scan.log 2>&1 &     # survives SSH drops, for the cluster
```

Writes `results/<config>/E<N>GeV.root` for every config in the table below
(everything except `none` — see step 4).

**4. Get the "no wrap" control — don't re-run it, stage it:**

```bash
bash stage_control.sh /path/to/RADiCALsimSIMPLE/build
```

`RADWRAP_SIDES`/`RADWRAP_ENDS` both default OFF, so a bare `radwrap` run and a
bare `radsimple` run build **byte-identical geometry**. RADiCALsimSIMPLE's own
sweep already *is* this study's `none` config — reusing it (rather than
re-running a redundant no-wrap pass through this binary) is the default
workflow. See `stage_control.sh`'s header for the same-random-seed caveat
(comparisons are statistical, not event-by-event — normally fine at
thousands of events).

**5. Cluster round-trip** (this study runs on **perseverence**):

```bash
# on perseverence: git pull, build (step 1), probe (step 2), then run (step 3)
# on the Mac, once it's done:
bash pull_wrap_results.sh perseverence
bash stage_control.sh ../RADiCALsimSIMPLE/build        # or build/short
root -l -b -q analysis/wrap_scan.C
```

**6. Analyze:**

```bash
root -l -b -q analysis/wrap_scan.C
```

Prints a table per energy (mean detected light, % change vs. `none`, timing
resolution, % change vs. `none`, energy resolution, efficiency) and writes
`results/plots/`:

| plot | what it shows |
|------|----------------|
| `npe_vs_E.png` | mean detected photons, one curve per config |
| `npe_gain_vs_E.png` | the same, as % change vs. no wrap |
| `sigma_t_vs_E.png` | timing resolution, one curve per config |
| `sigma_E_Npe_vs_E.png` | energy resolution (from detected light), one curve per config |
| `fits/dT_<config>_E<N>GeV.png` | the ΔT histogram behind each σ_t point |

---

## A real finding before any physics result: reflective wraps are SLOW

A local smoke test (10 GeV, `RADWRAP_SIDES=1`, default 0.98 reflectivity,
0.1 mm gap) confirmed the wrapped geometry is **not hung** — `top`/log
inspection showed both worker threads steadily advancing through events — but
running **roughly two orders of magnitude slower per event** than
RADiCALsimSIMPLE's no-wrap baseline. This is real physics, not a bug: a
highly reflective enclosure traps escape-cone photons in many more boundary
bounces before they're absorbed or detected, and `RADSIMPLE_PHOTON_STEP_CAP`
(default 20 000) bounds any *one* photon's runaway but does nothing about the
total *number* of photons now surviving far more steps.

**Practical consequence:** don't assume "5000 events × 6 energies, same as
`run.mac`" is a safe default for the wrapped configs — it might be. Run
`time_probe.sh` first and scale the event count/energy list in
`run_wrap_scan.sh` to what the timing budget actually allows. The `black`
(absorbing, R=0.02) config is the useful fast sanity check — low reflectivity
kills photons quickly on contact, so it should run close to baseline speed.

---

## Flags (env vars, set before the command — usually via `run_wrap_scan.sh`, not by hand)

| var | default | meaning |
|-----|---------|---------|
| `RADWRAP_SIDES` | **0** | wrap the 4 long faces (the ones you'd touch handling the module) |
| `RADWRAP_ENDS` | **0** | also cap the upstream/downstream faces (beyond the SiPMs, so it only catches light that already missed a photodetector) |
| `RADWRAP_REFLECTIVITY` | 0.98 | wrap surface reflectivity, 0–1 |
| `RADWRAP_FINISH` | `diffuse` | `diffuse` (Lambertian, like Tyvek) or `specular` (mirror-like, like ESR/mylar) |
| `RADWRAP_GAP_MM` | 0.1 | air gap between the stack and the wrap. `0` = optical contact — destroys LYSO/air TIR, the trap described above |
| `RADWRAP_THICK_MM` | 0.2032 | wrap physical thickness (matches the inter-layer Tyvek foil) |

All of RADiCALsimSIMPLE's own flags (`RADSIMPLE_LIGHT_SCALE`,
`RADSIMPLE_BEAM_SPOT_MM`, `RADSIMPLE_WITH_MCP`, etc.) work identically here —
see its README for the full list.

With no `RADWRAP_*` vars set, `./radwrap` builds **exactly** SIMPLE's
geometry — that's the control, not a special case of it.

---

## The configs `run_wrap_scan.sh` runs

`name:sides:ends:reflectivity:finish:gap_mm` — edit the `ALL_CONFIGS` array in
`run_wrap_scan.sh` to add more.

| config | sides | ends | R | finish | gap | source of the reflectivity number |
|---|---|---|---|---|---|---|
| `none` | 0 | 0 | – | – | – | the control — **staged**, not run (see step 4) |
| `tyvek` | 1 | 0 | 0.98 | diffuse | 0.1 mm | same value as this project's inter-layer foils (standard detector-wrap spec) |
| `tyvek_ends` | 1 | 1 | 0.98 | diffuse | 0.1 mm | same, + end caps (all 6 faces) |
| `esr` | 1 | 0 | 0.985 | specular | 0.1 mm | 3M ESR / VM2000, manufacturer spec |
| `mylar` | 1 | 0 | 0.90 | specular | 0.1 mm | aluminized mylar, typical vendor spec |
| `delrin` | 1 | 0 | 0.60 | diffuse | 0.1 mm | **estimate, not a measurement** — stands in for the milled Delrin housing the real module already sits in; treat as "some diffuse reflection happens," not a number to quote |
| `tyvek_contact` | 1 | 0 | 0.98 | diffuse | **0** | Tyvek in optical contact — the TIR-destroying trap, deliberately included |
| `black` | 1 | 0 | 0.02 | diffuse | 0.1 mm | absorbing wrap — pessimistic bound, and the fast sanity-check config |

Diffuse vs. specular is a real, easy-to-get-backwards distinction in Geant4's
UNIFIED optical model: with `finish=ground` and no specular constants set, the
Lambertian remainder is 1 (fully diffuse) — correct for Tyvek. Specular
(ESR/mylar) has to be asked for explicitly (`finish=polished` +
`SPECULARSPIKECONSTANT=1`), which is what `RADWRAP_FINISH=specular` does.

---

## What is deliberately NOT modeled

- No mechanical model of the wrap (seams, wrinkles, tape) — it's an idealized
  uniform-reflectivity skin.
- No wavelength dependence of the wrap's reflectivity (flat R across
  350–800 nm) — real Tyvek/ESR/mylar spectra vary somewhat; a flat number is
  the same simplification already used for the inter-layer Tyvek foils.
- No change to the tile-to-tile Tyvek or any other part of SIMPLE's geometry
  — this is a single, isolated addition, on purpose, so any effect measured
  is attributable to the outer wrap alone.
