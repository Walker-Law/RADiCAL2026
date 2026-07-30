# RADiCALsimLightScan — how much of σ_t is just photon counting?

A **study**, not a detector. It reuses the `RADiCALsimSIMPLE` binary and
geometry completely unmodified; the only thing that changes between runs is
the light scale. Read [SIMPLE's README](../RADiCALsimSIMPLE/README.md) first.

---

## The question

Every σ_t this project quotes is measured at **1% light**
(`RADSIMPLE_LIGHT_SCALE=1e-2`), because tracking all ~5×10⁸ optical photons in
a 120 GeV event is intractable. The current best number is
**σ_t = 333/√E ⊕ 0 ps**, i.e. 26.2 ps at 120 GeV — at 1% light.

The obvious move is to extrapolate: 10× less light means √10 worse timing, so
scale by √100 and claim ~2.6 ps at true light. **That is only valid if the
resolution is entirely photon counting.** In general it is not:

```
    σ_t²(f) = A²/f  +  B²
              \___/    \_/
        photon counting   light-INDEPENDENT floor
        (shrinks as f)    (never shrinks: fibre transit-time spread,
                           path-length dispersion, geometry)
```

A single thinned run cannot separate those two terms — it measures their sum
and nothing more. Extrapolating one point silently **assumes B = 0**. That
exact assumption hid a **43.5 ps floor** in the DSB sim's own ladder
([../RADiCALsimLadder](../RADiCALsimLadder/README.md)). This study is the
SIMPLE-family equivalent, and **B is the number that decides whether the
<10 ps goal is reachable at all.**

## The method

Run the same sweep at several light scales, plot σ_t² against 1/f, fit a
straight line. Slope = A², intercept = B². That's it.

```bash
bash run_lightscan.sh 2000          # default rungs, 25 + 120 GeV
```

On a cluster (logs itself, no redirect needed):

```bash
nohup bash run_lightscan.sh 2000 &
tail -f build/logs/run_lightscan.log
```

Then:

```bash
root -l -b -q analysis/lightscan.C
```

## Why these rungs

Default `f = 1e-3, 3e-3, 1e-2, 3e-2`. Two deliberate choices:

**1e-2 is included** because it's the production scale every existing result
uses — it anchors the ladder to numbers already trusted.

**3e-2 is the top rung**, and it is the expensive one (cost scales ~linearly
with f, so it dominates the runtime). It earns that cost: the intercept B is
only well constrained by rungs *close to true light*, because that's where B²
stops being dwarfed by A²/f. Verified by injecting known values and refitting
(A = 2.62 ps√f, i.e. the measured scale, 2% errors):

| true B | top rung 3e-3 | top rung 1e-2 | **top rung 3e-2** |
|---|---|---|---|
| 2 ps | ±48.6 | ±10.3 | **±3.0** |
| 5 ps | ±19.6 | ±4.2 | **±1.3** |
| 10 ps | ±10.0 | ±2.3 | **±0.8** |

The fit recovers B exactly in every case — what changes is the *error bar*.
Stopping at 1e-2 leaves a 5 ps floor determined only to ±4 ps, which answers
nothing. Going to 3e-2 measures a 10 ps floor at ~12σ, and even a 2 ps floor
is bounded well under the goal. **That is the whole reason the top rung is
worth its runtime.**

Two energies (25 and 120 GeV) confirm the A/B split isn't an artifact of one
operating point, at a fraction of the full 6-energy cost.

## Does this actually reach TRUE light?

Yes — and that claim is worth being precise about, because the fit is
evaluated at 1/f = 1, which lies **outside** the measured range
(1/f = 33…1000).

**f = 1 really is true light.** `RADSIMPLE_LIGHT_SCALE` multiplies LYSO's
datasheet 33200 ph/MeV directly, and `StackingAction` thins Cherenkov by the
same factor, so f = 1 means no thinning anywhere. There is no further
correction to apply.

**The extrapolation is short, not long.** At f = 1 the photon-counting term
has collapsed to nothing (A²/1 ≈ 7 ps²), so σ_t(f=1) = √(A²+B²) is dominated
by B — the *intercept*, which is exactly what the ladder measures directly.
Extrapolating a straight line to its own intercept is not a stretch. Injecting
known values and refitting gives the achievable precision on the true-light
number:

| true σ_t(f=1) | top rung 1e-2 | **top rung 3e-2 (default)** |
|---|---|---|
| 3.3 ps | ±6.3 | **±1.8** |
| 5.6 ps | ±3.7 | **±1.1** |
| 10.3 ps | ±2.2 | **±0.8** |

So with the default rungs, a true-light resolution anywhere near the 10 ps
goal is resolved to well under a ps — conclusive either way. `lightscan.C`
prints this number **with its error**, and flags `chi2/ndf` so a bad model
can't masquerade as a confident answer.

**Two guards on validity.** The analysis also checks that `Npe` is linear in
f (if it isn't, the detector isn't simply "the same thing with less light" and
the A²/f model is void — flagged automatically), and reports the extrapolated
true-light photon yield, which is the number to compare against the papers and
against DSB's ~50–90 pe/MeV prediction. That second check is what would catch
the sim over-collecting light, which is the failure mode that has bitten this
project before.

**What it does not cover:** electronics. See the caveat below.

## Reading the result

- **B** is the floor more light cannot fix. Compare it to 10 ps.
- **σ_t(f=1)** is the true-light extrapolation *including* that floor — the
  honest version of the naive √f scaling.
- **A** is the photon-counting coefficient; `A/√f` is what shrinks.

### The caveat that must travel with any number from here

SIMPLE models **no electronics**. B is therefore a **lower bound** on a real
device — SiPM SPTR (~60 ps single-photon), amplifier noise-over-slope, and
DRS4 timebase all add on top. The real test-beam data sits near 500 ps
*because of that chain*, not because of light. So:

- B ≪ 10 ps → light transport is **not** the obstacle; the goal lives or dies
  on the electronics (that work belongs in `archive/RADiCALsimDSB`, which has
  the full chain).
- B ≳ 10 ps → the goal is unreachable **regardless** of electronics, and the
  fix has to be geometric (fibre length, window, transit-time spread).

Either outcome is decisive, which is the point.

## Gotchas found while building this

- **Don't run the ladder at low energy.** At 5 GeV with f=1e-3 there are ~36
  detected photons — first-photon timing is meaningless and the fit returns
  garbage. The 25/120 GeV default exists for this reason.
- `analysis/lightscan.C` needs ≥50 events *after* the fiducial cut per point
  (the cut keeps ~35%), so ≥150 events/energy minimum, and far more for a
  publishable width. 2000 is the sensible floor for real use.
- The fiducial cut is the same one SIMPLE uses and is **not optional** — see
  [SIMPLE's scan.C](../RADiCALsimSIMPLE/analysis/scan.C) for why (beam down a
  capillary hole, or missing the 14 mm tile entirely).

## Layout

Standard, like every sim here — `build/rootfiles/f<scale>/E<N>GeV.root`,
`build/logs/`, `build/plots/`. See
[README_LAYOUT.md](../README_LAYOUT.md).
