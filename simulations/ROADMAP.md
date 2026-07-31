# RADiCAL roadmap — toward <10 ps, <10% σ_E/E, 1 mm position
*Rewritten 2026-07-30 ~23:00 after two full days of measurements. Everything
here is measured from data on disk — sources noted inline. The original
2026-07-30 morning version is in git history; statuses below supersede it.*

---

## 1. Where we stand tonight

| dataset | events | state |
|---|---|---|
| SIMPLE **15k × 6E** (curiosity) | 15000/E | on Mac, analyzed — the production reference |
| Wrap tyvek 2.5k × 6E, new schema | 2500/E | on Mac, analyzed — **valid timing verdict in hand** |
| Light-scan ladder, 4 rungs × {25,120} GeV | 2000/pt | on Mac, analyzed — **model discovery, see §2** |
| **Ladder rung f=0.1** | 1500/pt | **launched tonight on curiosity, done ~06:20** |
| SIMPLE 5k (old) / 10k 3-branch (Jul 24-25) | — | archives |
| DSB / firstsim | — | moved to `simulations/archive/` (frozen, LuAG stays active) |

**Production numbers (15k, fiducial, dTwls):**

| E (GeV) | σ_t (ps) @f=1e-2 | Npe res % | Elyso res % |
|---|---|---|---|
| 5 | 306.4 ± 4.6 | 23.03 | 5.54 |
| 25 | 66.1 ± 0.9 | 11.75 | 2.76 |
| 50 | 43.1 ± 0.7 | 10.68 | 2.22 |
| 100 | 28.8 ± 0.5 | **10.56** | 1.86 |
| 120 | 26.2 ± 0.4 | 11.03 | 1.77 |

σ_t = 333.4/√E ⊕ 0 ps (thinned); σ_E/E = 46.9%/√E ⊕ 7.82% (fit ≤50 GeV) vs
paper 52.04/√E ⊕ 31.62/E ⊕ 9.31; turn-up +4% rel from 100→120 GeV; 100%
timing efficiency everywhere.

---

## 2. Discoveries since the original roadmap (chronological)

**D1. Beam-acceptance pathologies were burying the physics** (07-29). Central
hole (σ/E = 82% for r<1 mm!), corner holes (r≈4.95 mm), and tile misses (5.4%)
averaged into a flat ~31% "resolution". The fiducial cut (>1.5 mm from any
hole, r<3.5 mm, ~37% keep) plus a 5% Pb-glass containment veto restored 100%
timing efficiency and 1/√E scaling. Same cut the real experiment makes
(2303.05580 §3).

**D2. All-light first-photon timing is multi-modal at thinned light** (07-29).
1% of detected photons are prompt Cherenkov arriving ~10 ns early; whether a
corner catches one is a coin flip at f=1e-2 → a 5-spike comb that made every
earlier σ_t fit meaningless. Fixed with the process-tagged `dTwls` estimator +
a multi-modality guard in every analysis. (Same conclusion DSB reached.)

**D3. Wrap verdict — now valid on both axes** (07-30 morning, dTwls both
sides): Tyvek on the 4 side faces gives **+133–137% light**, **σ_t −12% to
−31%** (≈ the √2.35 photostatistics expectation at 25–100 GeV, less at 120),
energy resolution ±1.5% (unchanged). The wrap is a straight win at the light
level; adoption decision pending the true-light picture (D5 changes how to
extrapolate its benefit).

**D4. Position goal effectively met** (07-29 probe, unchanged): corner-light
asymmetry gives σ_x = 0.61/0.38/0.33 mm at 10/50/120 GeV at 1% light —
already ×3 under the 1 mm goal. Formal S-curve study still pending (Phase 0).

**D5. THE BIG ONE — first-photon timing does not obey 1/√N** (07-30 tonight).
The 4-rung ladder (f = 1e-3…3e-2) *rejected* the σ_t² = A²/f + B² model
outright (χ²/ndf = 30 and 23). Diagnostic: σ_t·√f falls monotonically with f
(15.8→5.2 ps√f at 25 GeV), which is mathematically impossible for that model
with any B² ≥ 0. Cause: `dTwls` is the **minimum** of N photon times — an
order statistic, not a mean — and it improves *faster* than 1/√N. Measured
scaling: σ_t ~ f^−0.82 (25 GeV), f^−0.62 (120 GeV).
- Encouraging: light-side timing improves faster with light than naive
  counting predicts.
- Sobering: neither model can be extrapolated to f=1 — the two-term fit is
  invalid, and a pure power law has no floor and would be optimistic.
  **We still have no trustworthy true-light σ_t.**
- The one concrete lead: at 120 GeV, σ_t·√f ticks UP between f=0.01 and
  f=0.03 (2.51→2.79) — a possible first sight of the real floor.
  → **Tonight's f=0.1 rung exists to catch that turnover directly.**
- `lightscan.C` now refuses to print a true-light number when the model fails
  (it briefly printed `0.0 ± 3,075,432 ps` — fabrication, now guarded).

**D6. Light-yield sanity check passed** (S4 partially closed): Npe is linear
in f to 0.2–1.0% across a 30× lever (validates coherent thinning), and the
true-light yield extrapolates to ~60 pe per MeV *deposited* — inside DSB's
50–90 pe/MeV predicted band. The optical model is not obviously
over-collecting.

**D7. Center E-type capillary is wired end-to-end** (07-30): geometry verified
overlap-clean, `NpeCenter` > corner `Npe` in smoke tests (full-length fiber
sees more of the shower), and `scan.C` now computes the window/full-length
**ratio depth-proxy** correction automatically when a center-on dataset is
present. No production center-on sweep has been run yet — that is the
measurement that tests the SiPM-only depth correction (ceiling from truth:
15.9% → 10.8% at 120 GeV; Pb-glass proxy captures most of it at corr −0.65).

**Infrastructure landed along the way** (matters for reproducibility):
repo-wide layout (`build/rootfiles|logs|plots`) + `check_layout.sh` in CI-style
use; `run_simple.sh N` generates macros (stale-macro trap eliminated);
`RADSIMPLE_OUT_SUBDIR` protects the baseline; all pull scripts hardened
(dual-path, fail-loud); DSB+firstsim archived with the Ladder dependency
re-pointed; one `g4` conda env name across all machines + laptop.

---

## 3. Shortcomings — status

| # | shortcoming | status |
|---|---|---|
| S1 | σ_t only known at 1% light | **OPEN — sharpened.** Ladder ran; model invalid (D5). Path: find the σ_t·√f turnover empirically. f=0.1 running tonight; f≈0.3 decision after. |
| S2 | No electronics in SIMPLE | OPEN, unchanged. SIMPLE bounds the light side; device-level claim needs the archived DSB chain (SPTR knob = Phase 2). D5 makes this MORE important: if the light floor is tiny, electronics dominate the real answer. |
| S3 | Fixed 15 mm WLS window → high-E turn-up | OPEN. 15k pins it: best 10.56% @100 GeV, +4% by 120. Fixes queued: window scan + depth proxies (Phase 3). |
| S4 | Absolute light scale unvalidated | **Mostly closed** (D6): linear thinning + ~60 pe/MeV inside DSB's band. Residual: no wavelength-dependent PDE/transport. |
| S5 | No SiPM saturation | OPEN. At true light ~9×10⁵ photons reach SiPMs at 120 GeV (D6) — saturation is guaranteed relevant for the wrap decision at true light. |
| S6 | Single module ≠ array goals | OPEN — **still needs your scope call.** |
| S7 | dTwls not experimentally accessible | OPEN, unchanged — final numbers need an electronics-chain estimator. |
| S8 | Wrap variants untested | Baseline verdict done (D3); gap=0/ends variants remain flag-reachable, unrun. |

---

## 4. Gap analysis per goal

| goal | where we are | verdict |
|---|---|---|
| **<10 ps** | 26.2 ps at 120 GeV at **1% light**, scaling *faster* than 1/√N toward true light (D5); wrap gives another −12–31%; floor location unknown but possibly sighted at f~0.03 | **Open, trending favorable.** Tonight's rung is the decisive step; electronics (S2) then becomes the real fight. |
| **<10% σ_E/E** | 10.56–11.03% at 100–120 GeV uncut; truth-depth ceiling 10.8%; Pb-glass proxy measurable; center-E proxy ready to run | **Within reach** — needs the center-on sweep + 2D calibration to close. |
| **1 mm position** | 0.33–0.61 mm at 1% light; only improves with light | **Met in sim** — formalize (Phase 0). |

---

## 5. The plan (statuses updated)

### Phase 0 — offline, free
- [x] Wrap verdict memo (D3)
- [x] 15k refresh of all standard figures
- [x] Npe↔Elyso all-layer correlation + per-energy pe/GeV
- [ ] **Position S-curve study** (`analysis/position.C`): full inversion,
      residuals, σ_x,y(E), edge behavior — the probe says the goal is met;
      make it a defensible figure.
- [ ] **Pb-glass 2D depth calibration** E(Npe, ePbGlass) on the 15k set —
      no new running needed; ceiling says 16.5%→~11% at 120 GeV.

### Phase 1 — timing floor (REVISED after D5)
The A²/f+B² extrapolation is dead; the floor must be *seen*, not fitted from
below. New protocol: extend the ladder upward until σ_t·√f visibly turns over,
then read the floor from the turnover region directly.
- [x] Rungs 1e-3 … 3e-2 (done tonight — model discovery D5)
- [▶] **f = 0.1 × {25, 120} GeV × 1500 ev — RUNNING tonight, done ~06:20**
- [ ] Decision gate: if σ_t·√f rises at f=0.1 → floor is in view; one more
      rung (f≈0.3, ~1 day, possibly 120 GeV only) brackets it. If still
      falling → floor < ~3 ps√f territory and electronics officially dominate
      the <10 ps question → pivot effort to Phase 2/DSB.
- [ ] Write up the order-statistic scaling itself — σ_t ~ f^−0.6…−0.8 is a
      publishable methodological point (first-photon timing beats √N).

### Phase 2 — minimal electronics realism
- [ ] `RADSIMPLE_SPTR_PS` Gaussian smear knob (~5 lines) + one sweep at 60 ps.
      D5 raises the stakes: SPTR is likely the true limiting term.
- [ ] Timing budget figure: photostat ⊕ SPTR ⊕ floor vs E, wrap on/off.

### Phase 3 — energy: kill the turn-up
- [ ] **Center-E-type production sweep** (analysis ready per D7; one sweep,
      ~5 h on either cluster — natural next overnight after the rung).
- [ ] Window scan via `RADSIMPLE_WLS_DEPTH_MM`/`_LEN_MM` env-vars (code change
      pending) — depth {40.4, 44, 48} × len {15, 25}.
- [ ] Deliverable: σ_E/E(E) for {baseline, best window, Pb-glass-corrected,
      ratio-corrected, wrap} with the 10% line. Gate: ≤10% at 120 GeV closes
      the goal at single-module level; else escalate S6.

### Phase 4 — consolidation
- [ ] Three goal plots at true light with adopted fixes; port winning config
      to archive/RADiCALsimDSB for electronics-inclusive confirmation; record
      the wrap adoption decision.

### Standing discipline (unchanged + additions)
- Fiducial always on; keep% printed; multi-modality guard armed.
- **New:** any fit whose χ²/ndf fails its cut must refuse to print derived
  numbers (learned the hard way tonight — a 0.0 ± 3×10⁶ ps "result").
- **New:** extrapolations must state their model assumption; first-photon
  observables are order statistics and do NOT inherit mean-averaging rules.
- ETAs from measured per-cluster constants only.

---

## 6. Graph catalog — status

| # | figure | status |
|---|---|---|
| G1 | σ_t vs E true-light budget + 10 ps line | blocked on Phase 1 gate + Phase 2 |
| G2 | σ_E/E variants + 10% line | partially (baseline + fits exist); corrections pending |
| G3 | σ_x vs E + 1 mm line | probe done; formal version pending Phase 0 |
| E1 | ladder σ_t² vs 1/f | **exists** — now documents the model failure; will gain the turnover once f=0.1 lands |
| E1b | **NEW: σ_t·√f vs f** (the turnover plot — the floor is visible as a minimum) | add to lightscan.C when tonight's rung lands |
| E2/E3 | position S-curves + residuals | pending Phase 0 |
| E4 | Npe vs ePbGlass 2D + correction | pending Phase 0 |
| E5 | σ_E/E vs window depth | pending Phase 3 |
| E6/E7/E8 | shower profiles / linearity / wrap summary | **exist** (15k / wrap data) |
| E9 | pe/MeV sanity table | **numbers in hand** (D6) — worth one small figure |
| E10 | per-energy fit PNGs | auto-generated everywhere |

---

## 7. Tonight (2026-07-30, launched ~23:15)

- **curiosity**: ladder rung f=0.1, {25, 120} GeV, 1500 ev — the turnover
  hunt. Done ~06:20; pull + `lightscan.C` over coffee.
- perseverence: idle. The Phase-3 center-E-type sweep is the queued candidate
  for it — deliberately NOT launched without a go-ahead.
