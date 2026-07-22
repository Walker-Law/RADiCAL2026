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

**All of these are now directly confirmed against the paper's own text**
(`papers/2401.01747v1.pdf`, p.3, `pdftotext -layout`), not just cross-checked
against DURU: "the capillaries were of 183 mm length, having an outer diameter
of 1150 um and an inner (core) diameter of 950 um. Within each core, a DSB1 WLS
filament of 900 um diameter and 15 mm length was positioned in the region of
shower max ... The remainder of each core was filled and fused with quartz
rods ... The central hole in the module ... was unused in these studies."

**Two of our recent changes are independently confirmed by DURU:**
- **183 mm capillaries protruding to external SiPMs** (`capLength = 183.*mm`).
- **No center capillary** — corner holes only (paper: the 5th hole "was unused").

Both were adopted here on 2026-07-22.

**Timing method also confirmed verbatim.** Paper §5.2: "fixed thresholds were
set for the high-gain signals... When the leading edge of the high-gain pulses
from a given channel exceeded the threshold, the timing... was determined,"
and the reported resolution uses `(ΔtDW − ΔtUP)/2` (four-downstream-mean minus
four-upstream-mean, halved) specifically because it "gives a result which is
independent of" the MCP reference. This is exactly `leadingEdgeFixed()` +
the 4-capillary corner-mean estimator already implemented for `paperJ` — no
further estimator changes needed, DURU doesn't attempt this at all (no
test-beam line -> no fixed-threshold model, no MCP, no DW/UP split; DURU's
`SiPMHit` records raw `GlobalTime`/`LocalTime`, no digitizer or threshold
model, so its EventAction histograms are direct arrival-time distributions).

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

## DURU has 4 variants, not 1 — structure (re-surveyed 2026-07-22)

All 4 share `ActionInitialization/PrimaryGeneratorAction/RunAction/SiPMHit/
SiPMSD/SteppingAction/TrackingAction` near-verbatim (checksummed identical or
near-identical); only `DetectorConstruction.cc` differs meaningfully:

| Variant | Corners | Filament layout | Readout |
|---|---|---|---|
| `Radical_ShowerMax` | 4, square | ALL 4 short (~15.1mm @ layers 8-11), T-type | 8 SiPM, both ends |
| `Radical_EnergyMode` | 4, square | ALL 4 full-length (183mm), E-type | 4 SiPM downstream only + upstream mirror plug |
| `Radical_2x2` | 4, square | MIXED: corners 0,3 full-length (E), corners 1,2 short (T) | 6 SiPM (2 downstream-only E + 4 T both ends) |
| `SixSides` | 6, hexagonal | alternating full/short around 6 corners | scales the above to a hexagon (`G4Polyhedra`) |

This matters for the earlier email-list work: findings scoped to `ShowerMax`
(filament position, SiPM PDE placeholder, BCF-92) generalize to all 4, since
those constants are copy-pasted unchanged across variants (verified: `capLength`,
`capOuterRadius`, `capInnerRadius`, `holeInset`, `filamentRadius`, `couplingGap`,
`sipmPDE` are byte-identical in every variant's `DetectorConstruction.cc`). Only
the *topology* (which corners get which capillary type, square vs hex) differs.

## What DURU does better (adopted / to consider)

1. **SiPM optical coupling** — `couplingGap = 0.0*mm`, "direct optical contact:
   no vacuum gap, so guided". Our pre-183mm geometry had 0.2032 mm of AIR at the
   rod end, which total-internally-reflects the high-angle guided light
   (quartz->air critical angle 43.2 deg). Cost: ~2x the WLS light.
   **FIXED here** as a side effect of the 183 mm rebuild (PD now abuts the tip).
2. 183 mm capillaries + no center capillary (both adopted, see above).
3. Filament radius 0.45mm (900um dia) — exact paper match, same as ours.
4. **Fully solid rod outside the WLS band** (see retraction above) — same TIR
   design as ours: filament (0.45mm) sits inside a 0.475mm bore, leaving the
   ~25um vacuum annulus that lets light TIR down the fiber instead of leaking
   straight into the surrounding quartz at first contact.

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
   not the paper's DSB1 (peak 495nm, tau=3.5ns per paper p.2). Behaviour is close
   (492 vs 495 nm, 2.7 vs 3.5 ns) but it is not the paper's material, and the
   faster 2.7ns decay will make DURU's pulses systematically sharper/faster than
   the real DSB1 response — the opposite direction of error from discrete_sims'
   40ns LYSO overestimate, but still a real-vs-model gap worth flagging.
3. **SiPM PDE = 100%, explicitly a placeholder** —
   `sipmPDE = 1.00; // 100% efficiency: register every re-emitted photon`. DURU
   knows this (their own comment calls it a placeholder spec), but it means any
   DURU photon-count or sigma_t-vs-light-yield number is ~3x optimistic vs a
   real Hamamatsu HDR2-class device (~30-36% PDE at 495nm, same ballpark as the
   MicroFJ-30035-TSV 36% this repo uses). Worth flagging alongside the
   BCF-92/DSB1 point since both push detected light and pulse sharpness in the
   same "too good" direction.
4. LYSO 32000 ph/MeV / 41 ns (vs paper-implied ~33200 / 36 ns datasheet values
   this repo uses); quartz ABSLENGTH flat 2m (vs 5-10m here, more representative
   of fused silica's real multi-meter transmission). Minor, doesn't change the
   physics discipline (both repos correctly treat SCINTILLATIONYIELD as an
   MC-thinning knob, not a claimed real yield).
5. **Filament axial position is a judgment call, not settled** — DURU's
   `showerMaxFirstLayer=8, showerMaxLastLayer=11` spans 4 plates (~15.1mm,
   layers 8-11). The paper's own Fig.7 caption says the *real hardware*
   filament was "optimized for earlier studies at Fermilab FTBF... where shower
   max occurred in layers 8-10, adequate (although not optimized) for higher
   energy showers which occur deeper... in layers 11-13, and will be corrected
   in future work." I.e. the actual beam-test filament sat at a FIXED position
   somewhere around layers 8-10, un-corrected, while the beam-test data spans
   25-150 GeV (shower max deepening with energy). DURU's 8-11 span is a
   defensible compromise, not an error — but it's not a literal reproduction of
   either the FTBF-optimized position or the un-corrected higher-energy case.
   Worth a clarifying question to THolm144/DURU authors rather than a bug report.
6. No test-beam line -> no MCP reference, so the paper's MCP-referenced fixed-
   threshold + DW/UP corner-mean timing method (confirmed above) cannot be
   reproduced there; `SiPMHit` stores raw arrival/flight times with no
   threshold, digitizer, or amplifier-jitter model at all.

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
