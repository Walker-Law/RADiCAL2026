# RADiCAL Optical Photon Locator

A focused Geant4 viewer that colours optical-photon trajectories by **which corner
capillary they were born in**, so you can see where the light in the RADiCAL
timing capillaries comes from. Derived from `RADiCALsim1` — same geometry, but the
four corner capillaries are built as distinctly-named per-corner volumes so
`drawByOriginVolume` can paint each corner a different colour.

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

A bare run defaults the beam to 2 GeV, turns optical photons on, caps optical
photon steps (`RADICAL_OPT_MAXSTEP=200`, so trapped photons can't stall the
event), and opens `vis_corners.mac`. Override any of these with env vars, e.g.
`RADICAL_BEAM_ENERGY_GEV=5 ./radical` for denser light.

## Colour key (beam's-eye view, +x right, +y up)

| Corner | Position | Colour |
|--------|----------|--------|
| top-right    | (+x, +y) | red |
| top-left     | (−x, +y) | yellow |
| bottom-right | (+x, −y) | green |
| bottom-left  | (−x, −y) | blue |
| born elsewhere (centre cap, etc.) | — | dim grey |

Only optical photons are drawn (the EM shower is filtered out for clarity). Edit
the colours, viewpoint, or filter in `vis_corners.mac`.

## How the per-corner colouring works

`src/DetectorConstruction.cc` places the corner capillary segments via a
`placeCornerSegment` helper that names each `"<base>_<corner>"` (e.g.
`Cap_Corner_WLS_0`). The copy number is kept equal to the corner index so timing
scoring is unchanged. `src/SteppingAction.cc` routes WLS energy by substring match
on `Cap_Corner_WLS`. `vis_corners.mac` maps each corner's volumes to a colour with
`/vis/modeling/trajectories/create/drawByOriginVolume`.
