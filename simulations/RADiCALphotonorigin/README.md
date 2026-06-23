# RADiCAL Photon Origin

Where does the **particle hit**, and which **SiPM lights up**? This Geant4 study
maps the transverse impact position of the beam (hence the shower) to the optical
signal seen by the four corner-capillary SiPMs of a RADiCAL module.

Derived from `RADiCALopticalcrosstalk` — same geometry and readout — with three
additions:

1. **Random-uniform beam impact.** Each event fires a pencil beam at a fresh
   random `(x,y)` over the 14×14 mm tile face, so a run sweeps the whole
   transverse plane. (The cross-talk sim fired a fixed central beam.)
2. **Shower-max LYSO drawn as 4 colour-coded quadrants.** In the WLS z-window each
   LYSO tile is built as four 7×7 mm quadrant volumes — TR red, TL yellow, BR
   green, BL blue — so you can *see* which quadrant the shower develops in.
3. **Position → SiPM scoring.** Per-corner "where was the beam when this corner
   lit up" maps, plus a beam-quadrant → detecting-corner response matrix.

> LYSO emits **no** optical photons here — it's the energy-sampling absorber. All
> detected light is quartz Cherenkov + LuAG:Ce WLS born **inside the corner
> capillaries**, read out at each end by a SiPM. The colour quadrants just mark
> *where the shower is*; the physics question is whether an off-centre shower
> preferentially lights the **nearest** corner SiPM (transverse position
> sensitivity), which the Molière-radius shower spread blurs.

## Build

```bash
source setup_env.sh
rm -rf build && mkdir build && cd build      # fresh, after the dir rename
cmake .. && make -j$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)
```

## Visual: watch a quadrant light up its SiPM

```bash
cd build
./radical                                   # bare = TR-quadrant demo (1 GeV)
RADICAL_BEAM_QUADRANT=3 ./radical vis_quadrants.mac   # bottom-left, etc. (0=TR 1=TL 2=BR 3=BL)
RADICAL_BEAM_X_MM=2 RADICAL_BEAM_Y_MM=-5 ./radical vis_quadrants.mac   # explicit point
```

The shower lands in the coloured quadrant; the optical photons it makes in the
nearby corner capillary stream to that corner's SiPM, coloured by the same key.
Only optical photons are drawn (EM shower filtered). Step cap is 2000 for a quick
image — raise `RADICAL_OPT_MAXSTEP` for fuller propagation.

### Colour key (beam's-eye view, +x right, +y up)

| Quadrant | Position | Colour | Corner copy |
|----------|----------|--------|-------------|
| top-right    | (+x, +y) | red    | 0 |
| top-left     | (−x, +y) | yellow | 1 |
| bottom-right | (+x, −y) | green  | 2 |
| bottom-left  | (−x, −y) | blue   | 3 |
| light born outside the shower-max corners | — | dim grey | — |

## Quantitative: position → SiPM maps

Random-uniform beam over the face, scored into:

- `H2[16-19] CornerLightMap_{TR,TL,BR,BL}` — beam `(x,y)` weighted by the photons
  that corner's SiPMs detected (where the shower must sit to fire that corner).
- `H2[20] Quadrant_vs_Corner` — beam quadrant (row) vs detecting corner (col),
  photon-weighted. Diagonal-dominant = the nearest corner sees the most light.
- `H2[21] BeamHitMap` — beam impacts, unweighted (sampling-uniformity check).

**Cluster batch** (many cores, auto-merge + plot):
```bash
conda deactivate
bash run_origin_batch.sh 4096                       # 4096 random-beam events
RADICAL_BEAM_ENERGY_GEV=20 bash run_origin_batch.sh 20000
```

**Plot** (needs ROOT) — 4 corner light maps + the response matrix, with a printed
row-normalized percentage table:
```bash
root -l -b -q 'analysis/plot_photon_origin.C("build/radical_output.root")'
# -> build/plots/corner_light_maps.png, build/plots/quadrant_vs_corner.png
```

Lower beam energy → more localized shower → sharper position sensitivity; higher
energy → wider shower → more transverse light sharing. Scan `RADICAL_BEAM_ENERGY_GEV`
to see the trade-off.

## Also kept: the cross-talk matrix

The origin cross-talk score (`H2[15] SiPM_vs_OriginCorner`, plotted by
`analysis/plot_sipm_origin.C`) still fills — it answers the complementary question
of which corner each *detected photon was born in*, independent of beam position.
```
