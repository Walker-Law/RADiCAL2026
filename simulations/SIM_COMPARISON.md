# Cross-simulation comparison — RADiCALsimDSB vs external RADiCAL simulations

Three independent RADiCAL simulations now exist. This records how they differ,
which one is right where they disagree, and what each got wrong versus
**arXiv:2401.01747**. Written 2026-07-22.

| | **RADiCALsimDSB** (this repo) | **discrete_sims** (THolm144) | **geant4-DURU-models** |
|---|---|---|---|
| Toolkit | Geant4 11.4 / C++ | OpenGATE / Python | Geant4 / C++ (from the `B4d` example) |
| Independent? | — | **No** — cites this repo's files in its material comments | **Yes** — independent implementation |
| Variants | DSB1, LuAG siblings | DSB1 x LuAG:Ce, x4 geometries | ShowerMax, EnergyMode, 2x2, SixSides |
| Test-beam line | yes (trigger, MCP, Pb-glass) | no | no |

## Where all three agree (mutual validation)

29 LYSO 1.5 mm + 28 W 2.5 mm, 14 mm tiles, Tyvek 0.2032 mm; capillary OD/bore
1150/950 um; DSB1 filament 900 um; corner inset 3.5 mm.

**Two of our recent changes are independently confirmed by DURU:**
- **183 mm capillaries protruding to external SiPMs** (`capLength = 183.*mm`).
- **No center capillary** — corner holes only (paper: the 5th hole "was unused").

Both were adopted here on 2026-07-22.

**Photon thinning is universal.** DURU independently hit the same intractability
and adopted the same remedy, with the same discipline we arrived at the hard way:
```cpp
lysoMPT->AddConstProperty("SCINTILLATIONYIELD", (32000./kPhotonScaleDown)/MeV);
// "...OOM-killed. We therefore track a reduced yield = 32000/kPhotonScaleDown
//  and correct by kPhotonScaleDown in the analysis."
```
Their default 100x is the same order as our `RADICAL_LYSO_SCINT_SCALE=1e-2`.
Independent confirmation that **thinning + analytic correction** is the right
architecture, and that the thinning factor is NOT a physics parameter.

## What DURU does better (adopted / to consider)

1. **SiPM optical coupling** — `couplingGap = 0.0*mm`, "direct optical contact:
   no vacuum gap, so guided". Our pre-183mm geometry had 0.2032 mm of AIR at the
   rod end, which total-internally-reflects the high-angle guided light
   (quartz->air critical angle 43.2 deg). Cost: ~2x the WLS light.
   **FIXED here** as a side effect of the 183 mm rebuild (PD now abuts the tip).
2. 183 mm capillaries + no center capillary (both adopted, see above).
3. Filament at layers 8-11 (~15.1 mm), squarely on the paper's Fig 7 shower max.

## What DURU gets wrong (worth reporting upstream)

1. **~~HOLLOW capillary core~~ — RETRACTED, this was a misread.** Earlier notes
   here claimed `capInnerRadius` left the whole 183 mm rod hollow (an air bore),
   citing the "100 um quartz wall" comment out of context. Re-read on 2026-07-22,
   full `DefineVolumes()`:
   ```cpp
   auto quartzRodS = new G4Tubs("QuartzRod", 0., capOuterRadius, capLength/2, ...);
   auto quartzCoreS = new G4Tubs("QuartzCore", 0., capInnerRadius,
                                  filamentLength/2, ...);   // NOTE: filamentLength (~15mm), not capLength
   auto quartzS = new G4SubtractionSolid("Quartz", quartzRodS, quartzCoreS,
                                          nullptr, G4ThreeVector(0,0,filamentCenterZ));
   ```
   The bore subtracted from the solid rod is only `filamentLength/2` long (~7.5 mm
   half-length), not `capLength/2`. Outside that short shower-max band the
   `Quartz` logical volume is the full solid `capOuterRadius` rod — exactly the
   paper's "filled and fused with quartz rods" everywhere except the WLS region.
   Inside the band, the bore is filled by the BCF-92 filament (placed separately,
   radius `filamentRadius`=0.45mm < `capInnerRadius`=0.475mm, so a ~25 um vacuum
   annulus separates filament from quartz wall — the same TIR-preserving air gap
   our own `wlsFiberR`/`tCap_boreR` pair uses). **DURU's rod is solid outside the
   filament, exactly like the paper and exactly like ours.** The error was mine:
   I read the `capInnerRadius` comment ("100 um quartz wall") as describing the
   whole rod rather than the localized shower-max bore, the same class of mistake
   as the earlier DSB1/LuAG misread — parse the geometry construction, not an
   isolated comment.
2. **Wrong WLS material** — the filament is **BCF-92** (polystyrene, n=1.60,
   `WLSTIMECONSTANT 2.7 ns`, emission 492 nm), a Saint-Gobain commercial fiber,
   not the paper's DSB1. Behaviour is close (492 vs 495 nm, 2.7 vs 3.5 ns) but
   it is not the paper's material.
3. LYSO 32000 ph/MeV / 41 ns (vs datasheet 33200 / 36 ns); quartz ABSLENGTH 2 m
   (vs 5-10 m for fused silica). Minor.
4. No test-beam line -> no MCP reference, so the paper's MCP-referenced timing
   method cannot be reproduced there.

## What discrete_sims gets wrong

1. **Inherited stale calibrations from THIS repo, as "physics"** — its
   `Materials.xml` hardcodes DSB1 self-scint = 700 ph/MeV justified by our old
   `SCINT_YIELD=0.07` tuning and "the paper's ~25 npe/MeV", **a number that is
   not in the paper** (verified 2026-07-21), and which our own notes now retract.
   It also bakes LYSO = 32 ph/MeV — our 1000x *thinning factor* — into the
   material definition as if the crystal really produced 32 ph/MeV, with no
   analytic-correction machinery. Their sigma_t cannot be compared to the paper
   at face value until that is undone.
2. Filament 19.5 mm at layers 5-9 (paper: 15 mm at ~40.4 mm / layers 8-10) —
   too long and too shallow.
3. LYSO attenuation 3.1-3.8 cm (this repo and DURU both use ~40 cm).
4. 20%-of-peak threshold, where the paper uses a fixed ABSOLUTE threshold on the
   high-gain leading edge — a distinction that mattered enormously in the Fig 8
   work (a fixed *fraction* is scale-invariant and cannot improve with light).
5. Corner holes at 3.7 mm (inset from the Tyvek-padded envelope) vs 3.5 mm.

**Correction to an earlier reading:** discrete_sims' DSB1 is *correct* — 494 nm
emission, n=1.57, 3.5 ns. An initial pass misread the adjacent LuAG block in the
same file (535 nm, n=1.84) as DSB1's. Every one of these files defines both
materials; parse block boundaries, not grep context.

## Standing items for this repo

- Consider DURU's SiPM window/PDE treatment vs our flat `kQE = 0.36`.
- Our LYSO is optically active and theirs is too, but our WLS/self-scint
  *composition* is still wrong (~70% self-scint where reality is ~98% WLS) —
  see `RADiCALsimDSB/CLAUDE.md`, "COMPOSITION BUG".
