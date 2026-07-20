# RADiCALsimHoleScan — capillary-hole-diameter scan

Geant4 (v11.4.0) study: **light output at the capillary ends as a function of the
diameter of the holes drilled through the tiles.** Forked from `RADiCALsimDSB`
(identical base detector); the geometry is parametrized so all five holes share a
common, runtime-adjustable diameter and every capillary **scales to fill its
hole**.

## The parametrization

All five capillaries (4 corner timing + 1 center energy) share one hole diameter
`D` (`RADICAL_HOLE_DIAM_MM`, default 1.30 mm). Each capillary's outer diameter
equals `D`; its internal features scale proportionally, holding the paper's
design ratios fixed:

- **Corner timing capillary:** DSB1 WLS fiber radius = 0.7826 × D/2 (paper: 900/1150 µm)
- **Center energy capillary:** EJ309 bore radius = 0.4000 × D/2 (paper: 400/1000 µm)

Bigger hole → bigger fiber/bore → more captured light. Geometry is overlap-clean
across the full sweep range (1.2–2.0 mm, verified via `/geometry/test/run`).

## What's measured

The **center EJ309 capillary was made optically active** (it carries zero optical
photons in the parent `RADiCALsimDSB`) and given its own end photodetectors
(`PDC_Upstream`/`PDC_Downstream`), so all five capillaries — not just the four
corner WLS timing fibers — contribute detected light:

- **H1[38] Light_Corners** — detected p.e. summed over the 4 corner WLS capillaries, both ends
- **H1[39] Light_Center** — detected p.e. at the center EJ309 capillary, both ends
- **H1[40] Light_Total** — all five capillaries combined

All three are filled every event (including zeros) so each histogram's mean is
the true light yield at fixed hole diameter.

## Study: light output vs hole diameter

`run_hole_scan.sh [NEVT]` sweeps `RADICAL_HOLE_DIAM_MM` over 1.2 → 2.0 mm (0.1 mm
steps, 9 points) at fixed **50 GeV**, `NEVT` events per point (default 2000),
writing `build/scan/hole_scan_<NEVT>/hole_D<D>.root`. Chunks are split evenly
across all 9 points (same beam energy, so equal cost per point) and hadd-merged.
The photon budget (`RADICAL_MAX_OPT_PHOTONS`) is raised to 20M/event so bright,
large-hole events aren't silently truncated.

`analysis/plot_holescan.C(NEVT)` reads each point's mean light output and plots
all three curves vs D → `build/plots/holescan_light_vs_diameter.png`.

### Result so far (1000 evt/point)

Light output rises with hole diameter for all three curves, as expected. The
corner (4-capillary-summed) and center (1-capillary) curves track closely
despite the 4× multiplicity, because the center capillary sits on the beam axis
(shower core) while the corners sit ~5 mm off-axis in the shower's lateral tail:
per-capillary, the center collects roughly **2× more deposited energy** (pure
geometry) **and** converts it to detected photons roughly **2× more efficiently**
(direct EJ309 scintillation + quartz light guide, vs the corner's lossier
LYSO→DSB1 wavelength-shifting chain). Those two ~2× effects compound to ~4×,
which is what the 4-fold corner multiplicity is compensating for.

## Build & run

```bash
git clone https://github.com/Walker-Law/RADiCAL2026.git
cd RADiCAL2026/simulations/RADiCALsimHoleScan
source setup_env.sh
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=$CONDA_PREFIX -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX
make -j$(nproc)
cd ..

# Hole-diameter sweep (50 GeV, 9 diameters, all cores)
bash run_hole_scan.sh 1000        # events per hole diameter
```

## Analysis

```bash
root -l -b -q 'analysis/plot_holescan.C("build/scan/hole_scan_1000")'
```

`scan_resolution.C` (shared with the other sims) is also present and can be run
against any hole-scan output that includes a real multi-energy sweep, saving
per-energy histogram + Gaussian-fit overlays to `build/plots/fits/`. Since this
sim's primary study is a fixed-50-GeV hole-diameter sweep, `plot_holescan.C` is
the analysis that matters here.

## References

1. V. Beresovskyi et al., *"RADiCAL: a Radiation-hard Innovative Calorimeter"*, arXiv:[2303.05580](https://arxiv.org/abs/2303.05580) (2023).
2. C. Perez-Lara et al., *"Study of time resolution measurements and prospects for energy resolution of an ultra-compact sampling calorimeter (RADiCAL) module at EM shower maximum over the energy range 25 GeV–150 GeV"*, Nucl. Instrum. Methods Phys. Res. A **1068** (2024) 169737, arXiv:[2401.01747](https://arxiv.org/abs/2401.01747).
