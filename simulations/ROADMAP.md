# RADiCAL roadmap — toward <10 ps, <10% σ_E/E, 1 mm position
*Rewritten 2026-08-02. Every number is measured from data on disk — sources
noted inline. Supersedes the 2026-07-30 version (in git history).*

**HEADLINE: the light-transport timing floor is now MEASURED, not assumed:**

> **B = 8.24 ± 0.61 ps at 120 GeV** (q = 1.41 ± 0.05, χ²/ndf = 2.71)
> — light side only, no electronics. Under the 10 ps goal, with **~1.8 ps of
> headroom** for SPTR + amplifier + digitizer combined.

That single number reframes the whole project: **the light is no longer the
problem — the electronics are.** Everything below is organized around that.

---

## 1. Where we stand

| dataset | events | state |
|---|---|---|
| SIMPLE 15k × 6E (curiosity) | 15000/E | on Mac — production reference |
| Wrap tyvek 2.5k × 6E | 2500/E | on Mac — wrap verdict complete |
| Light-scan ladder, 6 rungs × {25,120} | 200–2000/pt | on Mac — **floor measured at 120 GeV** |
| archives | — | 5k SIMPLE, 10k 3-branch, DSB+firstsim frozen in `archive/` |

**Production reference (15k, fiducial, dTwls):** σ_t = 26.2 ± 0.4 ps @120 GeV
(at f=1e-2); σ_E/E = 46.9%/√E ⊕ 7.82% (fit ≤50 GeV), best **10.56% @100 GeV**,
11.03% @120; 100% timing efficiency; σ_x ≈ 0.33 mm @120 GeV (probe).

**The ladder (120 GeV), σ_t·√f — the turnover that made B measurable:**

| f | 0.001 | 0.003 | 0.01 | 0.03 | 0.1 | 0.3 |
|---|---|---|---|---|---|---|
| σ_t (ps) | 134.1 | 55.0 | 25.8 | 15.0 | 9.5 | 7.5 |
| σ_t·√f | 4.24 | 3.01 | **2.58 ←min** | 2.60 ↑ | 3.00 ↑ | 4.10 ↑ |

25 GeV is still falling at f=0.1 (4.86) — **its floor has not been reached**,
which is itself informative (see A2 below).

---

## 2. Discoveries (cumulative)

**D1. Beam-acceptance pathologies buried the physics.** Central hole (σ/E=82%
for r<1 mm), corner holes, tile misses (5.4%) averaged into a flat ~31%
"resolution". Fiducial cut (>1.5 mm from any hole, r<3.5 mm, ~37% keep) +
5% Pb-glass containment veto → 100% timing efficiency and 1/√E scaling.

**D2. All-light first-photon timing is multi-modal at thinned light.** 1% of
photons are prompt Cherenkov arriving ~10 ns early; at f=1e-2 whether a corner
catches one is a coin flip → 5-spike comb. Fixed by the process-tagged
`dTwls` estimator + a multi-modality guard.

**D3. Wrap verdict (valid, both axes).** Tyvek on 4 side faces: **+133–137%
light**, **σ_t −12% to −31%**, energy resolution unchanged (±1.5%).

**D4. Position goal effectively met.** σ_x = 0.61/0.38/0.33 mm at 10/50/120 GeV
*at 1% light* — 3× under the 1 mm goal, and it only improves with more light.

**D5. First-photon timing does NOT obey 1/√N — it beats it.** The σ_t²=A²/f+B²
model was rejected outright (χ²/ndf 30, 23): σ_t·√f *fell* monotonically,
impossible for that model with any B²≥0. Cause: `dTwls` is a **minimum** of N
photon times — an order statistic, not a mean. Measured exponent **q = 1.41 ±
0.05** (σ_t ~ f^−1.41/2 in amplitude terms), far steeper than the naive 0.5.
This is a publishable methodological result in its own right.

**D6. Light-yield sanity passed.** Npe linear in f to 0.2–1.1% over a 300×
lever; true-light yield ~60 pe/MeV deposited, inside DSB's 50–90 band. The
optical model is not obviously over-collecting.

**D7. Center E-type capillary wired end-to-end** — geometry overlap-clean,
`NpeCenter` > corner `Npe` as expected, `scan.C` computes the window/full-length
ratio depth-proxy automatically. **Never run in production** — that is D-tier
work now queued (§4 Run C).

**D8. THE FLOOR, MEASURED.** Once rungs existed on *both* sides of the σ_t·√f
minimum, the 3-parameter model σ_t = √(C·f^−q + B²) became identifiable:
**B = 8.24 ± 0.61 ps at 120 GeV, χ²/ndf = 2.71.** Before the turnover existed
the same fit was degenerate (χ²/ndf 8–13). The fit is now in `lightscan.C`,
guarded so it only speaks with ≥2 confirmed rises AND χ²/ndf < 5.

**D9. A fixed-binning bug nearly faked a result.** The 200-event emergency rung
first reported `22.5 ± 42.1 ps` — the code forced 300 bins regardless of
statistics, so the core fit diverged on a near-empty histogram. Adaptive
binning (~8 events/bin, clamped [20,300]) recovered the correct **7.5 ± 1.1 ps**
and flipped the fit from failing to passing. A >30% relative-error guard now
excludes such points from turnover verdicts. **Lesson: a bad histogram can
masquerade as a precise measurement — guard the histogram, not just the model.**

---

## 3. Accuracy gaps, ranked by how much they now matter

With B = 8.2 ps and only ~1.8 ps of headroom, the ranking has inverted. What
used to be "later" is now the critical path.

| # | gap | why it matters NOW | cost to close |
|---|---|---|---|
| **A1** | **Estimator choice.** `dTwls` = first photon. At true light there are ~9×10⁵ detected photons; the *minimum* of that many is an extreme-value statistic that a real device cannot reproduce (real CFD fires on the leading edge of the bulk). | **This may be the largest single systematic in the whole timing result.** If first-photon is optimistic vs a realizable estimator, B is not the real floor. | **Free** — photon-dump run (§4 Run C) makes every estimator testable offline. |
| **A2** | **B(E) — is the floor energy-dependent?** 120 GeV has turned over; 25 GeV has not. Physically B should GROW with E (deeper, longer shower → more path-length spread), so 8.2 ps may be the *worst* case and low-E may be better. | Determines whether "8.2 ps" is one number or a curve; changes the whole goal picture. | 1 run (§4 Run B) |
| **A3** | **SPTR (~60 ps single-photon jitter).** Eats the entire 1.8 ps headroom on paper. But with N huge, smearing-then-minimum pulls the minimum *earlier*, which interacts with A1 in a non-obvious way. | The difference between "goal met" and "goal missed". | **Free** — apply offline to photon-dump data; no code change, no rerun. |
| A4 | **SiPM saturation.** ~9×10⁵ photons on 5676 microcells = ~20× oversubscribed at true light. | Kills the true-light *energy* number. Timing is largely immune (first photons arrive when cells are fresh) — worth stating explicitly rather than assuming. | code + rerun |
| A5 | **PDE(λ) flat 0.36.** DSB uses the real MicroFJ curve. Blue Cherenkov vs green WLS are detected at different efficiencies → changes the population mix that D2 showed is decisive for timing. | Moderate — shifts the prompt/WLS balance. | code + rerun |
| A6 | **No photon budget cap.** DSB caps at 4M/event; SIMPLE has none. At f=1, 120 GeV that is ~5×10⁸ photons/event. | Practical: an f→1 run may OOM. Mitigated by choosing f=0.5 (§4). | small code change if needed |
| A7 | Single module ≠ 3×3 array (the papers' goal geometry) | **Needs your scope decision.** | large |
| A8 | Fixed 15 mm WLS window → high-E energy turn-up | Owns the remaining energy gap | §4 Run D + window scan |

---

## 4. The two-day campaign (2026-08-02 → 08-04)

**⚠️ CURIOSITY ONLY — perseverence is shut down.** The campaign therefore runs
**serially on one cluster**: ~40 h of work into a ~48 h window, no slack for a
failed run to be re-done. That makes *ordering* the critical decision.

**Run order is by value-per-hour, cheapest-and-most-unblocking first**, so that
if the cluster dies partway we lose the least:

| order | run | hours | cumulative | why this position |
|---|---|---|---|---|
| 0 | smoke test (3 ev) | 0.03 | — | A6 memory risk is untested; 2 min of insurance before committing 14 h |
| 1 | **C** photon dump | 5.4 | 5.4 | **Cheapest, unlocks the most.** Converts gaps A1 + A3 into offline work — after this, days of analysis need no cluster at all |
| 2 | **D** center E-type | 4.7 | 10.1 | Cheap, and the *only* thing blocking the energy goal |
| 3 | **B** 25 GeV ladder | 14.8 | 24.9 | New physics: true light + B(E) |
| 4 | **A** 120 GeV f=0.5 | 14.6 | 39.5 | Confirms a number we already have a fit for — most expendable if time runs out |

Sized from measured throughput: **wall seconds ≈ 1.095 × f × N_events × ΣE_GeV**
(fits every rung run so far to ~10%).

### Run A — the floor, measured directly [~14.6 h]
**f = 0.5, 120 GeV, 800 events.** At f=0.5 the photostatistics term contributes
only **1.8%** in quadrature — this measures B essentially *directly*, with no
model extrapolation at all. Chosen over f=1.0 deliberately: f=1.0 buys only 1.1
percentage points more (0.7% vs 1.8% excess) for **double** the cost and double
the OOM risk (5×10⁸ vs 2.5×10⁸ photons/event, against no budget cap — A6).
- **Success:** σ_t lands at 8.4 ± 0.4 ps → confirms B independently of the fit.
- **Failure mode to watch:** if it lands far from 8.4, the 3-param fit was
  wrong and B must be re-derived from the direct points only.
- ⚠️ **Smoke-test first** (3 events, ~2 min) — A6 means this is untested territory.

### Run B — B(25 GeV), and TRUE LIGHT for free [~14.8 h]
**f = 0.3 (1500 ev, 3.4 h) then f = 1.0 (1500 ev, 11.4 h), 25 GeV only.**
At 25 GeV, **f = 1.0 IS true light — zero thinning, zero extrapolation** — and
costs only 11 h because cost scales with E. This is the single most defensible
timing number the project can produce. Also finds the 25 GeV turnover, giving
**B(E)** (gap A2).

### Run C — the estimator study [~5.4 h] ← *best value per hour*
**`RADSIMPLE_STORE_PHOTON_TIMES=1`** at f=0.1 (500 ev) and f=0.5 (200 ev),
120 GeV, written to an isolated `dump/` subdir. Records **every** detected
photon's time and channel, which makes the following testable **entirely
offline, with no further cluster time**:
- first-photon vs 5th/10th/50th-photon vs threshold/CFD-like estimators (A1)
- SPTR at any value, applied by smearing and re-deriving (A3)
- the prompt-Cherenkov vs WLS composition question (D2) directly
This converts two of the three critical gaps from "needs a run" to "needs an
afternoon".

### Run D — center E-type production sweep [~4.7 h]
**`RADSIMPLE_CENTER_ETYPE=1`, 6 energies, 5000 ev, f=1e-2.** Analysis is
already written (D7). Gives the SiPM-only depth correction that targets the
high-E energy turn-up (A8) — the last blocker on the <10% goal.

### Offline, free, no cluster (do while runs execute)
- **Position S-curve study** — formalize D4 into a defensible figure.
- **Pb-glass depth correction** on existing 15k data — corr(Npe, ePbGlass) =
  −0.65; ceiling says ~16.5% → ~11% at 120 GeV.
- **Estimator + SPTR analysis** on Run C output as soon as it lands.

---

## 5. Gap analysis per goal

| goal | status | what closes it |
|---|---|---|
| **<10 ps** | **Light side: MET** (B = 8.2 ± 0.6 ps) — but only ~1.8 ps headroom, and the estimator itself (A1) is unvalidated | Runs A+B confirm B and give B(E); Run C decides whether a *realizable* estimator preserves it; then electronics (DSB chain) is the whole remaining fight |
| **<10% σ_E/E** | 10.56% @100 GeV, 11.03% @120 — just outside | Run D (center-E depth proxy) + Pb-glass correction + possibly the wrap's +133% light |
| **1 mm position** | **MET in sim** (0.33–0.61 mm, improves with light) | Formalize offline |

---

## 6. Standing discipline

- Fiducial always on; keep% always printed; multi-modality guard armed.
- **Any fit failing its χ² cut must refuse to print derived numbers** (learned
  from a `0.0 ± 3×10⁶ ps` near-miss).
- **Histogram binning must scale with statistics** (D9 — a fixed 300 bins faked
  a 22.5 ± 42.1 ps result on 82 events).
- Extrapolations must state their statistical model; **order statistics do not
  inherit mean-averaging rules** (D5).
- Non-standard runs use `RADSIMPLE_OUT_SUBDIR` so they cannot overwrite
  production rungs.
- ETAs from measured per-cluster constants only.
