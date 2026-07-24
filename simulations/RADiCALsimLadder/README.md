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

1. **The sim over-collects light by ~3–4×.** The end-to-end WLS
   capture×transport×PDE fraction is **~11 %**, where the physical limit for an
   n≈1.5 fiber (≈5–6 % total-internal-reflection trapping per end × ~36 % PDE)
   is **~3 %**. This is measured directly (H1 `PhotonsWLS` ÷ `PhotonsWLSEmitted`)
   and is stable across all rungs.
2. **That extra light over-saturates the SiPM.** The fired-pixel energy sags
   with beam energy (the "energy sag" column > 1, worsening as light rises) —
   i.e. the SiPM is running into its 5676-microcell ceiling. This **contradicts
   the paper's linear energy response** (Fig. 17), an independent confirmation
   that the modeled light level is unphysically high. A saturated pulse has a
   coarse, jittery leading edge, which is exactly what inflates the
   light-independent floor **B** and produces the pessimistic extrapolation.

## What we learned (the takeaway)

- **The ladder did its job.** It is a measurement, not a tuning knob, and it
  converted a single uninterpretable thinned run into a decomposition plus two
  corroborating diagnostics.
- **The headline number (958 ps/√E) is not a physics result — it is a symptom.**
  The dominant floor B is an artifact of SiPM over-saturation, which is caused by
  the simulation collecting **~3–4× too much light** (11 % capture vs ~3 %
  physical). The timing floor cannot be trusted until the light budget is right.
- **The three findings agree.** Over-collection (11 % capture), the energy
  nonlinearity (vs the paper's linear Fig. 17), and the pessimistic timing floor
  are one problem viewed three ways — not three separate issues.
- **The fix is upstream of timing.** Bring the WLS capture fraction down to the
  physical ~3 % (fiber-surface roughness / a corrected trapping model / a
  PDE-and-coupling audit). That should linearize the energy response *and*
  release the timing floor, at which point the ladder can be rerun to give a
  trustworthy comparison to 256 ps/√E.

If you need one config to quote as the current state, use **f = 3** (most light,
best-behaved, `a = 1020 ps/√E`) — but flag it as saturation-limited, not final.

## Files

```
run_ladder.sh          reruns the four scans (reuses ../RADiCALsimDSB)
analysis/ladder.C      decomposition + true-light extrapolation; writes results/
results/ladder.png     left: σ_t(E) per light level; right: a² vs 1/f fit
results/ladder_summary.txt   the numbers above
scan/                  merged per-energy ROOT files (git-ignored)
```
