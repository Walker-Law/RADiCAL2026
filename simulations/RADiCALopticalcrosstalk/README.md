# RADiCAL Optical Photon Locator

A focused Geant4 viewer that colors optical-photon trajectories by **which corner
capillary they were born in**, so you can see where the light in the RADiCAL
timing capillaries comes from. Derived from `RADiCALsim1` — same geometry, but the
four corner capillaries are built as distinctly-named per-corner volumes so
`drawByOriginVolume` can paint each corner a different color.

## Build

```bash
source setup_env.sh
mkdir -p build && cd build
cmake .. && make -j$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)
```

## Run

```bash
./radical            # bare = the photon-origin viewer (2 GeV, optical ON, vis_corners.mac)
```

A bare run defaults the beam to 5 GeV, turns optical photons on, sets the optical
step cap to 200000 (effectively uncapped, so photons propagate fully for a
faithful image), and opens `vis_corners.mac`. Override with env vars, e.g.
`RADICAL_OPT_MAXSTEP=200 ./radical` for a fast (but truncated) preview.

## Color key (beam's-eye view, +x right, +y up)

Only optical photons born in the **shower-max region** (the WLS section of each
corner capillary — the LuAG:Ce fiber + surrounding quartz at depth 32.9–47.9 mm,
straddling layer 9) are colored by their corner. Light born anywhere else
(upstream/downstream quartz rods, center cap, etc.) is dim gray.

| Born in shower-max corner | Position | Color |
|--------|----------|--------|
| top-right    | (+x, +y) | red |
| top-left     | (−x, +y) | yellow |
| bottom-right | (+x, −y) | green |
| bottom-left  | (−x, −y) | blue |
| born outside the shower-max region | — | dim gray |

> Note: LYSO emits **no** optical photons in this sim — it is the energy-sampling
> absorber, scored by energy deposit only. The detected light is quartz Cherenkov
> + LuAG:Ce WLS scintillation generated **inside the capillaries**. The color
> therefore marks light created in the capillary's shower-max section, which is
> embedded among the LYSO tiles at the shower peak.

Only optical photons are drawn (the EM shower is filtered out for clarity). Edit
the colors, viewpoint, or filter in `vis_corners.mac`.

## Cross-talk graph: which corner's light hits which SiPM

Beyond the live viewer, the locator scores, for every detected photon, the SiPM
that caught it (4 corners x upstream/downstream = 8) versus the corner it was
born in. This fills `H2[15] "SiPM_vs_OriginCorner"` in `radical_output.root`.

**Fill it (one process at a time, locally):**
```bash
cd build && source ../setup_env.sh
RADICAL_OPTICAL=1 RADICAL_OPT_MAXSTEP=200 RADICAL_BEAM_ENERGY_GEV=2 ./radical run_batch.mac
```

**Plot it** (needs ROOT) — a stacked bar per SiPM, colored by origin corner, with
a printed percentage-composition table:
```bash
cd ..
root -l -b -q 'analysis/plot_sipm_origin.C("build/radical_output.root")'   # -> build/plots/sipm_origin.png
```

**Small batch on the cluster** (many cores, auto-merge + plot):
```bash
conda deactivate
bash run_sipm_batch.sh 512            # 512 events spread over all cores
# RADICAL_BEAM_ENERGY_GEV=5 bash run_sipm_batch.sh 1024   # denser / higher E
```

Each event detects thousands of photons, so even ~50 events gives a clean
composition. A diagonal result (each bar one solid color) means the capillaries
are optically isolated — each SiPM only sees its own corner's light.

## How the per-corner coloring works

`src/DetectorConstruction.cc` places the corner capillary segments via a
`placeCornerSegment` helper that names each `"<base>_<corner>"` (e.g.
`Cap_Corner_WLS_0`). The copy number is kept equal to the corner index so timing
scoring is unchanged. `src/SteppingAction.cc` routes WLS energy by substring match
on `Cap_Corner_WLS`. `vis_corners.mac` maps each corner's volumes to a color with
`/vis/modeling/trajectories/create/drawByOriginVolume`.
