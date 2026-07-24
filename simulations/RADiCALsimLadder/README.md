# RADiCALsimLadder — photostatistics scale-ladder timing study

A study built on top of **RADiCALsimDSB** (same binary, same paper-fidelity
geometry — this is not a separate detector). It answers one question:

> With the simulation now carrying a **realistic light composition** and a
> **realistic electronics chain**, does its timing resolution reproduce the
> RADiCAL beam-test result of **σ_t = 256 ps/√E ⊕ 17.5 ps**
> ([arXiv:2401.01747](https://arxiv.org/abs/2401.01747))?

## What was done

The simulation tracks only a small, thinned fraction of the real scintillation
light (tracking every photon at 150 GeV is intractable). A single thinned run is
therefore dominated by artificial photon-counting noise and cannot be compared
to the paper directly. The **scale ladder** removes that ambiguity without
tuning:

1. Run the full 25–150 GeV energy scan at four coherent light levels
   **f = 0.1, 0.3, 1, 3**. At each rung *all three* light sources — the
   LYSO→WLS chain, DSB1 self-scintillation, and quartz Cherenkov — are thinned
   by the same factor, so the light **mix stays physical** and `f` is a pure
   intensity knob.
2. Fit the timing resolution at each rung, `σ_t(E) = a(f)/√E ⊕ b(f)`.
3. Fit the stochastic coefficients across rungs:

   **a²(f) = A²/f + B²**

   which cleanly separates the **photon-counting** part (A, ∝ 1/√light) from the
   **light-independent floor** (B, shower sampling + scintillator-shape + electronics).
4. Extrapolate to true light (f = 1/0.01 = **100**) — the honest number to place
   beside the paper.

Reproduce: `bash run_ladder.sh 500` then `root -l -b -q 'analysis/ladder.C(500)'`
(≈5 h for the four scans on a 512-core node). Results in
[`results/`](results/); raw merged ROOT files in `scan/` (git-ignored, ~14 MB).

## The base configuration ("paperR")

Everything the RADiCAL project converged on for a faithful run, all active here:

- Paper-fidelity geometry: 183 mm capillaries, no center capillary, SiPM
  optically coupled at the rod tip.
- **Realistic light composition** — self-scintillation thinned coherently with
  the WLS chain, so the timing light is ~100 % WLS (the physical mix), not the
  70 % prompt self-scint that made earlier runs look artificially fast.
- **Realistic electronics chain** (added 2026-07-23): each SiPM pulse is built
  in physical millivolts — per-photon gain, time-ordered microcell saturation,
  a CR-shaped amplifier tuned to the measured **8.3 ns** pulse FWHM, 0.5 mV
  electronic noise, and DRS4 clipping + 12-bit digitization — *before* the same
  5 % constant-fraction discriminator the beam-test analysis uses.

## What we found

| f | light ×1 | σ_t stochastic `a` [ps/√E] | capture×PDE | energy sag |
|---|---|---|---|---|
| 0.1 | 0.10 | 2895 | 11.5 % | 1.16 |
| 0.3 | 0.30 | 1686 | 11.3 % | 1.19 |
| 1.0 | 1.00 | 1525 | 11.3 % | 1.32 |
| 3.0 | 3.03 | 1020 | 11.6 % | 1.65 |

**Fit:** A = 867, B = 954 ps/√E, χ²/ndf = 3.0 → **extrapolated true-light
a ≈ 958 ps/√E**, about **3.7× worse** than the paper's 256, with a floor `b` of
160–360 ps versus the paper's 17.5 ps. See [`results/ladder.png`](results/ladder.png).

Taken at face value this says the simulation is far *too pessimistic*. But the
same run carries two diagnostics that explain why, and point at a single root
cause rather than a timing problem:

1. **The sim over-collects light — it sits at the theoretical trapping ceiling.**
   The end-to-end WLS capture×PDE fraction is **~11 %**, i.e. **~31 % geometric
   capture × ~36 % PDE**. The corner light guide is an *air-clad quartz rod*
   (n=1.46 in a ~75 µm air gap), whose bare total-internal-reflection ceiling is
   exactly `1 − 1/1.46 = 31.5 %` of isotropic light. The rods had **no optical
   surface at all** — perfectly smooth walls — so every trapped ray reached the
   end and the sim ran at that ideal ceiling. Real fused rods scatter the
   near-critical rays out of the walls and fall below it. (Note: this is a quartz
   *waveguide*, so the trapping is genuinely much higher than a clad plastic
   fiber's ~3–4 % — the fix is surface-scatter loss, not a fiber cladding.)
   Measured directly, H1 `PhotonsWLS` ÷ `PhotonsWLSEmitted`, stable across rungs.
2. **That extra light over-saturates the SiPM.** The pulses slam into the DRS4
   rail and the fired-pixel energy sags as the beam energy climbs — the SiPM is
   hitting its 5676-microcell ceiling (at the f = 1 rung):

   | E (GeV) | 25 | 50 | 75 | 100 | 125 | 150 |
   |---|---|---|---|---|---|---|
   | pulses clipped at rail | 1 % | 10 % | 56 % | 81 % | 93 % | 95 % |
   | fired-pixels / E (flat = linear) | 109 | 105 | 97 | 91 | 88 | 83 |

   A linear detector keeps a flat fired-pixels/E; ours falls, and 95 % of pulses
   clip at 150 GeV. This **contradicts the paper's linear energy response**
   (Fig. 17) — an independent confirmation that the modeled light is unphysically
   high. A saturated, clipped pulse has a coarse, jittery leading edge, which is
   exactly what inflates the light-independent floor **B** and produces the
   pessimistic extrapolation.

## What we learned (the takeaway)

- **The ladder did its job.** It is a measurement, not a tuning knob, and it
  converted a single uninterpretable thinned run into a decomposition plus two
  corroborating diagnostics.
- **The headline number (958 ps/√E) is not a physics result — it is a symptom.**
  The dominant floor B is an artifact of SiPM over-saturation, which is caused by
  the simulation running at the **light-guide's ideal trapping ceiling** (rods
  had no surface at all). The timing floor cannot be trusted until the light
  budget is right.
- **The three findings agree.** Over-collection (capture at the 31 % ceiling),
  the energy nonlinearity (vs the paper's linear Fig. 17), and the pessimistic
  timing floor are one problem viewed three ways — not three separate issues.

## The fix (2026-07-24) and its validation

The rods had **no optical surface**, so the air-clad quartz guide ran at its
theoretical trapping maximum. The missing physics is **surface-scatter loss**:
real fused rods leak the near-critical guided rays out of the walls. Added a
ground dielectric surface (specular-lobe reflection so TIR still guides; only the
`RADICAL_ROD_SIGMA_ALPHA_DEG` micro-facet tilt leaks the near-critical rays).

Validated — capture×PDE at 5 GeV falls smoothly with roughness:

| σα (deg) | 0 | 0.3 | 0.7 | **1.3** | 3 |
|---|---|---|---|---|---|
| capture×PDE | 10.8 % | 10.0 % | 7.9 % | **5.6 %** | 4.2 % |

**Default σα = 1.3°** halves the capture — a defensible "real polished-but-
imperfect fused rod." (A first attempt with a plain *ground* surface was a bug:
it defaulted to Lambertian reflection and killed the guide entirely, capture → 0
at any roughness; the specular-lobe constant fixes that.)

**σα = 1.3° is a starting value, not a calibration.** The physical anchor is the
paper's linear Fig. 17: rerun and check that the fired-pixel energy no longer
sags. If it still saturates, the **unmeasured DSB1 light yield** (10000 ph/MeV,
an estimate) is the other lever on total light. Halving capture likely is *not*
enough on its own to fully linearize — expect to also trim the DSB1 yield.

## Lean next step (validate the fix without the full 4-point ladder)

One f = 1 scan with the fix, compared to the pre-fix `lad1` (identical config,
σα = 0). On curiosity:

```bash
conda activate g4 && cd ~/RADiCAL2026 && git pull
cd simulations/RADiCALsimDSB/build && make -j$(nproc)
setsid nohup env RADICAL_OPTICAL=1 RADICAL_ENERGIES="25 50 75 100 125 150" \
  RADICAL_LYSO_SCINT_SCALE=1e-2 RADICAL_SCINT_YIELD=1e-2 RADICAL_QUARTZ_CHER_KEEP=1e-2 \
  RADICAL_MAX_OPT_PHOTONS=20000000 RADICAL_SPTR_PS=60 RADICAL_SIPM_NPIX=5676 \
  RADICAL_SM_COG_CUT_MM=2 RADICAL_ROD_SIGMA_ALPHA_DEG=1.3 RADICAL_RUN_TAG=roughtest \
  bash ../run_scan.sh 300 1 > roughtest.log 2>&1 &
```

≈1 h. Sync `optical_scan_300_roughtest/` back; the comparison to `lad1` shows
whether capture dropped, Fig. 17 flattened, and the timing floor `b` fell.

If you need one existing config to quote meanwhile, use **f = 3** (most light,
best-behaved, `a = 1020 ps/√E`) — but flag it as saturation-limited, not final.

## Files

```
run_ladder.sh          reruns the four scans (reuses ../RADiCALsimDSB)
analysis/ladder.C      decomposition + true-light extrapolation; writes results/
results/ladder.png     left: σ_t(E) per light level; right: a² vs 1/f fit
results/ladder_summary.txt   the numbers above
scan/                  merged per-energy ROOT files (git-ignored)
```
