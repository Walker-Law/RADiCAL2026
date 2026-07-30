# RADiCAL roadmap — toward <10 ps, <10% σ_E/E, 1 mm position
*Written overnight 2026-07-29→30 for morning review. Every number in here is
measured from data on disk tonight, not estimated — sources noted inline.
Nothing has been launched; the run commands are staged at the bottom for
approval.*

---

## 1. Where we stand tonight

| dataset | events | schema | state |
|---|---|---|---|
| SIMPLE 5k × 6E (curiosity) | 5000/E | 19-branch, `dTwls` | on Mac, analyzed |
| SIMPLE **15k × 6E** | 15000/E | same | **running on curiosity, done ~07:15** |
| Wrap tyvek 2.5k × 6E (perseverence) | 2500/E | same | on Mac, analyzed |
| Wrap control (= SIMPLE staged) | 5000/E | same | staged |
| SIMPLE 10k old-schema (Jul 24-25) | 10000/E | 3-branch | `build/archive_2026-07-25/` |
| DSB full-chain results | — | — | paperJ/paper183/paperR + ladder (see its CLAUDE.md) |
| Real test-beam DRS4 data | ~30k evt × 4 runs | — | `RADiCAL/Data/`, σ_t ≈ 476–614 ps uncalibrated |

**Validated results (fiducial, dTwls, 5k):**

- σ_t = **328/√E ⊕ ~0 ps** at LIGHT_SCALE=1e-2 (25.1 ± 0.6 ps at 120 GeV), 100% efficiency
- σ_E/E (Npe) = **45.5%/√E ⊕ 8.98%** (fit ≤50 GeV) vs paper 52.04/√E ⊕ 31.62/E ⊕ 9.31 — but **12.5% at 120 GeV** (turn-up, fixed WLS window)
- σ_E/E (Elyso truth) = 11.7%/√E ⊕ 1.61% — the stack itself has huge headroom
- Light: 598 pe per GeV deposited (×100 for true light); per-energy slope drifts ±6% (depth effect)
- **Tyvek wrap: +133–137% light, σ_t −12% to −31%, energy res ±1.5%** (first valid timing comparison, tonight)

---

## 2. Shortcomings — ranked by how much they threaten conclusions

**S1. Every σ_t we quote is at 1% light.** The single biggest caveat. At
LIGHT_SCALE=1e-2 the photostatistics term is ~10× inflated. Extrapolating
328/√E by √100 → ~33/√E ps assumes the *entire* width is photostatistical —
the DSB ladder proved that assumption can hide a floor (it found B = 43.5 ps
there). **We have never run a scale ladder on SIMPLE.** Until we do, we cannot
say whether the true-light floor is 2 ps or 40 ps, i.e. whether the <10 ps
goal is alive. → Phase 1.

**S2. No electronics anywhere in SIMPLE.** First-photon timing with an ideal
sensor. The real device adds: SiPM SPTR (~60 ps single-photon), amplifier
noise-over-slope, DRS4 timebase (~50 ps/cell, partially cancelling in DW−UP),
CFD behavior on a 8.3 ns pulse. The real test-beam data sits at ~500 ps
*because* of this chain (DSB's finding: threshold-independence proves it's not
photostatistics). SIMPLE can bound the light-side budget; it cannot claim a
device-level <10 ps alone. The DSB sim owns that chain. → Phase 2 adds the
one cheap term (SPTR); full-chain validation stays in DSB.

**S3. The fixed 15 mm WLS window at 40.4 mm.** Causes the measured energy
turn-up (11.4% best at 100 GeV → 12.5% at 120) and the ±6% pe/GeV drift.
Paper flags it themselves ("not optimized... will be corrected"). Window
depth/length are compile-time constants — not scannable without a code touch.
→ Phase 3.

**S4. Absolute light scale unvalidated.** SIMPLE's optics are simpler than
DSB's (flat PDE 0.36, no rod-surface roughness, no wavelength-dependent
transport), and DSB measured its own light over-collection at 3–4× before its
roughness fix. If SIMPLE over-collects similarly, every √N extrapolation is
optimistic by ~2×. Cheap cross-check: SIMPLE predicts 598 pe/GeV_dep ×100 =
~60k pe/GeV true ≈ **~60 pe/MeV** — DSB's post-fix prediction was ~50-90
pe/MeV, so we're in the same band, but this deserves one deliberate
comparison table. → Phase 1 sanity row.

**S5. No SiPM saturation.** 5676-cell MicroFJ at true light with the wrap's
2.35× would clip hard (DSB measured 74% clipped pulses at 150 GeV in data).
Npe linearity conclusions at true light are unproven. Matters most for the
wrap adoption decision. → noted gate in Phase 3.

**S6. Single module ≠ the array goals.** R_M = 13.7 mm vs a 14 mm tile: a
single cell is intrinsically leakage-limited laterally; the papers' <10%
design goal belongs to a 3×3 array. Our fiducial-cut numbers are the
single-cell best case. If the goal is strict single-module <10% at all E, the
depth correction (below) is the lever; if array-level is acceptable, that's a
geometry build we haven't started. **Needs your call on scope.**

**S7. dTwls is a sim-side estimator.** Process-tagging isn't available to a
real SiPM — the real device sees all light through electronics. Fine for
studying the light-transport physics (and the real CFD on the WLS-dominated
bulk behaves like it), but the final quoted number must come from an
electronics-chain estimator (DSB's CFD path). Same conclusion DSB reached.

**S8. Housekeeping debt.** Wrap README still documents `RADWRAP_ENDS` etc. —
fine; but the wrap study has only tested ONE wrap config with valid timing
(sides, R=0.98, 0.1 mm gap). The gap=0 trap and ends-on variants exist as
flags, untested with dTwls. Low priority; flags are there when wanted.

---

## 3. Tonight's feasibility probes (new information)

Ran directly on the ntuples tonight; these reshape the plan:

**P1. Position from corner-light asymmetry — the 1 mm goal looks ALREADY MET
in sim.** Using `NpeCorner` x-asymmetry (corners at +x vs −x), fiducial
events, linearized at x=0:

| E (GeV) | slope (1/mm) | σ(A_x) | → σ_x |
|---|---|---|---|
| 10 | 0.105 | 0.065 | **0.61 mm** |
| 50 | 0.114 | 0.043 | **0.38 mm** |
| 120 | 0.119 | 0.039 | **0.33 mm** |

And this is at 1% light — photostatistics shrinks the asymmetry noise further
at true light. Needs the full S-curve treatment (nonlinearity toward edges,
y-axis, energy dependence, true-light extrapolation), but the goal is not in
danger. *This was measurable only since the schema extension — first time
anyone looked.*

**P2. Timing does NOT measure shower depth at thinned light.** corr(dTwls,
layer-COG) = −0.02…−0.05. The fixed window pins the light origin; first-photon
jitter buries the residual. Kills "use dT to correct energy" at thinned light
(retest once at true-light equivalent, but don't plan around it).

**P3. Depth correction of the energy IS the high-E fix — and it's measurable.**
Within the fiducial: corr(Npe, layer-COG) = −0.58/−0.69/−0.73 at 50/100/120
GeV. A perfect depth correction takes the raw core spread 15.9% → **10.8% at
120 GeV** (−32% relative). And a *real, measurable* proxy exists — the
Pb-glass tail-catcher we already model and the real beamline already had:
corr(Npe, ePbGlass) = −0.65 at 120 GeV, capturing most of the truth-COG
information. A 2D calibration E_reco = f(Npe, ePbGlass) is the single
highest-value energy lead. The center E-type capillary (flag exists,
RADSIMPLE_CENTER_ETYPE) offers a second proxy: window/full-length light ratio.

---

## 4. Gap analysis per goal

| goal | where we are | gap | verdict |
|---|---|---|---|
| **<10 ps σ_t** | 25 ps at 120 GeV *at 1% light*; naive √100 → 2.5 ps but floor unmeasured; wrap −25%; paper's own constant term 17.5 ps (electronics-inclusive) | ladder B unknown; electronics chain unmodeled here | **undecided until Phase 1**; light-side plausible, device-level hinges on electronics (DSB track) |
| **<10% σ_E/E** | 8.98% constant ≤50 GeV (already <10 there); 12.5% at 120 | high-E turn-up | **likely reachable** via Pb-glass depth correction (ceiling says 10.8% at 120) + window optimization; array scope decision pending |
| **1 mm position** | 0.33–0.61 mm from corner asymmetry at 1% light | full S-curve + edges | **effectively met in sim**; formalize and publish the curve |

---

## 5. The plan

### Phase 0 — free, offline, on data we already have (start tomorrow)
No cluster time. All on 15k data when it lands (~07:15), 5k meanwhile.
1. **Position paper-grade study**: S-curves A_x(x), A_y(y) per energy; invert;
   residual distributions; σ_x,y vs E; edge behavior to the fiducial boundary.
   *(new macro `analysis/position.C`)*
2. **Depth-corrected energy**: 2D profile Npe vs ePbGlass per E; derive
   correction; before/after σ_E/E vs E. Same with NpeCenter ratio once a
   center-on sweep exists (Phase 3).
3. **Wrap verdict memo**: finalize tyvek table (light/timing/energy/position
   effect of wrap — position uses NpeCorner, so wrap changes it; check).
4. 15k refresh of every existing figure + the σ_t=328/√E fit error bars.

### Phase 1 — the scale ladder (decides the timing goal) — needs approval
σ_t²(f) = A²/f + B² per energy; extrapolate f→1 (true light). B is the
floor; B vs 10 ps is the verdict.
- Scales: LIGHT_SCALE ∈ {1e-3, 3e-3, **1e-2 = reuse the 15k run**, 3e-2},
  optional 1e-1 confirmation point later.
- Energies {25, 120}, 4000 ev/point (fiducial keeps ~1500).
- Cost (measured 5.63 core-s/ev/GeV at 1e-2, cost ≈ ∝ scale):
  perseverence takes {1e-3, 3e-3} ≈ 2 h total; curiosity takes {3e-2} ≈ 5.5 h
  after the 15k run finishes. 1e-1 = 15 h, only if the fit demands it.
- Sanity row: pe/MeV prediction vs DSB's ~50-90 (S4).
- **Gate T1**: B < 5 ps → photostatistics/electronics-dominated; proceed to
  Phase 2 knowing light is not the limit. B > 15 ps → dissect the floor
  (window length, path dispersion) *before* any electronics work.

### Phase 2 — minimal realism on the timing (cheap code, one sweep)
1. Add `RADSIMPLE_SPTR_PS` (Gaussian smear per detected photon, default 0
   = current behavior; ~5 lines in EventAction). Run one sweep at 60 ps.
2. Rebuild the **timing budget plot**: σ_t(E) stacked as photostat ⊕ SPTR ⊕
   floor, at true light, wrap on/off. This is the figure that says whether
   <10 ps survives contact with a real sensor — and hands DSB a target.

### Phase 3 — energy: kill the turn-up — needs approval (one overnight)
1. Env-var the window: `RADSIMPLE_WLS_DEPTH_MM`, `RADSIMPLE_WLS_LEN_MM`
   (defaults unchanged). Scan depth {40.4, 44, 48} × len {15, 25} at
   {25, 50, 120} GeV, 3000 ev → ~9 h, one cluster overnight.
2. One sweep with `RADSIMPLE_CENTER_ETYPE=1` → window/full ratio as the
   second depth proxy; compare against Pb-glass proxy.
3. Deliverable: σ_E/E vs E for {baseline, best window, depth-corrected,
   wrap} with the 10% goal line. **Gate E1**: if corrected 120 GeV ≤10%,
   energy goal closes at single-module level; else escalate the array
   question (S6).

### Phase 4 — consolidation
- σ_t and σ_E/E and σ_x final curves at true light with all adopted fixes
  (wrap? window? correction?) — the three "goal plots" with goal lines.
- Port the adopted config to DSB for electronics-inclusive confirmation of
  the timing number (S2/S7 close-out).
- Wrap adoption decision recorded in both READMEs.

### Standing discipline (all phases)
- Fiducial cut always on; keep% always printed.
- Any resolution from a fit gets its per-energy fit PNG saved.
- Multi-modality guard stays armed everywhere σ_t is fitted.
- ETAs only from measured per-cluster constants (5.63 / 6.53 core-s/ev/GeV).
- New observables → ntuple columns, never new study forks.

---

## 6. Graph catalog

**Goal plots (the three that matter):**
| # | figure | axes | needs |
|---|---|---|---|
| G1 | σ_t vs E, true light, budget-stacked, 10 ps line | ps vs GeV | Phase 1+2 |
| G2 | σ_E/E vs E, variants + corrected, 10% line | % vs GeV | Phase 0.2 + 3 |
| G3 | σ_x vs E, true-light band, 1 mm line | mm vs GeV | Phase 0.1 |

**Evidence plots behind them:**
| # | figure | why |
|---|---|---|
| E1 | σ_t² vs 1/f per energy (ladder lines) | the A/B separation itself |
| E2 | A_x vs x S-curve per energy | position calibration curve |
| E3 | x_reco − x_true residuals | the 1 mm claim, honestly shaped |
| E4 | Npe vs ePbGlass 2D + correction contour | the depth correction, visibly |
| E5 | σ_E/E vs window depth (scan) | window optimization result |
| E6 | Elayer profile overlay per E + window band | shows WHY the turn-up (have data) |
| E7 | Npe linearity E_reco/E_beam vs E | response flatness (have data) |
| E8 | wrap: ΔNpe, Δσ_t, Δσ_x summary | wrap adoption evidence (have data) |
| E9 | pe/MeV table sim-vs-DSB-vs-paper-era numbers | light-scale sanity (S4) |
| E10 | dTwls fit PNGs per energy (existing) | fit transparency, keep generating |

---

## 7. Approval list (nothing launched)

- [ ] Phase 1 ladder: 2 short runs on perseverence (~2 h) + 1 on curiosity
      after 15k finishes (~5.5 h)
- [ ] Phase 2 SPTR knob (code change, then one ~5 h sweep)
- [ ] Phase 3 window env-vars (code change) + overnight scan (~9 h)
- [ ] Scope call on S6: single-module goals vs 3×3 array
- [ ] Phase 0 needs no approval — offline analysis only; I'll start with the
      position S-curve study on the 15k data unless redirected
