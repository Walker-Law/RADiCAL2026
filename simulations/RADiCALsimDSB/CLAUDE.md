# RADiCALsimDSB — Claude/AI Reference Guide

> **VARIANT: DSB1 timing WLS.** This is the fast-scintillator copy of the sim —
> the corner timing fiber is **DSB1** (polysiloxane WLS, ~2 ns decay) to chase the
> RADiCAL paper's ~27 ps σ_t. The sibling `simulations/RADiCALsimLuAG/` is the
> identical geometry with the **LuAG:Ce** fiber (60 ns decay, ~52 ps σ_t floor).
> Only the corner WLS material differs between the two. Decay is tunable at
> runtime via `RADICAL_DSB_DECAY_NS` (default 2.0).

> Read this first. It captures everything needed to work on this project without
> re-deriving context. Keep it updated when geometry, materials, or workflow change.

## ★ PAPER TIMING REPRODUCED (July 22 2026, run `optical_scan_1000_paperJ`)

**σ_t = 247.9 ps/√E ⊕ 18.7 ps  vs  paper 256 ps/√E ⊕ 17.5 ps** (stochastic −3.2%,
constant +6.9%; all six points 25–150 GeV within 10%). Estimator = H1[34]
DeltaT_CFD_4c_Scint_DRS4: per-event 4-capillary mean of 5% CFD (DW−UP), scint
light, ÷2 corner trick, + DRS4 timebase (50 ps/cell) + per-channel amplifier
jitter (`RADICAL_ELEC_JITTER_PS=50` → 50/(2√2) ≈ 17.7 ps on the mean).
Config: `LYSO_SCINT_SCALE=1e-2` (unscaled DSB1 self-scint), `QUARTZ_CHER_KEEP=0.01`,
`SPTR_PS=60`, `SIPM_NPIX=5676`, `SM_COG_CUT_MM=2`, energies 25/50/75/100/125/150,
1000 evt/E. Curves: `optical_scan_1000_paperJ/resolution_curves.root`
(`TimingResolution_4cScintDRS4_paperMatched`), per-energy fits in `build/plots/fits/`.

**DO NOT chase the paper with global light-yield cuts** (re-confirmed the hard
way, run `paperLY`): cutting LY ×36 to "25 npe/MeV" blew σ_t up 15×
(3830 ps/√E ⊕ 223 — pure counting jitter). The sim reproduces the paper's TIMING
at FULL modeled light; the LY-starved config instead reproduces the paper's
Fig 17 ENERGY *shape* (12.61% ⊕ 88.2%/√E vs paper 9.31 ⊕ 52.04/√E — stochastic
term appears only when photostat-limited). No single LY does both → our
per-photon timing is pessimistic (SPR τ_f=3 ns vs data pulse FWHM 8.3 vs sim
~17–19 ns; LYSO 36 ns gating) and compensates with photon count. OPEN: shower-max
energy shape at full LY is FLAT (~13% all E); COG cut (2 mm) helped the constant
(15.9→13.2%) but not the shape. Next lever: LY ≈ 3e-3 + tighter COG for a
dedicated Fig 17 run (88.2×√(1.1e-3/3e-3) ≈ 53%/√E ≈ paper's 52), timing quoted
from paperJ.

**ROOT-CAUSE HYPOTHESIS for the two-config split (2026-07-22, per Walker's
question "how can both be optimized in the real experiment?"):** in the real
detector there is exactly ONE physical light yield, and it sets BOTH the timing
and energy stochastic terms simultaneously (both are 1/sqrt(N_pe) of the same N).
Our sim needing DIFFERENT LY (1e-2 for timing, 3e-3 for energy — a ~3.3x gap) is
therefore not "two aspects of physics at different scales" but a real miscalibration:
our conversion from photons -> timing precision is worse than reality's, so timing
compensates with extra light. Quantitatively: closing a 3.3x LY gap via sharper
per-photon timing needs ~sqrt(3.3) = 1.8x better precision per photon. That
matches, independently, the ALREADY-DOCUMENTED pulse-shape defect: emulated pulse
FWHM ~17-19 ns vs the real data's measured 8.3 ns (~2x too broad). Both numbers
pointing the same way and same rough magnitude is evidence, not coincidence.
**Prime suspect: the hardcoded single-photon-response time constants**
`tauR=1.0, tauF=3.0` (ns) in BOTH `pulseCFD()` and `leadingEdgeFixed()`
(`EventAction.cc:94,153`) — never tuned to the measured 8.3 ns FWHM, and not
env-configurable. **Proposed fix (not yet implemented — deferred by user
2026-07-22):** make tauR/tauF runtime knobs (e.g. `RADICAL_SPR_TAU_R_NS`,
`RADICAL_SPR_TAU_F_NS`, defaults unchanged), tune tauF down (~1.5 ns) until the
emulated FWHM matches 8.3 ns, then re-search for the SINGLE light yield where
BOTH the timing law (256/√E ⊕ 17.5) and the Fig17 energy law (52.04/√E ⊕ 9.31)
hold at once — that unified result, not two separately-tuned configs, is the
physically honest reproduction. Until this is done, treat `paperJ` (timing) and
the planned `fig17` LY=3e-3 run (energy shape) as two DIAGNOSTIC configs, not a
final result.

## ★★★ WAVEFORM-REALISM CHAIN (2026-07-23) — photons -> mV -> noise -> DRS4 -> CFD

The timing estimators no longer run on idealized photon times. `pulseCFD()` /
`leadingEdgeFixed()` (EventAction.cc) now build each SiPM channel's pulse in
PHYSICAL mV at TRUE-light amplitude scale and digitize it before the 5% CFD —
the same processing the test-beam data received. `RADICAL_WFM_REALISM=0`
restores the legacy path (how everything before 2026-07-23 ran).

The chain (all env-tunable):
1. Each detected (thinned) photon carries peScale = 1/LYSO_SCINT_SCALE true pe
   (override `RADICAL_WFM_PE_SCALE`) -> noise/saturation/quantization enter at
   REAL relative size; only photon granularity remains a thinning artifact
   (exactly what the scale ladder extrapolates away).
2. Time-ordered SiPM microcell saturation (`RADICAL_SIPM_NPIX`=5676, optional
   recovery `RADICAL_SIPM_TAU_REC_NS`, default 0). NOTE: at the predicted
   ~90 npe/MeV true light, 25 GeV puts ~33k pe on 5676 cells — heavily
   saturated. **The data's LINEAR E_meas vs E (paper Fig 17-left) is therefore
   IN TENSION with our light prediction by ~1-2 orders of magnitude — an
   independent clue the sim over-collects light** (see capture fraction below).
3. SPR per pe x `RADICAL_SPE_MV` (default 0.25 mV/pe, high-gain).
4. CR high-pass (AC-coupled amp), `RADICAL_AMP_TAU_D_NS` (default 12):
   EMPIRICALLY tuned so the summed pulse FWHM matches the DATA's 8.3 ns
   (smoke: tauD 2->4.7ns, 5->6.4, 8->7.4, 12->8.55). Unshaped the WLS pulse is
   ~38 ns wide (LYSO 36 ns gating) — the old path could never match the data's
   pulse shape; H1[23] PulseFWHM validates.
5. White noise `RADICAL_ELEC_NOISE_MV` (default 0.5 mV/sample) -> the real
   noise-over-slope timing term now exists. `RADICAL_ELEC_JITTER_PS` (ad-hoc,
   scint-only estimators) should be left UNSET now — double-counting.
6. DRS4 digitization: clip at `RADICAL_DRS4_VMAX_MV` (830, the observed data
   rail), quantize `RADICAL_DRS4_BITS` (12) over 1 V.
   Diagnostics: H1[38] PulsePeak_mV, H1[39] PulseSat (mean = clipped fraction;
   DATA anchors: 3.8% @25 GeV, 74% @150 GeV — tune SPE_MV against these).
   H1[40] PhotonsWLSEmitted: <PhotonsWLS>/<H1[40]> = capture x transport x PDE.
   Smoke measured **10.7%** vs ~3% naive (bare-fiber TIR 5.4%/end x PDE) —
   part legitimate (annulus/capillary guiding, PD spans full OD), but a prime
   suspect for the light over-prediction. INVESTIGATE.
7. HG threshold is physical: `RADICAL_HG_THR_MV` (default = 2.5 detected-pe
   equivalents x peScale x SPE_MV).

Also fixed: `setup_env.sh` now validates the geant4-config --datasets paths and
remaps them onto the actual install prefix (the 2026-07-22 reorg moved the
install; baked-in absolute paths pointed at the old location ->
"ENSDFSTATE.dat is not found" on local Mac runs).

**Ladder on this chain — RESULT (2026-07-24): moved to `../RADiCALsimLadder/`.**
The scale ladder (f in {0.1,0.3,1,3} coherent) now lives in its own study folder
(`simulations/RADiCALsimLadder/`: run_ladder.sh + analysis/ladder.C + README).
Headline: extrapolated true-light a ~ 958 ps/sqrt(E), **3.7x the paper's 256** —
but this is a SYMPTOM, not a result. The run's own diagnostics show the sim
**over-collects light ~3-4x** (WLS capture*PDE ~11% vs ~3% physical: H1[40]
PhotonsWLSEmitted vs H1[30] PhotonsWLS), which **over-saturates the SiPM** (H1[36]
fired-pixel energy sags with E, contradicting the paper's LINEAR Fig 17) and
that saturation is what inflates the timing floor B. **Root cause = light
over-collection, upstream of timing.** Fix the fiber capture fraction (surface
roughness / trapping model / PDE+coupling audit) to ~3% -> should linearize the
energy response AND release the timing floor; THEN rerun the ladder. See
`../RADiCALsimLadder/README.md`.

## ★★★ paperR RESULT (2026-07-23, `optical_scan_500_paperR`) — realistic composition makes timing 6-8x WORSE, in the honest direction

500 evt x 6 E, paper183 geometry + `SCINT_YIELD=0.01` (coherent thinning) +
PDE(lambda) + `MAX_OPT_PHOTONS=20e6`. All three intended effects confirmed:

1. **Composition fixed.** Self-scint fell 70% -> ~0% of scint light; WLS is now
   ~100% of the timing population (the realistic mix). Verified per-energy.
2. **Truncation cured.** PhWLS climbs linearly to 15743 @ 150 GeV (was plateaued
   at ~9500 in the 4M-capped 1000-evt paperR). No high-E saturation.
3. **Energy path untouched.** EnergyResolution a=15.6% b=2.56% (paper183: 15.3/2.64)
   — the composition/PDE change did not disturb the energy observable, as expected.

**Headline all-light + DRS4 timing (H1: DeltaT_CFD_4c_DRS4):**

    E [GeV]     25   50   75  100  125  150     fit a / b
    paper183    38   26   21   19   18   18     a=170  b=10   (self-scint dominated)
    paperR     318  236  227  191  192  183     a=1402 b=142  (WLS dominated, REALISTIC)
    PAPER       54  ...  ...  ...  ...   27     a=256  b=17.5

paper183's beautiful 18-38 ps was an **artifact of the composition bug**: 70% of
its timing light was prompt DSB1 self-scint (tau=3.5 ns), a population that is ~2%
in the real device. Remove it and the light is WLS, which inherits LYSO's 36 ns
decay before the 3.5 ns re-emission -> slow leading edge -> 6-8x worse per-photon
timing. **This is the correct direction** (we knew paper183 was too optimistic).

**But paperR CANNOT be compared to the paper's 256/sqrt(E) at face value** — it is
100x photon-thinned, so its a=1402 is dominated by photostatistics, not the
physical resolution. 1402/sqrt(100) ~ 140 ps/sqrt(E) if that were pure photostat,
but b=142 (the thinned "floor") is also photostat-inflated. The ONLY honest way to
get the real-light number from this composition is the **scale ladder** — coherently
thin all light knobs by f, fit a^2(f)=A^2/f+B^2, extrapolate to true light. A single
thinned point does not decide it. Prior ladders were run on the WRONG (self-scint)
composition and must be redone here.

**NEXT (clean, no tuning): scale ladder on the paperR composition.** Coherently
scale `LYSO_SCINT_SCALE`, `SCINT_YIELD`, `QUARTZ_CHER_KEEP` by f (base = paperR),
same geometry, `MAX_OPT_PHOTONS=20e6` throughout. That measures A (photostat) and
B (light-independent floor) for the realistic WLS-dominated timing, and its
extrapolation to true light is the honest number to put beside 256/sqrt(E).

**Minor anomaly to check:** WLS-only estimator is WORSE in paperR (a=600) than
paper183 (a=386) despite MORE WLS light — investigate (500 vs 1000 evt stats on a
first-photon estimator? SPTR interaction?); it is not the headline (real SiPM sees
all light) so it does not block the ladder.

## ★ DECISIONS LOCKED (2026-07-22, per Walker) + REALISTIC-COMPOSITION RUN `paperR`

Walker's rulings on the reproduce-the-paper option list — do not re-litigate:
- **Keep the 5% CFD estimator.** No switch to the paper's fixed absolute
  threshold unless it becomes an obvious problem later.
- **No origin-gated headline estimators.** A real SiPM detects whatever light
  arrives, weighted by its PDE(lambda) — it cannot tag photon provenance. So the
  headline is the ALL-light estimator, and realism must come from making the
  light that ARRIVES realistic, not from gating detection.
- **Keep MicroFJ-30035** (not Hamamatsu HDR2) for now.
- Hole-wall TIR back into LYSO: believed fine as modeled (dropped).
- Fused rod-joint losses: negligible for our purposes (dropped).
- No email to the collaboration asking for measured npe — they don't know it
  with certainty either.
- Stay at 1000 evt/E for speed (error bars understood: ±2σ core fit keeps
  ~37% effective statistics; COG cut keeps 27% of events for shower-max).

Two code changes implement the "realistic light" directive:
1. **PDE(lambda) replaces flat kQE=0.36** (`EventAction.hh::sipmPDE`,
   photon energy now passed from SteppingAction). MicroFJ-30035 curve digitized
   from the datasheet, anchored at 50% @ 420 nm peak / 36% @ 495 nm (the two
   values the old comment already cited). Blue light (LYSO-direct, Cherenkov)
   now detected at up to 50%, red tail less. `RADICAL_SIPM_QE=0.36` restores
   the old flat behavior for reproducing pre-2026-07-22 runs.
2. **`TimingResolution_4cAllDRS4`** added to `scan_resolution.C` — fits the
   ALL-light + electronics histogram `DeltaT_CFD_4c_DRS4` that was filled but
   never analyzed. Baseline on paper183 (WRONG mix, see below):
   a = 170.5 ± 5.4, b = 10.02 ± 1.02 (chi2/ndf 12.2/4).

**The `paperR` run — realistic composition.** The composition bug (70% of scint
light = DSB1 self-scint, ~2% in reality) is a THINNING ASYMMETRY: LYSO chain
x1e-2, Cherenkov x1e-2 (KEEP=0.01), but self-scint x1 (unthinned). Fix = thin
all three coherently: add `RADICAL_SCINT_YIELD=0.01` to the paper183 config.
Then the all-light estimator sees the physical mix at 100x-thinned intensity,
and s (the coherent thinning) is a clean single light-lever on the NEW geometry
— which also de-confounds the ladder (B=43.5 was measured across a geometry
change). paperR vs paper183 differs by (self-scint thinning + PDE(lambda))
only; geometry identical.

## ★★★ paper183 RESULT (2026-07-22) — the ladder's missing point, and the gap to the paper WIDENS

Run `optical_scan_1000_paper183`: paper-fidelity geometry (183 mm rods, NO center
capillary, SiPM coupling fixed), 1000 evt x 6 energies, `LYSO_SCINT_SCALE=1e-2`
(identical to paperJ). Both geometry changes verified in the data:
`CenterCapEnergy` integral = **0** (capillary gone), `PhotonsWLS` **+85%**.

    estimator: TimingResolution_4cScintDRS4_paperMatched  (H1[34])
    run        a [ps/sqrt(E)]   b [ps]    chi2/ndf
    paperJ         247.9         18.71    21.04/4   <- POOR fit (5.3)
    paper183       174.5         20.42     6.40/4   <- good fit (1.6)
    PAPER          256.0         17.50

**The constant term b is unchanged (18.7 -> 20.4 ps, vs paper 17.5)** — expected,
it is DRS4-timebase + amplifier dominated and no geometry change touches it. The
(DW-UP) common-mode cancellation still works. **All the movement is in a.**

Note also that paperJ's headline "247.9 vs 256" rested on a fit with
**chi2/ndf = 5.3** — the six points did not actually follow the sqrt law. The
paper183 fit is chi2/ndf = 1.6. The new number is the more trustworthy one even
before interpretation.

### paper183 IS the ladder point that was missing

The killed f=3 ladder point was meant to pin the intercept B. paper183 supplies
it: measured light ratio vs paperJ = **1.867**, i.e. x = 1/f = 0.536, sitting
exactly in the x-gap between paperJ (x=1) and x=0.

    fit  a^2 = A^2/f + B^2   (f from MEASURED <PhotonsWLS>, not nominal knobs)
    3 old-geometry points   A = 236.6   B = 88.2   chi2/ndf = 1.25/1
    ALL 4 incl. paper183    A = 239.2   B = 43.5   chi2/ndf = 3.48/2

**A is rock-stable (236.6 -> 239.2); B HALVES (88.2 -> 43.5.)** Exactly as warned
— B was the loose extrapolated intercept, and the first point with real leverage
on it moved it a lot. **CONFOUND: paper183 changed geometry as well as light, so
B=43.5 is not a clean single-variable measurement.** Longer rods and the removed
center capillary could each move timing independently of photon count.

### Consequence: the sim is now TOO GOOD, not too noisy

Predicted true light yield rises **50 -> 91 npe/MeV** (thinning undone; stable
85-99 across 25-150 GeV). Extrapolating with A=239.2, B=43.5:

    f=1.87 (as run, 100x thinned)  a = 180 ps/sqrt(E)   [measured 174.5 - consistent]
    f=10                          a =  87
    f=50                          a =  55
    f=186.7 (TRUE, unthinned)     a =  47 ps/sqrt(E)
    PAPER                             256 ps/sqrt(E)

At realistic light the sim predicts sigma_t **~5.5x BETTER than the paper measured.**
The earlier hope — that the residual was a light-independent shower-sampling floor
which would survive extrapolation — is **weakened**, because the floor B went DOWN
when the constraining point arrived. There is no large light-independent term
hiding here. So: **the real detector is degraded by something we do not model**
(candidates: the pulse-shape defect already documented — emulated FWHM 17-19 ns vs
measured 8.3 ns; SiPM saturation/nonlinearity; beam momentum spread; MCP reference
resolution folded into the paper's quoted number).

**Do NOT read paper183's 174.5 as "closer to 256 = better."** It is closer only
because it is still ~100x photon-starved. The honest statement is that the sim's
timing model, extrapolated to its own predicted light yield, does not reproduce
the paper's stochastic term and is too optimistic by ~5x.

**Next measurement (clean, no tuning):** run one reduced-light point ON THE NEW
GEOMETRY (e.g. `RADICAL_LYSO_SCINT_SCALE=3e-3`, everything else = paper183). That
gives A and B within a single geometry and removes the confound above.

## ★★ SCALE-LADDER RESULT + SiPM COUPLING FIX (2026-07-22) — superseded in part by the paper183 result above

### 1. The ladder measured it: paperJ was ~90% photostatistics, NOT a reproduction

Ran f = 0.1, 0.3, 1 (f=1 = `paperJ`; verified paperJ sits on the light-scaling
line — PhotonsScint ratios 1.00 / 2.98 / 9.44 vs expected 1 / 3 / 10, so paperJ
is a legitimate f=1 point to ~6%). The f=3 point was killed before merging.

    f      a(f) [ps/sqrt(E)]    b(f) [ps]
    0.1        740.2              0.0
    0.3        440.2              0.0
    1.0        247.6             18.7      <- paperJ

    a^2(f) = A^2/f + B^2  ->  A = 233.6,  B = 93.7 ps/sqrt(E)   chi2/ndf = 0.49/1

Textbook 1/sqrt(f) scaling (740/440 = 1.68, 440/248 = 1.77, both ~ sqrt(3) = 1.73).
So at f=1 the headline 247.6 was **A=234 photostatistics (+) B=94 floor — i.e.
~90% photon-counting noise from running 100x thinned.** Extrapolated to true
light (f~100): **a ~ 97 ps/sqrt(E) vs the paper's 256.** The paperJ "agreement"
was two errors cancelling, exactly as feared. NEITHER branch of the decision rule
won outright: B does not dominate (can't call paperJ legitimate) but the light
model isn't off by a clean 10x either. The residual now sits in the
**light-INDEPENDENT floor** = shower-sampling physics, which no light-yield change
can fix. CAVEAT: 3 points, 1 dof, and B is an *intercept extrapolation* from
x=1/f in [1,10] down to x=0 — B is the loose parameter and it is the one now
carrying the physics. The f=3 point (x=0.33) is what would pin it.

### 2. SiPM COUPLING BUG in ALL pre-183mm runs — they lost ~half their light

The old geometry placed the PD `wrapThick` = 0.2032 mm of **AIR** beyond the
quartz rod end. That is not a small Fresnel loss — it is a **total-internal-
reflection trap**: quartz->air has critical angle arcsin(1/1.46) = 43.2 deg, and
light guided by TIR in the rod arrives at the end face at angles up to almost
exactly that, so the highest-angle (large) fraction of the guided light was
reflected BACK into the rod instead of reaching the SiPM.

The 183 mm rebuild fixed this incidentally: the PD now abuts the rod tip exactly
(both faces at capEndZ = 91.5 mm, zero gap) and pdRINDEX = 1.50 vs quartz ~1.46,
so Fresnel at that boundary is ~0.02%. Equivalent to `geant4-DURU-models`'
deliberate `couplingGap = 0.0*mm` ("direct optical contact: no vacuum gap, so
guided"). Measured at 50 GeV: **2.1x more PhotonsWLS, 2.4x more PhotonsDetected**
(2-event smoke; part of the "detected" gain is extra Cherenkov from the longer
rods, but PhotonsWLS is comparatively clean).

**Consequence for the ladder:** every pre-183mm run — paperJ AND all ladder
points — was discarding ~half its light at the exit face. So the sim's predicted
true light yield rises ~50 -> **~100 npe/MeV**, and extrapolating photostatistics
to MORE light pushes predicted sigma_t **further BELOW** the paper's 256, not
closer. This strengthens (not overturns) the ladder conclusion: the gap to the
paper is shower-sampling/geometric, not photon counting.

**Comparing `paper183` to `paperJ` folds THREE changes at once** (183 mm rods,
center capillary removed, ~2x coupling gain). Attribute carefully; do not credit
any shift to capillary length alone.

## ★★ LIGHT YIELD IS A PREDICTION, NOT A KNOB (2026-07-22) — READ BEFORE TUNING

Prompted by Walker asking "what is the most realistic LY value? why does it sound
like we have to manually find it?" — he was right, and the answer reframes the
whole timing comparison.

**1. `RADICAL_LYSO_SCINT_SCALE` is a MONTE CARLO THINNING FACTOR, not physics.**
Full LYSO yield is ~5e8 optical photons/event at 120 GeV — untrackable, so the
sim thins. Setting scale=1e-2 does NOT mean "the detector has 1% light"; it means
we track 1% of the photons and **must correct analytically**. The documented rule
(already in this file) is: *the emission-jitter part of σ_t scales as √scale;
geometric floors don't.* Tuning the scale until output matches the paper is
fitting the answer — do not do it.

**2. The sim already PREDICTS the light yield: ~50 npe/MeV.** Measured from
`optical_scan_1000_paperJ` by undoing the thinning (`PhotonsWLS`/`TotalLYSO`/s):
46–54 npe/MeV, stable across 25–150 GeV (a good self-consistency sign). It falls
out of LYSO 33200 ph/MeV (datasheet) + the sim's own computed transport/trapping
+ MicroFJ-30035 PDE 36% (datasheet). Nothing tuned. NOTE: the "paper's ~25
npe/MeV" figure that appears in older notes is **not in the paper** — the paper
quotes no measured light yield (verified 2026-07-21). Don't calibrate to it.

**3. This makes the `paperJ` "reproduction" SUSPECT.** At scale 1e-2 we detect
**0.5 npe/MeV** vs the predicted real **~50** → we run **100× photon-starved**,
so our photostatistics are √100 = **10× worse than reality**. The 247.9 ps/√E
that matched the paper was obtained under artificially degraded statistics. If
that stochastic term were pure photostatistics, extrapolating to true light gives
247.9·√(1e-2) ≈ **25 ps/√E — 10× BELOW the paper's 256**. So either the paper's
stochastic term is NOT photostatistics (i.e. it is shower-sampling, which is
light-independent and would not extrapolate), or our light-collection model is
~10× off. **The apparent agreement may be two errors cancelling.**

**4. COMPOSITION BUG (found while designing the ladder): 70% of the timing light
is the wrong population.** At paperJ, 150 GeV: `PhotonsScint`(cat1+2)=27486 vs
`PhotonsWLS`(cat2)=8372 → self-scint (cat 1) is **70%** of the scint-only timing
population. Cause: `RADICAL_LYSO_SCINT_SCALE` thins ONLY the LYSO→WLS chain
(cat 2) while `RADICAL_SCINT_YIELD`=1.0 leaves DSB1 self-scintillation
**unthinned**. Unthinned on both, WLS would be ~100× larger → reality is ~98%
WLS / ~2% self-scint. So our headline timing observable is currently driven by a
population that is negligible in the real device, and the two populations have
different time profiles (self-scint prompt ~3.5 ns; WLS gated by LYSO's 36 ns).
**Fix this before quoting any timing number as final.**

### The scale ladder — the measurement that decides it (no tuning)

`run_scale_ladder.sh` + `analysis/plot_scale_ladder.C` (added 2026-07-22).
Decomposes the stochastic term into light-dependent and light-independent parts:

    a²(f) = A²/f + B²      (LINEAR in x = 1/f: slope = A², intercept = B²)

- `f` = coherent light multiplier, f=1 ≡ the paperJ config.
- **Both yield knobs are scaled together** (`LYSO_SCINT_SCALE`=1e-2·f AND
  `SCINT_YIELD`=1.0·f). Moving only one would change the light COMPOSITION as
  well as the total (see item 4) and confound the fit. Everything else is pinned
  to paperJ so f is the only variable; f=1 is re-run, not reused, so all points
  share one binary.
- Ladder: f = 0.1, 0.3, 1, 3 (30× lever). f=3 is the expensive point (~60M
  photons/event at 150 GeV → `MAX_OPT_PHOTONS=80M`); drop it via
  `RADICAL_LADDER_FACTORS="0.1 0.3 1"` if wall time is tight.

**Decision rule:**
- **B dominates** → the paper's 256 ps/√E is shower-SAMPLING physics, light yield
  is nearly irrelevant to timing, and the paperJ agreement was essentially real.
- **A/10 dominates** → extrapolation to true light gives ~25 ps/√E, 10× under the
  paper ⇒ our light-collection model is the bug, and paperJ was a coincidence.

Either outcome is a measured number rather than a fitted knob. Run:
`setsid nohup bash run_scale_ladder.sh 1000 > dsb_ladder.log 2>&1 &` then
`root -l -b -q 'analysis/plot_scale_ladder.C(1000)'`.

## CURRENT STATE (July 2026) — timing puzzle SOLVED, validation run in flight

**UPDATE (July 8 2026) — data-matched estimators added for direct test-beam
comparison.** Three new histograms mirror the `RADiCAL/` data pipeline's
construction so sim and real data land on the same axes: **H1[31] DeltaT_CFD_4c**
(per-event **4-corner-averaged** 5% CFD ΔT, all light) and **H1[32]
DeltaT_CFD_4c_Scint** (same, scint-only — the headline sim↔data timing number)
replicate test-beam Method A `(mean{DW}−mean{UP})/2` from
`RADiCAL/Analysis/timingEnergyBins.C`. The old per-corner H1[6]/H1[22]/H1[25]
lack the √4 per-event averaging, so they overstate σ_t by ~2×. **H1[33]
Npe_Scint_veto** is the fiber-light `sum_lg` energy analog (Pb-glass as *veto*,
not the ECombined *add-back*). Overlay vs data+paper: `analysis/compare_sim_data.C`.
The full comparison needs a fresh OPTICAL scan with the rebuilt binary (the
in-flight 20k run predates these histograms). Systematic method diff:
`DATA_VS_SIM_SYSTEMATICS.md`.

**H1[34] DeltaT_CFD_4c_Scint_DRS4** = H1[32] + a **datasheet-grounded DRS4
uncalibrated-timebase** residual — the honest data-comparison timing. CAEN DT5742
/ PSI DRS4 nominal (uncalibrated) axis: cell width deviates from 0.2 ns by up to
±100 ps. Within a DRS4 group the common accumulated error cancels in (DW−UP); the
residual is the differential width error over the ~1 cell between the down/up
crossings, modeled per corner as Gaussian σ = σ_cell·√(|ΔT|/0.2 ns).
`RADICAL_DRS4_CELL_PS` sets σ_cell (per-cell RMS, ps; default **50**, ≈ half the
±100 ps max; set 0 → H1[34]==H1[32]). This is a grounded approximation, NOT tuned
to the data's 35 ps floor — it is expected to UNDER-shoot 35 ps, quantifying how
much of the real floor is DRS4 timebase vs amplifier/system. Electronic noise
(~0.5 mV) + 1 Vpp saturation + 12-bit quant are second-order for the 5% CFD
leading edge and not yet added (would need a photons→mV gain calibration).

**CORRECTION (2026-07-09, per Walker): the real RADiCAL capillary is SOLID and DOES
produce Cherenkov — it is NOT a thin-wall hollow capillary.** The sim already models
solid quartz rods (`DetectorConstruction.cc:472-486`: upstream/downstream are solid
quartz `G4Tubs`, no air bore), so the geometry needs no change. The real mechanism
for the all-light floor is the **light-yield ratio, not the geometry**: the sim
scales LYSO/WLS scint down 1000× (`RADICAL_LYSO_SCINT_SCALE=1e-3`) while quartz
Cherenkov runs at the full physical rate, **inverting the real ~1000:1
scint:Cherenkov ratio** (`StackingAction.cc:8-11`). In the real detector the WLS
light overwhelms Cherenkov, so the CFD leading edge is WLS-set → good timing. So
Cherenkov IS real and the SiPM DOES see it; scint-only is a tractable **proxy** for
the WLS-dominated regime, not evidence Cherenkov is absent. Faithful headline =
all-light at realistic yield; all-light at scaled yield is misleading. **Open test:**
raise `RADICAL_LYSO_SCINT_SCALE` and confirm all-light σ_t converges to the WLS value.

**YIELD-SWEEP POSTMORTEM + Cherenkov-thinning knob (2026-07-09).** The
LYSO-yield sweep (3e-3/1e-2/3e-2, 2 energies, 2000 evt) was INVALIDATED above
3e-3 by `RADICAL_MAX_OPT_PHOTONS` (StackingAction, default 4M/event): at 150 GeV
generated photons hit ~6.8M (1e-2) and ~20M (3e-2), so stacking silently killed
the excess — detected N_pe SATURATED (~6k/evt, energy-independent), OpWLS froze
at ~2810, scale-independent Cherenkov/self-scint fell 4-5×, and σ_t blew up
(~199 ps artifacts). Diagnostic signature to watch for: N_scint NOT scaling with
the yield knob + energy-independent N_pe saturation. Data parked in
`build/scan/yield_sweep_2000/` (only 3e-3 is valid; shows no convergence — 10:1
detected scint:cher still lets PROMPT Cherenkov set the 5% CFD edge; real ratio
is ~1000:1). **Consequence:** scaling WLS *up* is intractable (20M photons/evt);
instead use the new `RADICAL_QUARTZ_CHER_KEEP` knob (StackingAction, default
1.0): binomial thinning of quartz/fiber Cherenkov at stacking ≡ reducing
Cherenkov yield, restoring the REAL ratio at LOWER cost. KEEP=0.01 ≈ real
~860:1. Also fixed: kill decisions now precede the budget increment (LYSO
Cherenkov no longer wastes ~15% of the budget).

**Key finding (reframed):** the sim's ~46-52 ps all-light σ_t floor (vs paper's 17.5
ps) is prompt quartz-rod Cherenkov **dominating the leading edge because WLS is
suppressed 1000×**. Prompt Cherenkov born along the whole rod imprints shower-depth
fluctuations on ΔT; not common-mode, so (DW−UP)/2 can't cancel it. Process-tagged
scoring — scint-only timing gives **90.1 ps/√E ⊕ 17.1 ps vs paper's 256 ps/√E ⊕ 17.5
ps** (constant term matches to 0.4 ps), i.e. the WLS-dominated value the real device
achieves. Stochastic differs because sim detects ~373 pe/MeV-in-fiber vs paper's ~25
npe/MeV → `RADICAL_SCINT_YIELD=0.07` scales it to match.
New histograms: H1[24] DeltaT_Scint, H1[25] DeltaT_CFD_Scint, H1[26] PhotonsScint,
H1[27] PhotonsCher, H1[28] EShowerMax (LYSO layers 8-10 = WLS window slice —
the analog of paper Fig 17 right, ~10% σ/E; full-module ECombined is 2.7% and
fiber dE/dx is ~95%, neither comparable to Fig 17).

**In flight on curiosity** (launched July 2026): 20k evt/E, 7 energies,
`MAXSTEP=5000 TMAX=50 SCINT_YIELD=0.07` → `dsb_20k_paper.log`, ~10 hr. Expect:
scint-only σ_t curve ≈ paper's 256/√E ⊕ 17.5, and showermax_resolution.png vs
paper Fig 17 overlay. Sync from Mac:
`rsync -avz -e "ssh -p 10022" wlaw@172.16.17.188:~/RADiCAL2026/simulations/RADiCALsimDSB/build/scan/optical_scan_20000/ ~/Research/simulations/RADiCALsimDSB/build/scan/optical_scan_20000/`
then `root -l -b -q 'analysis/scan_resolution.C("build/scan/optical_scan_20000","optical")'`.

**Workflow guardrails added to run_scan.sh:** pre-flight smoke test (catches
missing binary / wrong conda env — must be `conda activate g4` on curiosity, NOT
base), inline chunk-log dump on merge failure, exit-before-analysis on any failed
energy, RADICAL_ENERGIES env override, no default step cap.

**LYSO OPTICALLY ACTIVE (July 2026):** full realistic chain now implemented —
LYSO 420 nm scint (33200 ph/MeV × `RADICAL_LYSO_SCINT_SCALE`, default 1e-3;
36 ns decay, n=1.81) → DSB1 425 nm WLS absorption → 495 nm OpWLS re-emission →
PDs. Detected photons are tagged 3 ways at the PD: Cherenkov / fiber self-scint
/ OpWLS (H1[29] DeltaT_WLS + H1[30] PhotonsWLS = the realistic-chain estimator;
"scint" bucket = self-scint + OpWLS). LYSO-born Cherenkov is killed in
StackingAction (real ratio ~0.1% of scint; would unphysically dominate at scaled
yield — `RADICAL_KEEP_LYSO_CHER=1` to keep). Photostatistics extrapolation:
emission-jitter part of σ_t scales as √scale; geometric floors don't.
NOTE (yield-scaling lesson from the 20k `SCINT_YIELD=0.07` run): scaling yields
down re-introduces first-photon counting jitter that inflates the fitted
"constant" term (52.7 ps vs 17.1 ps unscaled) — compare populations at matched
statistics, don't chase the paper's stochastic term with global yield knobs.

## What this is

Geant4 (v11.4.0, FTFP_BERT) replication of the **RADiCAL** (Radiation-hard
Innovative Calorimeter) shashlik module. Primary geometry reference:
**arXiv:2401.01747** (NIM A 1068 (2024) 169737, Perez-Lara et al.) and
**arXiv:2303.05580** (earlier beam-test results, Beresovskyi et al.). Full
calorimetry simulation: 120 GeV electrons into a LYSO/W sampling stack with
embedded quartz capillaries for energy (EJ309) and timing (DSB1 WLS) readout.

- **Repo:** GitHub `Walker-Law/RADiCAL2026` (public). Git root is `/Users/macro-2/Research`.
- **Project dir:** `/Users/macro-2/Research/RADiCAL2026/simulations/RADiCALsimDSB/`
- **Sibling:** `simulations/firstsim/` is an earlier prototype — reference only, not the active sim.

## Directory layout

```
RADiCALsimDSB/
  CMakeLists.txt
  setup_env.sh          # sources Geant4 + sets all 12 data paths (USE THIS)
  vis.mac               # geometry-only viewer (NO beamOn)
  run_batch.mac         # /run/initialize + printProgress + beamOn N
  src/
    DetectorConstruction.cc   # ALL geometry, materials, vis attributes
    PrimaryGeneratorAction.cc # beam: 120 GeV e-, Gaussian sigma=2.9mm, z=-100mm
    EventAction.cc            # per-event accumulation + fills all histograms
    SteppingAction.cc         # routes edep by volume name to EventAction
    RunAction.cc              # defines all histograms, opens/writes radical_output.root
  include/                    # matching headers
  build/                      # cmake build dir; radical_output.root lands here
```

## Build & run

```bash
cd /Users/macro-2/Research/RADiCAL2026/simulations/RADiCALsimDSB/build
source /Users/macro-2/Research/RADiCAL2026/simulations/RADiCALsimDSB/setup_env.sh   # <-- ALWAYS source first
make -j$(sysctl -n hw.logicalcpu)
./radical                 # opens OpenGL viewer (geometry only)
./radical run_batch.mac   # batch physics run -> build/radical_output.root
```

Geometry overlap check:
```bash
printf '/run/initialize\n/geometry/test/run\n' > /tmp/check.mac
./radical /tmp/check.mac 2>&1 | grep -iE "overlap|OK|Exception"
```

### CRITICAL gotcha — Geant4 data paths
The installed data versions do NOT match older hardcoded names. `setup_env.sh`
sets the correct ones. If you ever see `PART70001`, `had014`, `em0003`, or
"data file ... is not opened", a data path is wrong. Correct versions on this
machine (under `/Users/macro-2/Research/geant4-install/share/Geant4/data/`):
`G4ENSDFSTATE3.0`, `PhotonEvaporation6.1.2`, `RadioactiveDecay6.1.2`,
`G4PARTICLEXS4.2`, `G4PII1.3`, `RealSurface2.2`, `G4SAIDDATA2.0`, `G4ABLA3.3`,
`G4INCL1.3`, `G4EMLOW8.8`, `G4NDL4.7.1`.

## Geometry (DetectorConstruction.cc)

Stack = **29 LYSO (1.5mm) + 28 W (2.5mm) + 56 Tyvek (0.2032mm)** = `stackZ` **124.88 mm**.
- Pattern: `LYSO(0)|Tyvek|W(0)|Tyvek|LYSO(1)|...|LYSO(28)`. Even tile=LYSO, odd=W.
- Tiles are 14×14 mm (±7 mm half-width). Shared logical volumes placed by copy number.
- One LYSO+W period = **4.4064 mm** (with 0.2032mm Tyvek). **Center of LYSO layer L = L×4.4064 + 0.75 mm from upstream face.**
- Convention: beam travels +z, so "upstream" = −z end, "downstream" = +z end.
- Housing: Delrin shell, 18mm outer / 14mm inner cavity, `housingHalfZ`=65mm.
- World: ±120mm transverse, ±650mm z (CERN test-beam line).

### Geometry corrections applied (June 2026, vs arXiv:2401.01747 / NIM A 1068 (2024) 169737)
| Parameter | Old (wrong) | Corrected | Source |
|-----------|-------------|-----------|--------|
| Tyvek thickness | 0.01 mm | **0.2032 mm** (0.008") | arXiv:2401.01747 §2 |
| WLS length | 6 mm | **15 mm** | arXiv:2401.01747 §2 |
| Corner capOff | 4.5 mm | **3.5 mm** | arXiv:2401.01747 Fig. 2 |
| showerMaxDepth | 43.0 mm | **40.4 mm** (layer 9, 20–30 GeV opt.) | arXiv:2401.01747 Fig. 7 |
| stackZ | 114.06 mm | **124.88 mm** | Tyvek fix |
| WLS period coverage | layers ~10–11 | **layers ~7.3–10.7** | WLS pos fix |

### Capillaries (PAPER-FIDELITY UPDATE 2026-07-22: 183 mm, no center capillary)
- **Center hole: DRILLED BUT EMPTY.** The paper states the fifth central hole
  "was unused in these studies, but is reserved for future use" — the tested
  module had NO center capillary. The old EJ309-filled center capillary
  (quartz tube + liquid on the beam axis) was REMOVED; `centerHoleR`=0.45mm
  keeps the drilled hole (air). `Cap_Center_EJ309` no longer exists →
  H1[4] CenterCapEnergy / H1[10] / H2[3] are now empty (SteppingAction branch
  left in place, dead). Data taken before 2026-07-22 has the extra on-axis
  material — do not mix with new runs.
- **4 corners (timing), FULL 183 mm LENGTH** (`capTotalLen`; paper: ~18 cm,
  extending ~2.5 cm beyond the module each end to the SiPMs — was a deferred
  gap, closed after seeing discrete_sims implement it). Chain per corner:
  in-cavity quartz rods (stretched to the calo-cavity faces) + quartz tube
  mid-section + **DSB1 WLS fiber (r=0.45mm, 15mm) at shower max (UNCHANGED,
  32.9–47.9 mm)** + world-mothered `Cap_Corner_Ext` extension rods passing
  through 4 new drilled holes in the Delrin end walls, out to z=±91.5 mm.
  `cornerHoleR`=0.65mm, OD=1.15mm — verified vs paper: OD=1150μm, bore=950μm,
  fiber=900μm. Corner positions ±3.5mm from tile center.
  **PDs now sit OUTSIDE the housing** at ±(91.5mm+pdHalfZ), mother=world,
  same logical names `PD_Upstream`/`PD_Downstream` (SteppingAction unchanged).
  Validated: 155 volumes overlap-clean; 33k p.e. reach the relocated PDs at
  50 GeV (quartz→quartz index-matched at the calo/world seam); ΔT mean ~0.17 ns
  preserved (symmetric extensions cancel in DW−UP).

### Materials
LYSO, Tungsten (W), Tyvek, Delrin (POM), fused quartz, EJ309 liquid scintillator,
**DSB1** (polysiloxane WLS, ~1.05 g/cm³, modeled PDMS-like Si 37.9%, O 21.6%,
C 32.4%, H 8.1%; ~10000 ph/MeV, RINDEX 1.50). **Measured optical properties:**
absorption peak **λ=425 nm** (matches LYSO ~420 nm emission), emission peak
**λ=495 nm**, fluorescence decay **τ=3.5 ns**. The fast 3.5 ns decay (vs LuAG:Ce's
60 ns) is the whole point of this variant — sharper leading edge → better σ_t.
The fiber is modeled as a self-scintillator, so the 425 nm WLS-absorption band is
encoded (WLSABSLENGTH/WLSCOMPONENT) but only functional once LYSO 420 nm optical
light is propagated into it; emission + decay are used now. Density/yield are
literature estimates. Decay overridable via `RADICAL_DSB_DECAY_NS` (default 3.5).

### Material properties — verification pass (June 2026)
Cross-checked every material against the RADiCAL paper (arXiv:2401.01747 HTML)
and manufacturer/literature datasheets. Code = what's actually compiled in;
✓ = matches a real source; ~ = within published spread; "no MPT" = material has
no `SetMaterialPropertiesTable` call, so it produces **zero** optical photons
regardless of these numbers (only its `density`+composition feed the dE/dx shower).

| Material | In sim | Verified value | Source | Optically active? |
|----------|--------|-----------------|--------|---------------------|
| LYSO:Ce | ρ=7.1 g/cm³ ✓ | 7.1 g/cm³ | Saint-Gobain/Luxium datasheet | **No** — no MPT |
| LYSO:Ce | composition tightened (71.45/4.03/6.37/18.15%) | computed from Lu1.8Y0.2SiO5 (M=440.82) | stoichiometry | — |
| LYSO:Ce | (not modeled) | 33200 ph/MeV, 36 ns decay, 420 nm emission | Luxium datasheet | n/a — not in sim |
| Quartz (fused SiO2) | RINDEX 1.455–1.472 ✓ | 1.453–1.476 (350–800nm) | Malitson Sellmeier eq. | Yes |
| Tyvek | reflectivity 0.98 ✓ | 98% diffuse | coded `REFLECTIVITY` prop, standard detector-wrap spec | surface only |
| EJ309 | ρ=0.959 g/cm³ ✓ | 0.959 g/cm³ | Eljen datasheet | **No** — no MPT |
| EJ309 | (not modeled) | ~11500 ph/MeV, 3.5 ns decay, 424 nm emission | Eljen datasheet / NIM papers | n/a — not in sim |
| **DSB1** | absorption 425nm ✓, emission 495nm ✓, decay 3.5ns ✓ | **identical** | arXiv:2401.01747 (this paper, directly) | Yes |
| DSB1 | ρ=1.05 g/cm³, yield 10000 ph/MeV | **not published** — estimate | none found | Yes (estimate) |
| Capillary geometry | OD 1.15mm, bore 0.475mm(r), fiber 0.45mm(r) | OD 1150μm, bore 950μm, fiber 900μm | arXiv:2401.01747 (this paper) | — |

**Biggest open uncertainty: DSB1 density and light yield are not in any public
source** (the paper gives only the 3 optical constants above — already exact in
the code). Treat `RADICAL_SCINT_YIELD` as a free knob until a real measured
yield is available from the collaboration.

**Architectural note:** LYSO and EJ309 carry zero optical photons in this sim —
"LYSO light" and "EJ309 yield" histograms are dE/dx energy deposits, not photon
counts. Only the corner DSB1 fiber does real optical-photon transport (Cherenkov
in quartz + DSB1's own scintillation/self-emission). A true WLS chain (LYSO light
→ absorbed by DSB1's 425nm band → re-emitted at 495nm) is wired but inert
(WLSABSLENGTH/WLSCOMPONENT) since LYSO has no RINDEX/photon source yet.

### Visualization (for inspection)
Housing + all tiles + quartz tubes = **wireframe** w/ `SetForceAuxEdgeVisible(true)`.
LYSO=blue, W=red, Tyvek=white. The two ACTIVE scoring volumes are kept **solid**:
EJ309 bore=green, DSB1 fibers=orange — so they pop against the wireframe lattice.

## Scoring & histograms (RunAction.cc → radical_output.root)

Volume name → EventAction routing (SteppingAction.cc): `LYSO`, `W_Absorber`,
`Cap_Center_EJ309`, `Cap_Corner_WLS`.

**24 TH1D (H1[0–23]) + 15 TH2D (H2[0–14])** — last entries are the test-beam line
detectors (see CERN test-beam line section below). Full histogram inventory:

| ID | Name | Description |
|----|------|-------------|
| H1[0] | ShowerProfile | Energy/layer (longitudinal) |
| H1[1] | TotalLYSO | Sampled LYSO energy (GeV), 5000 bins 0–25 |
| H1[2] | TotalW | W absorber energy |
| H1[3] | SamplingFraction | E_LYSO/(E_LYSO+E_W) |
| H1[4] | CenterCapEnergy | EJ309 liquid scintillator yield (MeV) |
| H1[5] | CornerWLSEnergy | LuAG:Ce WLS per corner (MeV) |
| H1[6] | DeltaT | First-photon ΔT downstream−upstream (ns), 4000 bins −0.2→0.6 |
| H1[7] | ShowerMaxLayer | Layer of shower maximum |
| H1[8] | ShowerCOG | Energy-weighted longitudinal COG (layers) |
| H1[9] | ShowerRMS | Longitudinal shower RMS width (layers) |
| H1[10] | CenterCapFraction | E_EJ309/E_LYSO |
| H1[11] | CornerWLSPerCorner | WLS energy bar chart (x=corner index 0–3) |
| H1[12] | ZResidual | z_reco−z_true residual (mm) |
| H1[13] | TotalCornerWLS | Sum of all 4 corner WLS energies (MeV) |
| H1[14] | Trig1Edep | Trigger scint 1 dE |
| H1[15] | Trig2Edep | Trigger scint 2 dE |
| H1[16] | MCPEdep | MCP radiator dE |
| H1[17] | PbGlassEnergy | Pb-glass tail-catcher energy (GeV) |
| H1[18] | WLS_minus_MCP | RADiCAL WLS time − MCP t0 (ns) |
| H1[19] | TOF_Trig1_MCP | Trig1→MCP TOF (ns) |
| H1[20] | ECombined | Tail-catcher-corrected E = E_LYSO + 0.18·E_PbGlass (GeV) |
| H1[21] | PhotonsDetected | Detected optical photons/event (N_p.e.) |
| H1[22] | DeltaT_CFD | **Waveform 5% CFD ΔT** (data-identical estimator), 800 bins −4→4 ns |
| H1[23] | PulseFWHM | Emulated pulse FWHM (ns); validate vs data ~8.3 ns |

| ID | Name | Description |
|----|------|-------------|
| H2[0] | DeltaT_vs_TrueZ | Timing calibration matrix |
| H2[1] | ZReco_vs_ZTrue | Position reco diagonal |
| H2[2] | LateralProfile | Integrated XY energy map (70×70, ±7mm) |
| H2[3] | CenterCapVsLYSO | EJ309 vs LYSO linearity |
| H2[4] | CornerWLSVsLYSO | WLS vs LYSO correlation |
| H2[5] | DeltaTVsLYSO | ΔT vs sampled energy |
| H2[6] | ShowerMaxVsLYSO | Shower depth vs energy |
| H2[7–12] | LateralProfile_Slice0..5 | XY maps at 6 depth slices (layers 0–4, 5–9, 10–14, 15–19, 20–24, 25–28); routed in EventAction via `depthSliceH2(layer)` lambda |
| H2[13] | LYSOvsPbGlass | Tail-catcher correlation |
| H2[14] | MCPtime_vs_WLStime | MCP t0 vs WLS arrival timing correlation |

Timing recon uses the ACTUAL recorded WLS hit z-positions (not a hardcoded
constant), so moving the WLS section flows through automatically.

Inspect output:
```bash
root -l -b build/radical_output.root -e 'gDirectory->ls(); gApplication->Terminate();'
```

## Optical photons (timing) — toggle via env var
`RADICAL_OPTICAL=1 ./radical ...` enables Cherenkov + LuAG:Ce scintillation +
light-guiding + upstream/downstream photodetectors (20% QE) → real photon-based
downstream−upstream ΔT timing (H1[6] DeltaT, H1[21] PhotonsDetected). **OFF by default** (gated in
radical.cc) because it is ~190× slower (~34 s/event @120 GeV vs ~0.18 s; >4000
p.e./event). When OFF, the optical material tables/PDs sit inert and **DeltaT is
empty** (timing is optical-only now — the old geometric ΔT proxy was removed from
H1[6]). Energy/σ-E and shower profiles work in both modes. GPU accel (Celeritas/
AdePT) doesn't cover optical photons; Opticks does but needs NVIDIA+OptiX (N/A on
this Mac). Best speed lever: cut LuAG yield ~10× (still ~400 p.e., good timing).

## REAL test-beam data (June 2026 comparison)
Located `/Users/macro-2/Research/RADiCAL/Data/`: RUN1211 (25 GeV), RUN1259/60/61
(150 GeV), ~30k events each, 2 GB files. Format: TTree `pulse` with
`timevalue[4096]` (5 GS/s, 1022 ns window) + `amplitude[36864]` = **9 ch × 4096**.
- **Pileup**: 2–6 pulses/window — must select in-time pulses (±15 ns around each
  channel's mode time: MODE[9]={75,75,125,95,396,406,85,115,115} ns).
- **Channel map (inferred)**: ch0–ch1 = the instrumented timing-capillary
  upstream/downstream pair (tightest ΔT, matching mode times). ch6/ch8 narrow
  (~4 ns FWHM) = MCP/trigger-like. ch4–ch5 NOT a capillary pair (8 ns ΔT core).
- **Saturation**: DRS4 clips at ~830 mV → 74% of capillary pulses saturated at
  150 GeV (3.8% at 25 GeV). Amplitude analysis only valid at 25 GeV; timing OK
  (leading edge intact; non-sat subset σ_t=553 ps consistent w/ 502 full).
- **CFD convention (per user): 5% of peak**, not 50%.
- **Measured timing (5% CFD, iterative ±2σ core)**: σ_t(25)=614 ps,
  σ_t(150)=470–488 ps across the 3 runs (mean 476). Pulse FWHM ≈ 8.3 ns.
  (For reference, 50% CFD gave 558 / 502 ps.)
- Analysis snippets in /tmp during session; reference macro: analysis/compare_data.C.

### RESULT of data-vs-sim comparison (June 2026) — 5% CFD convention
| E (GeV) | DATA σ_t | SIM 5% CFD | SIM first-photon |
|---------|----------|------------|------------------|
| 25  | 614 ps | 145 ps | 100 ps |
| 150 | 476 ps |  68 ps |  94 ps |
(50%-CFD first pass for reference: data 558/502, sim 485/393 ps.)

**KEY FINDING:** data σ_t is nearly THRESHOLD-INDEPENDENT (5%: 614/476 vs 50%:
558/502) while the noiseless sim collapses to its photostatistics floor at 5%.
⇒ The real detector's ~500 ps is NOT photostatistics-limited; it is dominated by
a pulse-wide systematic that shifts whole waveforms event-to-event — most likely
uncalibrated DRS4 inter-cell timing (raw `timevalue` used, no cell-by-cell
calibration; typically several-hundred-ps RMS), plus amplifier rise time/noise on
the 5% crossing. The sim says the light itself supports ~70–145 ps.
Graphs: build/plots/datacomp/ (sigma_vs_E.png, data_deltaT_{25,150}GeV.png,
data_waveform_example.png) via analysis/compare_graphs.C + /tmp/dataplots.C.

### Waveform emulation added for data comparison
First-photon ΔT (~95–111 ps) is idealized. Added **waveform emulation** in
EventAction (`pulseCFD()`): sums single-photon responses
SPR(t)=(1−e^{−t/1.0ns})·e^{−t/3.0ns} over ALL detected photon arrival times
(stored in fPhTUp/fPhTDown vectors up to kMaxStore=60000), samples at 0.2 ns
(DRS4-like 5 GS/s), applies **5% CFD** (user-confirmed convention — `thr = 0.05 * pk`).
H1[22] DeltaT_CFD = data-identical estimator. H1[23] PulseFWHM = FWHM to
validate against data ~8.3 ns. Comparison sim runs (40 evt optical, `build/datacomp/`):
`./build/datacomp_run.sh` (handles merge-safety + retry).

## Test-beam analysis config (analysis/plot_testbeam.C)
Replicates CERN test-beam plots. Run per energy file:
`root -l -b -q 'analysis/plot_testbeam.C("build/radical_output.root", 120)'`
→ writes 4 PNGs to build/plots/ (energy res, timing res, long/lat shower).

Locked conventions (per user, June 2026):
- **Energy estimator** = tail-catcher-corrected `ECombined` = E_LYSO + f_s·E_PbGlass
  (f_s=0.18). The −0.94 LYSO/PbGlass anti-correlation lets the Pb-glass recover
  forward leakage → tightens σ/E. Filled in EventAction §7 as H1[20].
- **Beam-acceptance cut**: ECombined filled only if module-reco E (E_LYSO/f_s) >
  E_PbGlass — removes halo events that missed the ±7 mm module and showered in the
  Pb-glass (a spurious sharp peak at ~0.18·E_beam). Keeps genuine leakage events.
- **Energy fit** = iterative Gaussian core, ±2σ, 4 iterations (excludes leakage tail).
- **Timing** = downstream−upstream ΔT (H1[6]); MCP reference cancels. Gaussian core fit → σ_t.
  CAVEAT: σ_t here is a GEOMETRIC proxy (spread of energy-deposit z within the 6 mm
  WLS) — no optical-photon/photostatistics/electronics modeled. Trends vs E are
  meaningful; absolute ps value is not the real resolution.
- Energy hist `TotalLYSO`/`ECombined` = 5000 bins 0–25 GeV; macro adaptively rebins
  to ~σ/5 per energy. `DeltaT` = 2500 bins 0–0.5 ns (0.2 ps/bin).
- 120 GeV result: σ/E ≈ 2.6%, μ ≈ 16.9 GeV sampled; σ_t ≈ 10.8 ps, ΔT ≈ 136 ps.

### Energy scan (DONE — June 2026, 1500 evt/point)
Driver: `./run_scan.sh` — runs 5,10,20,50,100,120 GeV, one process per energy
(beam energy via env var `RADICAL_BEAM_ENERGY_GEV`, MT-safe), with the merge-safety
loop (kill procs + rm output + validate ECombined integral + retry). Writes
`build/scan/radical_E{N}GeV.root`.
Analysis: `root -l -b -q analysis/scan_resolution.C` → fits every energy file and
builds `build/plots/{energy,timing}_resolution_curve.png` + shower_long_overlay.png.
It ALSO writes `build/scan/resolution_curves.root` holding the resolution curves as
ROOT objects: `EnergyResolution` + `TimingResolution` (TGraphErrors, fit TF1 stored
inside each) and a `scan` TTree (E, sigmaE_pct, sigmaT_ps). `run_scan.sh` calls this
analysis automatically at the end, so the curves .root refreshes on every scan.

Results (1500 evt/point, tail-catcher energy, downstream−upstream ΔT):
| E (GeV) | σ/E (%) | σ_t (ps) |
|---------|---------|----------|
| 5   | 6.42 | 17.2 |
| 10  | 5.40 | 16.1 |
| 20  | 3.91 | 14.9 |
| 50  | 3.22 | 13.2 |
| 100 | 2.78 | 12.0 |
| 120 | 2.62 | 11.0 |
Fits: **σ/E = 14.1%/√E ⊕ 2.38%**;  **σ_t = 33.9 ps/√E ⊕ 11.5 ps** (σ_t floor is the
geometric WLS-spread proxy — no photostatistics; see caveat above). ΔT mean ≈137 ps
stable across energy (WLS at fixed z), confirming the downstream−upstream observable.

## Beam (PrimaryGeneratorAction.cc)
120 GeV e⁻, momentum +z, Gaussian spot σ=2.9mm centered at **(0,0,−500mm)** —
upstream of the first trigger counter so the beam traverses the full test-beam line.
**Currently centered** (an earlier −25mm offset request was reverted — it missed
the ±7mm module entirely).

## CERN test-beam line (DetectorConstruction.cc §9)
Full beamline replicated from a test-beam photo (standard defaults; a photo gives
no exact metrology — all params are gathered in §9 for easy correction).
Beam travels +z; RADiCAL module stays centered at z=0. World enlarged to
±120mm transverse, ±650mm z.

| Element | volume name(s) | z-center | size | material |
|---------|----------------|----------|------|----------|
| Trigger scint 1 | `Trig1` | −400mm | 30×30×5mm | plastic scint (vinyltoluene) |
| Trigger scint 2 | `Trig2` | −350mm | 30×30×5mm | plastic scint |
| MCP window (timing) | `MCP_Radiator` | −250mm | 27×27×3mm | fused silica (G4_SILICON_DIOXIDE) |
| MCP body | `MCP_Body` | −247mm | 27×27×3mm | Al₂O₃ (kept thin, <0.05 X0 preshower) |
| RADiCAL module | (see above) | 0 | — | LYSO/W |
| Pb-glass calo | `PbGlass` | +320mm | 100×100×400mm | G4_GLASS_LEAD (~30 X0) |

- MCP `MCP_Radiator` records earliest hit time = **t0 timing reference**.
- Trigger counters record dE + earliest time (coincidence + TOF).
- Pb-glass is the downstream **tail catcher** (sees ~4% leakage of 120 GeV).
- Scoring is edep + particle-passage time (NO optical-photon tracking — consistent
  with how the RADiCAL capillaries work). Adding Cherenkov/optical is a future option.

### New histograms (beamline) — appended to existing set
- H1[14] Trig1Edep, H1[15] Trig2Edep, H1[16] MCPEdep, H1[17] PbGlassEnergy,
  **H1[18] WLS_minus_MCP** (RADiCAL WLS time − MCP t0, the key resolution plot),
  H1[19] TOF_Trig1_MCP.
- H2[13] LYSOvsPbGlass (tail-catcher correlation), H2[14] MCPtime_vs_WLStime.

Validated (50 evt): Trig dE ≈1 MeV (MIP), TOF Trig1→MCP =0.504ns (geom 0.500ns),
t_WLS−t_MCP =0.81ns (geom 0.79ns), Pb-glass ≈5.3 GeV leakage. All physical.

## Git / GitHub workflow
- **Auto-push hook** in `~/.claude/settings.json` (PostToolUse, Write|Edit matcher):
  every edit under `/Users/macro-2/Research/*` auto-commits + pushes. So source
  changes land on GitHub automatically — no manual commit needed.
- `.gitignore` excludes build dirs, `*.root`, geant4 source/install, `.DS_Store`,
  CMake cruft. Do NOT commit large geant4 tarballs (previously broke the push).
- Commit author shows as MACRO-2 (git identity not globally configured — cosmetic).

## Conventions & gotchas
- Geant4 v11.4.0 has **no** `G4StrUtil.hh` — use `str.find(...) != G4String::npos`.
- After renaming/moving the project, delete `build/CMakeCache.txt` and re-run cmake
  (it hardcodes the source path).
- Multithreaded run writes per-thread `radical_output_tN.root` then merges into
  `radical_output.root`; empty thread files are auto-deleted.
- **MERGE GOTCHA (important):** the G4 MT master merge intermittently fails,
  leaving ~1 event in the output (`ECombined`/`TotalLYSO` Integral ≈ 1 instead of
  ≈ #events; log shows `delete empty file ... has failed`). Two triggers:
  (1) a stale `./radical` or `root` process holding the file open, and
  (2) **overwriting an existing `radical_output.root`**. ALWAYS before a run:
  `ps aux | grep -E '/radical|root' | grep -v grep` + `kill -9` leftovers, AND
  `rm -f radical_output*.root`. Then VALIDATE via `Integral()` (not GetEntries)
  and retry if it dropped events. This is baked into the scan workflow.
- `./radical` with no arg = viewer; with a `.mac` arg = batch. vis.mac has no beamOn.
- Disk has been tight before — watch free space before large runs.

## Analysis scripts

| Script | Command | Output |
|--------|---------|--------|
| `analysis/plot_testbeam.C` | `root -l -b -q 'analysis/plot_testbeam.C("build/radical_output.root",120)'` | 4 PNGs in build/plots/ |
| `analysis/scan_resolution.C` | `root -l -b -q analysis/scan_resolution.C` | build/scan/resolution_curves.root + PNGs |
| `analysis/compare_data.C` | `root -l -b -q analysis/compare_data.C` | prints σ_t table (sim vs data) |
| `analysis/compare_sim_data.C` | `root -l -b -q 'analysis/compare_sim_data.C("build/scan/optical_scan_<N>")'` | sim (data-matched H1[31-33]) vs test-beam + paper overlay PNGs |
| `analysis/compare_graphs.C` | `root -l -b -q analysis/compare_graphs.C` | build/plots/datacomp/sigma_vs_E.png |
| `analysis/fix_titles.C` | one-time patch | replaces "front/back" → "upstream/downstream" in ROOT files |

Plots already produced (build/plots/datacomp/):
- `sigma_vs_E.png` — σ_t vs E_beam: DATA (black), SIM 5% CFD (red), SIM first-photon (blue)
- `data_deltaT_25GeV.png` / `data_deltaT_150GeV.png` — measured ΔT distributions
- `data_waveform_example.png` — raw DRS4 single-event waveform (ch0 blue, ch1 red)

## Open questions / future work
1. **DRS4 inter-cell calibration**: applying per-cell timing correction to raw data
   should move σ_t from ~500 ps toward the sim's ~70–145 ps prediction.
2. **Tune SPR τ_f**: sim emulated pulse FWHM ~17–19 ns vs data ~8.3 ns — adjust τ_F
   from 3ns toward ~1.5ns; τ_R from 1ns stays, or tune both to match data FWHM first.
3. **Add DRS4 noise floor** to sim emulation (~0.5–2 mV RMS, but much smaller than
   the inter-cell calibration effect).
4. **Optical yield reduction** (×10) to speed up scans while keeping photostatistics regime.

## Session history (high level)
Built full geometry → GitHub upload + auto-push hook → enriched histograms
(shower shape, timing, capillary, lateral) → DSB1 swapped to LuAG:Ce →
6 depth-sliced lateral profiles → vis switched to wireframe for inspection →
WLS shortened to 6mm → energy cap fills its hole → **WLS retargeted to measured
shower max (43mm / layer ~10.5)** after histogram showed peak is not at center →
energy scan (5–120 GeV, 1500 evt/point) → CERN test-beam data comparison (4 runs,
5% CFD convention) → waveform emulation (H1[22] DeltaT_CFD) → sim/data gap
identified as DRS4 calibration artifact → graphs generated.
