# RADiCAL roadmap — toward <10 ps, <10% σ_E/E, 1 mm position
*Rewritten 2026-08-02, continuously updated since. Every number is measured
from data on disk — sources noted inline. Supersedes the 2026-07-30 version
(in git history).*

**HEADLINE (updated 2026-08-16):**

> **Timing, realizable estimator:** under the electronics-free **5% constant fraction discriminator**
> (`dTcfd`) the light-transport resolution at 120 GeV, true light, is
> **≈ 28 ps** (direct f=0.5 point 28.2 ± 1.8 ps; ladder extrapolation
> 27.1 ± 2.5 ps; turnover floor B = 20.4 ± 5.8 ps, χ²/ndf = 0.42).
> **The old B = 8.24 ± 0.61 ps was an artifact of the retired first-photon
> estimator** (Discovery 11) — the <10 ps goal is NOT met by light transport alone
> under the estimator a real device uses.
>
> **Three independent escape routes were checked and all failed — this is
> now a settled conclusion, not an open question:**
> 1. **A cleverer estimator?** No. First photon (pure Cherenkov) beats the
>    constant fraction discriminator, but averaging over the early burst to
>    make it realizable either
>    hits the wrong photon population (Discovery 12) or is too energy-fragile to
>    trust (Discovery 14). Best found: 26 ps at 120 GeV, useless at 25 GeV.
> 2. **Is single-photon time resolution the real problem, so a better silicon photomultiplier would fix it?** No —
>    the opposite: `dTcfd` is **immune to single-photon time resolution smearing** (Discovery 14). 100 ps of single-photon
>    jitter moves it ~1 ps. The floor was never an electronics-noise
>    problem to begin with.
> 3. **Would a different beam energy help?** No — **B falls as energy
>    RISES** (Discovery 15, backwards from the original guess), only as E^−0.15,
>    so closing 28→10 ps needs ~3500 GeV. Not a lever.
>
> **Net: <10 ps is now purely a hardware-geometry question** (faster light
> path, thinner/prompt-collecting design), not an analysis or estimator
> question. The simulation's remaining timing role is testing hardware
> variants under `dTcfd`. Dispersion (Gap A9) is now confirmed to matter —
> **+17% at a thinned rung, direction as predicted, 1.1σ** (Discovery 17) —
> but its effect on the ~28 ps floor itself is still open (Gap A9b).
>
> **Energy: the <10% goal is MET in simulation.** The center E-type depth
> correction (Discovery 7 hardware, run 2026-08-03) brings σ_E/E at 120 GeV from
> 11.67% → **6.03%**, and every energy 10–120 GeV lands at 6.0–9.5%.
>
> **Most defensible single number:** 25 GeV at f = 1.0 — TRUE light, zero
> thinning, zero extrapolation — **σ_t = 35.5 ± 1.9 ps** (5% constant fraction discriminator, fiducial;
> the 34.5 ± 2.2 first quoted came from `scan.C` before its binning fix, Discovery 13).

Three flips in a week: the light side went from "solved" (8 ps) to "broken"
(28 ps, Discovery 11) to "broken and confirmed unfixable by estimator/electronics/
energy tricks" (Discovery 14, Discovery 15) — while the energy goal, which looked blocked,
turned out to be closed all along (Gap A8). Everything below is organized
around that. One open systematic remains before ~28 ps itself is fully
trusted: chromatic dispersion, now rebuilt and confirmed to shift the
timing spread (+17%, Discovery 17), but not yet measured at the floor
regime itself (Gap A9b).

---

## 1. Where we stand

| dataset | events | state |
|---|---|---|
| SIMPLE 15k × 6E (curiosity) | 15000/E | on Mac — production reference (pre-constant-fraction-discriminator schema) |
| Wrap tyvek 2.5k × 6E | 2500/E | on Mac — wrap verdict complete |
| Light-scan ladder, 6 rungs × {25,120} | 200–2000/pt | on Mac — dTwls-era floor (superseded by Discovery 11) |
| **constant fraction discriminator ladder, 6 rungs × 120 GeV** | 500/pt | on Mac — **dTcfd floor measured**, full waveforms |
| **Run A: 120 GeV f=0.5** | 800 | on Mac — direct floor point + waveforms |
| **Run B: 25 GeV f=0.3, f=1.0** | 1500 each | on Mac — TRUE-LIGHT timing + waveforms |
| **centerE: 6E × 5000** | 5000/E | on Mac — energy depth-correction, goal met |
| **25 GeV constant fraction discriminator ladder, 7 rungs (f=1e-3…1.0)** | 500–1500/pt | on Mac — **DONE 08-09**, B(25 GeV) measured (Discovery 15) |
| 10 GeV / 5 GeV constant fraction discriminator ladders | — | **not run** — planned in Section 4b but the chain crashed after only 25 GeV finished; deprioritized since Discovery 15 already shows energy isn't a lever (lower E only gets worse) |
| **dispersion_check, 120 GeV, f=0.01** | 500 (+ 50 discarded) | on Mac — **DONE 08-16**: +17% shift versus the pre-dispersion rung, direction confirmed, 1.1σ (Discovery 17); the 50-event first attempt was unusable and discarded, not reported |
| archives | — | 5k SIMPLE, 10k 3-branch, DSB+firstsim frozen in `archive/` |

**Production reference (15k, fiducial, dTwls):** σ_t = 26.2 ± 0.4 ps @120 GeV
(at f=1e-2); σ_E/E = 46.9%/√E ⊕ 7.82% (fit ≤50 GeV), best **10.56% @100 GeV**,
11.03% @120; 100% timing efficiency; σ_x ≈ 0.33 mm @120 GeV (probe).

**The ladder above is the ORIGINAL dTwls-era 120 GeV ladder** (Discovery 8's now-
superseded 8.24 ps fit) — kept here only as the historical record of what
first showed a turnover existed. For the current, trusted numbers use the
constant-fraction-discriminator-era ladders: 120 GeV (Discovery 11, B=20.4±5.8 ps) and 25 GeV (Discovery 15, B=32.8±2.4
ps).

---

## 2. Discoveries (cumulative)

**Discovery 1. Beam-acceptance pathologies buried the physics.** Central hole (σ/E=82%
for r<1 mm), corner holes, tile misses (5.4%) averaged into a flat ~31%
"resolution". Fiducial cut (>1.5 mm from any hole, r<3.5 mm, ~37% keep) +
5% Pb-glass containment veto → 100% timing efficiency and 1/√E scaling.

**Discovery 2. All-light first-photon timing is multi-modal at thinned light.** 1% of
photons are prompt Cherenkov arriving ~10 ns early; at f=1e-2 whether a corner
catches one is a coin flip → 5-spike comb. Fixed by the process-tagged
`dTwls` estimator + a multi-modality guard.

**Discovery 3. Wrap verdict (valid, both axes).** Tyvek on 4 side faces: **+133–137%
light**, **σ_t −12% to −31%**, energy resolution unchanged (±1.5%).

**Discovery 4. Position goal effectively met.** σ_x = 0.61/0.38/0.33 mm at 10/50/120 GeV
*at 1% light* — 3× under the 1 mm goal, and it only improves with more light.

**Discovery 5. First-photon timing does NOT obey 1/√N — it beats it.** The σ_t²=A²/f+B²
model was rejected outright (χ²/ndf 30, 23): σ_t·√f *fell* monotonically,
impossible for that model with any B²≥0. Cause: `dTwls` is a **minimum** of N
photon times — an order statistic, not a mean. Measured exponent **q = 1.41 ±
0.05** (σ_t ~ f^−1.41/2 in amplitude terms), far steeper than the naive 0.5.
This is a publishable methodological result in its own right.

**Discovery 6. Light-yield sanity passed.** Npe linear in f to 0.2–1.1% over a 300×
lever; true-light yield ~60 pe/MeV deposited, inside DSB's 50–90 band. The
optical model is not obviously over-collecting.

**Discovery 7. Center E-type capillary wired end-to-end** — geometry overlap-clean,
`NpeCenter` > corner `Npe` as expected, `scan.C` computes the window/full-length
ratio depth-proxy automatically. **Never run in production** — that is D-tier
work now queued (Section 4 Run D).

**Discovery 8. THE FLOOR, MEASURED — since superseded by Discovery 11 (the number was
estimator-specific, not physical).** Once rungs existed on *both* sides of the σ_t·√f
minimum, the 3-parameter model σ_t = √(C·f^−q + B²) became identifiable:
**B = 8.24 ± 0.61 ps at 120 GeV, χ²/ndf = 2.71.** Before the turnover existed
the same fit was degenerate (χ²/ndf 8–13). The fit is now in `lightscan.C`,
guarded so it only speaks with ≥2 confirmed rises AND χ²/ndf < 5.

**Discovery 9. A fixed-binning bug nearly faked a result.** The 200-event emergency rung
first reported `22.5 ± 42.1 ps` — the code forced 300 bins regardless of
statistics, so the core fit diverged on a near-empty histogram. Adaptive
binning (~8 events/bin, clamped [20,300]) recovered the correct **7.5 ± 1.1 ps**
and flipped the fit from failing to passing. A >30% relative-error guard now
excludes such points from turnover verdicts. **Lesson: a bad histogram can
masquerade as a precise measurement — guard the histogram, not just the model.**

**Discovery 10. Architecture pivot: from "pick an estimator" to "record everything"
(2026-08-02).** `dTwls` — first-photon-per-corner — is an order statistic
(Discovery 5) and, at thinned light, was shown by Discovery 2 to go multi-modal. Rather than
patch estimator #3, the simulation was rebuilt as a **light recorder**: every
detected photon's arrival time, silicon-photomultiplier end, and wavelength-shifted-versus-prompt tag is now
stored unconditionally (`phT`/`phId`/`phWls`, no flag, no opt-in — this used
to require `RADSIMPLE_STORE_PHOTON_TIMES=1`, now removed), and the simulation
computes exactly one in-simulation trigger from it: an electronics-free
**5% constant fraction discriminator** (`dTcfd`) — the arrival time of the `⌈0.05N⌉`-th photon at each
silicon-photomultiplier face, via `std::nth_element`. This is the direct light-level analog of
the real test-beam's own 5% constant fraction discriminator convention, so simulated and real timing
numbers are comparable for the first time. Every other estimator (first-
photon, Nth-photon, alternate constant-fraction-discriminator fractions, single-photon-time-resolution-smeared variants) is now
a `TTree::Draw` on the stored waveform, offline, no rerun. Consequence:
**B = 8.24 ps (Discovery 8) was measured on the retired `dTwls` and must be
re-derived on `dTcfd`** before it can be called final — Gap A1c, Section 4 Run C.

**Discovery 11. THE 8.24 ps FLOOR DID NOT SURVIVE THE ESTIMATOR SWITCH (2026-08-04,
Section 4 campaign results).** Under `dTcfd` the 120 GeV ladder gives **B = 20.4 ±
5.8 ps** (q = 0.90 ± 0.07, χ²/ndf = 0.42, clean 4-rise turnover), the linear
A/B fit predicts true-light **σ_t = 27.1 ± 2.5 ps**, and the direct f=0.5
run confirms it: **28.2 ± 1.8 ps**. The scaling exponent flipped to the
*other* side of naive photon counting (σ_t ~ f^−0.40 versus first-photon's
f^−0.7): a quantile improves *slower* than √N. So the old 8.24 ps was a
property of the minimum-of-N statistic, not of the light. At 25 GeV, true
light, measured directly with zero extrapolation: **σ_t = 34.5 ± 2.2 ps**
(still photostatistics-limited — f=0.3→1.0 was still improving steeply, so
the 25 GeV floor is below this). Light transport under a realizable
estimator sits at ~28 ps at 120 GeV: **the <10 ps goal is not met by light
alone**, and estimator/correction engineering on the stored waveforms is
now the timing critical path.

**Discovery 12. The offline fraction scan (`analysis/cfdfrac.C`, on stored
waveforms — zero cluster time) mapped the whole estimator landscape:**

| estimator (120 GeV, f=0.5) | σ_t (ps) | prompt share below threshold |
|---|---|---|
| first photon | 18.1 ± 1.2 | 100% — pure Cherenkov |
| 0.5–2% quantile | unstable/bimodal | 9–35% — the prompt-versus-wavelength-shifted-light transition zone |
| **5% constant fraction discriminator (the in-simulation trigger)** | **30.7 ± 2.4** | **3.7% — safely wavelength-shifted light** |
| 10% / 20% | 35.8 / 45.0 | 1.9% / 1.0% |

Three findings: **(a) Gap A1b is answered — the 5% threshold does NOT fire on
the Cherenkov precursor** (≈4% prompt contamination, at both 120 and 25
GeV); the back-of-envelope worry was wrong in the safe direction. **(b) 5%
is near-optimal among stable wavelength-shifted-light-side fractions** — going lower hits the
bimodal transition zone (the Discovery 2 comb, resurfacing at the quantile level).
**(c) The first photon is pure Cherenkov and *beats* the constant fraction discriminator** (18 ps at
120 GeV, 25 ps at 25 GeV). Not realizable as a single photon (single-photon time resolution ~60 ps),
but at true light the prompt cluster is ~10³ photons/end — a constant fraction discriminator *within
the Cherenkov cluster* would average enough of them to beat the single-photon time resolution smearing, and is
computable offline from `phT`/`phWls`. That was the one identified path
back toward ~20 ps or below without touching electronics — **tested in
Discovery 14: it mostly does not survive.**

**Discovery 13. Discovery 9 recurred in `scan.C` (2026-08-04).** Its timing fit had a fixed
400-bin/±3 ns histogram; on the 293-fiducial-event Run A it quantized the
core and reported 48.9 ± 9.8 ps where the true value was 30.7 ± 2.4 (caught
because the stored-column refit and the waveform recomputation in
`cfdfrac.C` agreed with each other and disagreed with `scan.C`). Now
adaptive (~8 events/bin, clamped [20,300]) like `lightscan.C`; the 15k
production numbers reproduce exactly. **Lesson upgraded: every fit macro
gets adaptive binning the day it's written, not the day it fails.**

**Discovery 14. Single-photon time resolution is harmless to the constant fraction discriminator, and Cherenkov-cluster timing dies on
contact with data (2026-08-08, `analysis/sptr_est.C` — all offline, zero
cluster time).** Gaussian-smeared every stored photon time and re-derived
every estimator at single-photon time resolution = 0/30/60/100 ps:

| estimator, σ_t (ps) | 120 GeV f=0.5, single-photon time resolution 0 → 60 | 25 GeV TRUE light, single-photon time resolution 0 → 60 |
|---|---|---|
| **5% constant fraction discriminator (recomputed)** | **30.7 → 30.8** | **35.5 → 37.3** |
| all-event Cherenkov mean (truth) | ~3700 (wrong population) | ~5600 (wrong population) |
| pre-wavelength-shifted-light burst mean (truth ceiling) | 47.6 → 47.3 | 89.0 → 87.7 |
| mean of first 10 photons | 22.3 → 26.1 | 136.1 → 140.7 |

Four verdicts. **(a) Gap A3 is answered: the constant fraction discriminator is immune to single-photon time resolution smearing** — even 100 ps
of single-photon jitter moves it by ~1 ps, because a quantile of ~10³–10⁴
photons averages the noise away. The ~28–31 ps light-side number stands
as-is in front of a real silicon photomultiplier. **(b) Naive cluster averaging fails for a
reason worth remembering:** averaging only reduces noise when every sample
measures the *same* thing. The Cherenkov tag spans the whole shower
duration (late-shower Cherenkov → ns-scale spread), and any early *window*
(fraction- or count-based) that reaches past the true prompt spike mixes
two populations and inherits the wider one's variance. **(c) The one
partial survivor:** mean-of-first-~10-photons beats the constant fraction discriminator at 120 GeV
(26 versus 31 ps, robust to single-photon time resolution) — but collapses at 25 GeV (140 ps), because the
prompt cluster shrinks with energy and K=10 over-reaches it. An
energy-dependent estimator that needs per-configuration tuning is not a
result to build on. **(d) Gap A1d is closed, negative: no realizable estimator
found below ~26 ps at 120 GeV, and none that generalizes across energy.**
Light transport + estimator cleverness ends at ~28–31 ps (120 GeV) /
~35–37 ps (25 GeV, true light, single-photon time resolution 60). Getting under 10 ps is now
squarely a *hardware* question (faster light path — thinner window,
prompt-light collection, faster wavelength-shifting material), not an analysis question.

**Discovery 15. B(E) RUNS THE WRONG WAY — low energy is WORSE (2026-08-09).** The
25 GeV constant fraction discriminator ladder (5 local rungs f=1e-3…1e-1, joined to the existing f=0.3
and f=1.0 Run B points → 7 rungs spanning 3 decades) turns over cleanly:
**B = 32.8 ± 2.4 ps** (q = 1.12 ± 0.05, χ²/ndf = 0.83), linear A/B fit
29.0 ps, and the **direct f=1.0 true-light point 35.5 ± 1.9 ps** — three
methods agreeing. Against 120 GeV's B = 20.4 ± 5.8 / direct 28.2 ± 1.8 ps:
**the floor FALLS with energy**, the opposite of Gap A2's stated
expectation ("B should GROW with E — deeper shower, more path spread, so
low-E may be better"). That reasoning was wrong: B is not set by fixed
path-length geometry but by **shower-to-shower fluctuation in the light
emission profile**, which averages down as particle multiplicity grows.
More energy → more particles → more reproducible pulse → lower floor.

**The scaling is shallow, and that closes a door.** Direct points give
B ~ E^−0.15; the turnover fits give E^−0.30. Even at the steeper exponent,
reaching 10 ps from 28 ps at 120 GeV needs ~3500 GeV. **No achievable beam
energy closes the timing gap** — energy is not a lever, and CMS-relevant
energies are *below* our best measured point, that is, worse than 28 ps.

**Discovery 16. The turnover guard was counting the wrong thing (2026-08-09).**
`lightscan.C` claimed "≥2 **consecutive** rises" but counted rises
*anywhere* in the sequence. The 5-rung 25 GeV ladder — σ_t·√f = 22.7, 20.6,
23.1, 18.6, 19.2 — that is, **flat within errors**, dn/UP/dn/UP scatter — scored
"2 rises", passed the guard, and printed **B = 18.8 ± 30.7 ps: a 163%
relative error reported as a measurement.** Two fixes: rises must now be
an unbroken run, and B is suppressed unless its own relative error clears
the same 30% bar the input points must clear. Re-verified: the 5-rung
ladder now correctly says "possible turnover (1 rise)", while the 7-rung
25 GeV and 6-rung 120 GeV verdicts are unchanged (see Discovery 15 above for the
correct 7-rung result this guard now protects). **This is Discovery 9's lesson
recurring a third time** (after `scan.C`'s fixed binning, Discovery 13) — and the
first time the failure was in a *guard* rather than a fit. Guards need
their own regression tests; a guard that never fires looks identical to a
guard that works.

**Discovery 17. Chromatic dispersion measurably shifts the timing spread,
direction as predicted — magnitude at the floor itself still open
(2026-08-15/16).** Rebuilt `RADiCALsimSIMPLE` with the Sellmeier curves
prepared for Gap A9 (Malitson quartz; an effective single-pole LYSO curve
anchored to phase index n(420)=1.82 and group index n_g(420)≈1.95; a
renormalized wavelength-shifting-material curve) — first clean build of
that source, confirmed with `make` before any run. Same-configuration
comparison, 120 GeV, f=0.01 (standard production thinning), 500 events,
`dTcfd`:

| | σ_t (ps) | source |
|---|---|---|
| pre-dispersion (flat RINDEX) | 99.7 ± 8.1 | the Discovery 11 ladder, same rung |
| with dispersion | 116.9 ± 12.9 | this check, 500 events, 30.6% fiducial keep |

**+17% (+17.2 ps), in the predicted direction** — dispersion adds a
chromatic-spread mechanism the flat tables structurally could not produce
— but at only **1.1σ significance** (combined error 15.2 ps): real-looking,
not proven at this statistics.

**A first attempt at this same check used only 50 events and had to be
discarded.** It returned 180.6 ± 108.9 ps — a 60% relative error, with the
light-yield fit even worse (23.15 ± 52.18%, error bigger than the value) —
off only ~16 fiducial events. **This is the fourth time this project has
hit the small-statistics trap**, after Discovery 9 (the binning bug),
Discovery 13 (`scan.C`'s recurrence), and Discovery 16 (the turnover
guard's recurrence). This time is different in kind: the standing
discipline (Section 6) worked as intended — the bad number was visibly
bad (its own error bar said so) and was discarded before being used for
anything, rather than silently reported as a result.

**What this does and does not establish.** f=0.01 is deep in the
photostatistics-dominated part of the curve, not the near-true-light
regime (f≥0.5) where the actual ~28 ps headline floor (B) was measured. A
confirmed shift at a thinned rung is real evidence that dispersion
matters, but it is not yet a measurement of how much B itself moves.
Closing that is Gap A9b: rerun the direct f=0.5 point (or the full 120 GeV
ladder) with dispersion enabled.

---

## 3. Accuracy gaps, ranked by how much they now matter

The old framing here was "B = 8.2 ps, only ~1.8 ps of headroom." That's
gone (Discovery 11) — the real number is B ≈ 20–33 ps depending on energy, and
estimator, single-photon-time-resolution, and energy fixes are all exhausted (Discovery 14, Discovery 15). Most of Gap A1's
children below are now CLOSED; Gap A9b (does dispersion move the floor
itself) is the one open systematic standing between "~28 ps" and a fully
trusted number — Gap A9's existence and direction are now confirmed
(Discovery 17).

| # | gap | why it matters NOW | cost to close |
|---|---|---|---|
| **Gap A1** | ~~Estimator choice~~ **CLOSED BY ARCHITECTURE, 2026-08-02.** Was: `dTwls` = first photon, a minimum of ~9×10⁵ photons at true light — an extreme-value statistic no real device reproduces. Now: the simulation stores every photon (`phT`/`phId`/`phWls`, always on) and computes an electronics-free **5% constant fraction discriminator quantile** (`dTcfd`) instead — the direct light-level analog of the test-beam's own 5% constant fraction discriminator convention. See `RADiCALsimSIMPLE/README.md` "The light chain". | Was possibly the largest systematic in the whole result. | **Done** — no further run needed; every run from now on produces this automatically. |
| **Gap A1b** | ~~Is the 5% threshold safe?~~ **ANSWERED 2026-08-04 (Discovery 12): yes.** Prompt contamination below the 5% threshold is ≈4% at both 120 and 25 GeV — the constant fraction discriminator times on wavelength-shifted light, not Cherenkov. The transition zone is at 0.5–2%, and it is unstable there — do not lower the fraction into it. | Was the one question that could flip the timing story. | **Done** (`analysis/cfdfrac.C` on stored waveforms). |
| **Gap A1c** | ~~Does B = 8.24 ps survive `dTcfd`?~~ **ANSWERED 2026-08-04 (Discovery 11): no.** True-light 120 GeV under the 5% constant fraction discriminator is ≈28 ps (three independent routes agree). 8.24 ps was the order-statistic artifact. | The headline number changed: light alone no longer meets <10 ps. | **Done** (constant fraction discriminator ladder + Run A). |
| **Gap A1d** | ~~Cherenkov-cluster timing~~ **ANSWERED 2026-08-08 (Discovery 14): mostly no.** Best realizable variant (mean of first ~10 photons) gives 26 ps at 120 GeV and survives single-photon time resolution smearing, but collapses to 140 ps at 25 GeV — energy-fragile, not a foundation. No estimator found below ~26 ps. | The last identified analysis-side path under ~20 ps is closed; <10 ps is now a hardware question. | **Done** (`analysis/sptr_est.C`). |
| **Gap A2** | ~~B(E)?~~ **ANSWERED 2026-08-09 (Discovery 15): B falls with energy, shallowly.** 25 GeV B = 32.8 ± 2.4 ps versus 120 GeV B = 20.4 ± 5.8 ps; B ~ E^−0.15…−0.30. The gap's own stated expectation (B grows with E) was backwards. | **Energy is not a lever** — closing 28→10 ps would need ~3500 GeV, and real running is below 120 GeV, that is, worse. | **Done** (7-rung 25 GeV ladder). |
| **Gap A3** | ~~Single-photon time resolution (~60 ps single-photon jitter)~~ **ANSWERED 2026-08-08 (Discovery 14): harmless to the constant fraction discriminator.** Even 100 ps of single-photon time resolution jitter shifts `dTcfd` by ~1 ps (quantile of ~10³–10⁴ photons). The light-side numbers stand unchanged in front of a real silicon photomultiplier. | The largest feared electronics term costs ~nothing at the trigger level. | **Done** (`analysis/sptr_est.C`). |
| Gap A4 | **Silicon photomultiplier saturation.** ~9×10⁵ photons on 5676 microcells = ~20× oversubscribed at true light. | Kills the true-light *energy* number. Timing is largely immune to the EARLY photons that set a 5% quantile — worth stating explicitly rather than assuming. | code + rerun |
| Gap A5 | **Photon detection efficiency as a function of wavelength flat at 0.36.** DSB uses the real MicroFJ curve. Blue Cherenkov versus green wavelength-shifted light are detected at different efficiencies → changes the population mix that sets where the 5% threshold lands (Gap A1b). | Moderate — shifts the prompt-versus-wavelength-shifted-light balance. | code + rerun |
| Gap A6 | **No photon budget cap.** DSB caps at 4M/event; SIMPLE has none. At f=1, 120 GeV that is ~5×10⁸ photons/event, and now EVERY one of them gets pushed into the waveform vectors (more memory pressure than before, not less). | Practical: an f→1 run may run out of memory, more so now than pre-08-02. Mitigated by choosing f=0.5 (Section 4) and smoke-testing first. | small code change if needed |
| Gap A7 | Single module ≠ 3×3 array (the papers' goal geometry) | **Needs your scope decision.** | large |
| **Gap A9** | ~~No chromatic dispersion~~ **PARTIALLY VALIDATED 2026-08-16 (Discovery 17).** Was: LYSO/DSB1 `RINDEX` flat → Geant4's derived group velocity = c/n for every wavelength, zero chromatic arrival-time spread. Now: rebuilt with Sellmeier curves; a same-configuration comparison at f=0.01, 120 GeV shows **+17% (99.7→116.9 ps)**, direction as predicted, but only 1.1σ. | Confirms dispersion is a real, non-zero, correctly-signed effect on the timing spread. | **Done** — rebuild complete, one-rung comparison measured. |
| **Gap A9b** | **NEW — does dispersion move the ~28 ps floor itself, and by how much?** Discovery 17's +17% shift was measured at f=0.01, deep in the photostatistics-dominated regime — not at the f≥0.5 regime where B actually lives. | The last open systematic standing between "~28 ps" and a fully trusted headline number. | Rerun Run A's direct f=0.5 point (or the full 120 GeV ladder) with dispersion — several hours locally, matching Discovery 17's 500-event statistics. |
| Gap A8 | ~~Fixed 15 mm wavelength-shifting window energy turn-up~~ **CLOSED 2026-08-04:** the center E-type depth correction (ratio NpeCenter/Npe, both real silicon-photomultiplier sums, no truth) takes 120 GeV from 11.67% → **6.03%**; all of 10–120 GeV lands at 6.0–9.5%. Only 5 GeV (13.2%) stays over 10%. | The <10% energy goal is met in simulation. | **Done** (centerE run + `scan.C`'s built-in correction). |

---

## 4. The two-day campaign (2026-08-02 → 08-04) — **✅ COMPLETED 2026-08-04**

**All four runs finished on curiosity and are analyzed. Outcomes:**

| run | result | verdict |
|---|---|---|
| **C** constant fraction discriminator ladder | B = 20.4 ± 5.8 ps, true-light σ_t = 27.1 ± 2.5 ps | 8.24 ps refuted (Discovery 11) |
| **A** 120 GeV f=0.5 | σ_t = 28.2 ± 1.8 ps direct | confirms the ladder |
| **B** 25 GeV f=1.0 | σ_t = 34.5 ± 2.2 ps at TRUE light | most defensible number |
| **D** centerE | σ_E/E 11.67% → 6.03% @120 GeV | **energy goal met** (Gap A8) |

Plus two offline products: the fraction scan (`cfdfrac.C`, Discovery 12) and the
`scan.C` adaptive-binning fix (Discovery 13). The plan below is kept for the record.

**⚠️ CURIOSITY ONLY — perseverence is shut down.** The campaign therefore runs
**serially on one cluster**: ~40 h of work into a ~48 h window, no slack for a
failed run to be re-done. That makes *ordering* the critical decision.

**Run order is by value-per-hour, cheapest-and-most-unblocking first**, so that
if the cluster dies partway we lose the least:

| order | run | hours | cumulative | why this position |
|---|---|---|---|---|
| 0 | smoke test (3 ev) | 0.03 | — | Gap A6 memory risk is untested; 2 min of insurance before committing 14 h |
| 1 | **C** constant-fraction-discriminator-ladder validation | ~8.2 | 8.2 | **Cheapest thing that can invalidate the headline number.** Answers Gap A1c (does B=8.24 ps survive under `dTcfd`?) and Gap A1b (does the precursor cross the 5% threshold?) together |
| 2 | **D** center E-type | 4.7 | 12.9 | Cheap, and the *only* thing blocking the energy goal |
| 3 | **B** 25 GeV ladder | 14.8 | 27.7 | New physics: true light + B(E), now natively on `dTcfd` |
| 4 | **A** 120 GeV f=0.5 | 14.6 | 42.3 | Confirms a number we already have a fit for — most expendable if time runs out |

Sized from measured throughput: **wall seconds ≈ 1.095 × f × N_events × ΣE_GeV**
(fits every rung run so far to ~10%). ~42.3 h into the ~48 h window leaves
~6 h slack — tighter than before (the constant-fraction-discriminator-ladder rerun costs real cluster
time; it isn't free the way the old photon-dump run was), but every run
below now produces the full waveform automatically, so nothing here needs
a special flag anymore.

### Run A — the floor, measured directly [~14.6 h]
**f = 0.5, 120 GeV, 800 events.** At f=0.5 the photostatistics term contributes
only **1.8%** in quadrature — this measures B essentially *directly*, with no
model extrapolation at all. Chosen over f=1.0 deliberately: f=1.0 buys only 1.1
percentage points more (0.7% versus 1.8% excess) for **double** the cost and double
the out-of-memory risk (5×10⁸ versus 2.5×10⁸ photons/event, against no budget cap
— Gap A6).
Now reads `dTcfd` automatically (no flag needed) — this is a second,
independent cross-check of whatever Run C finds.
- **Success:** σ_t(dTcfd) lands close to Run C's fitted B → confirms the floor
  under the realizable estimator, independent of the fit.
- **Failure mode to watch:** if it lands far off, trust the direct point over
  the fit and re-derive B from Run C's rungs alone.
- ⚠️ **Smoke-test first** (3 events, ~2 min) — Gap A6 means this is untested territory.

### Run B — B(25 GeV), and TRUE LIGHT for free [~14.8 h]
**f = 0.3 (1500 ev, 3.4 h) then f = 1.0 (1500 ev, 11.4 h), 25 GeV only.**
At 25 GeV, **f = 1.0 IS true light — zero thinning, zero extrapolation** — and
costs only 11 h because cost scales with E. This is the single most defensible
timing number the project can produce, and — same as every run now — it comes
with the full waveform and `dTcfd` for free, extending the B(E) question (Gap A2)
onto the *validated* estimator rather than the retired one.

### Run C — constant-fraction-discriminator-ladder validation [~8.2 h] ← *best value per hour*
**120 GeV, the same 6 rungs as the original ladder (f = 0.001, 0.003, 0.01,
0.03, 0.1, 0.3), 500 events/rung**, on current (waveform-always-on) code —
no flag needed. This directly answers the two gaps the constant-fraction-discriminator switch opened:
- **Gap A1c:** refit σ_t(dTcfd)·√f across the 6 rungs the same way Discovery 8 did for
  `dTwls`. If B comes out near 8.24 ps, the headline number survives the
  estimator switch; if not, `dTcfd` is now the number that counts.
- **Gap A1b:** at the top two rungs (f=0.1, 0.3 — closest to true light), pull
  the raw `phT`/`phWls` waveform and check directly whether the prompt-
  Cherenkov precursor's peak rate crosses the 5% threshold before the
  wavelength-shifted-light bulk does. This is the one open question that
  could most change the
  timing story, and it needs real photons at high f to answer honestly.
This converts the largest remaining unknown — "does any of this still hold
under a realizable estimator?" — into a single, moderate-cost run.

### Run D — center E-type production sweep [~4.7 h]
**`RADSIMPLE_CENTER_ETYPE=1`, 6 energies, 5000 ev, f=1e-2.** Analysis is
already written (Discovery 7). Gives the silicon-photomultiplier-only depth correction that targets the
high-E energy turn-up (Gap A8) — the last blocker on the <10% goal.

### 4b. The local Mac campaign (2026-08-08 → 08-09) — **✅ DONE for the
question it was launched to answer**

**Both clusters are down until end of August** (perseverence permanently,
curiosity for the month). The Mac is now the only compute: 8 cores, 8 GB
RAM, ~12 GB disk free. Benchmarked at **1.5 core-s per event·GeV at
f=1e-2** (~3.7× faster per core than curiosity), which makes low-f ladder
work entirely feasible locally — an overnight run buys ~230k event·GeV.

**Launched 08-08 evening as a 3-energy serial chain** (25, then 10, then
5 GeV, 5-rung constant fraction discriminator ladders each). **In practice only 25 GeV ran**: the
terminal that launched it was closed without `nohup` protecting the outer
chain, which SIGHUP-killed the whole script mid-way through the 25 GeV
ladder's 5th rung. The single missing rung was relaunched properly
detached and finished 08-09; 10 GeV and 5 GeV were never reached and the
chain was not restarted for them (see below for why).

**Result: the question this was for — B(E), Gap A2 — is answered (Discovery 15).**
Joining the 5 local rungs to the existing f=0.3/f=1.0 Run B points gave a
clean 7-rung turnover: B(25 GeV) = 32.8 ± 2.4 ps, *higher* than 120 GeV's
20.4 ± 5.8 ps. Since B falls as E rises rather than the reverse, 10 GeV and
5 GeV would only confirm an even higher (worse) floor — informative for a
complete published B(E) curve, but not decision-relevant for the <10 ps
question, which is why they're marked deprioritized in Section 1 rather than
relaunched. A genuine analysis-macro bug (Discovery 16) was also caught and fixed
along the way, on the 5-rung 25 GeV data before the rescue rung landed.

Not feasible locally, regardless: true-light or f≥0.3 runs at 120 GeV
(days of wall time, GBs of waveform per run against 12 GB free). Those
wait for curiosity's return in September.

### Offline, free, no cluster (do while runs execute)
- **Position S-curve study** — formalize Discovery 4 into a defensible figure.
- **Pb-glass depth correction** on existing 15k data — corr(Npe, ePbGlass) =
  −0.65; ceiling says ~16.5% → ~11% at 120 GeV.
- **Estimator and single-photon-time-resolution analysis (Gap A3)** on ANY waveform-bearing file as soon as
  it lands — no longer specific to Run C, every run now carries `phT`/`phId`/
  `phWls`. Alternate constant-fraction-discriminator fractions, Nth-photon estimators, and single-photon-time-resolution smearing
  are all `TTree::Draw`-level offline work.

---

## 5. Gap analysis per goal

| goal | status | what closes it |
|---|---|---|
| **<10 ps** | **NOT met by light alone, and now confirmed robust to that conclusion**: ~28–31 ps at 120 GeV (5% constant fraction discriminator, immune to single-photon time resolution smearing per Discovery 14); 35–37 ps at 25 GeV true light, and B *rises* toward lower energy (Discovery 15). The old "MET at 8.2 ps" was the first-photon artifact (Discovery 11); estimator, single-photon-time-resolution, and beam-energy fixes are all exhausted (Discovery 14, Discovery 15). | **Hardware, not analysis**: faster light path — prompt-light collection, thinner/faster wavelength-shifting material, geometry. The simulation's remaining role: dispersion re-check (Gap A9) and testing hardware variants (for example window length, wrap) under `dTcfd`. |
| **<10% σ_E/E** | **MET in simulation (2026-08-04)**: 6.0–9.5% across 10–120 GeV with the center E-type depth correction (Gap A8, Discovery 7). | Done at the simulation level. Remaining: does it survive silicon photomultiplier saturation (Gap A4) and real photon detection efficiency (Gap A5)? |
| **1 mm position** | **MET in simulation** (0.33–0.61 mm, improves with light) | Formalize offline |

---

## 6. Standing discipline

- Fiducial always on; keep% always printed; multi-modality guard armed.
- **Any fit failing its χ² cut must refuse to print derived numbers** (learned
  from a `0.0 ± 3×10⁶ ps` near-miss).
- **Histogram binning must scale with statistics** (Discovery 9 — a fixed 300 bins faked
  a 22.5 ± 42.1 ps result on 82 events).
- Extrapolations must state their statistical model; **order statistics do not
  inherit mean-averaging rules** (Discovery 5).
- Non-standard runs use `RADSIMPLE_OUT_SUBDIR` so they cannot overwrite
  production rungs.
- Estimated completion times come from measured per-cluster constants only.
