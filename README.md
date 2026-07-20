# RADiCAL2026

Research Geant4 simulation repository for the RADiCAL project.
Based on [[1]](https://arxiv.org/abs/2303.05580) and [[2]](https://arxiv.org/abs/2401.01747).

## References

1. V. Beresovskyi et al., *"RADiCAL: a Radiation-hard Innovative Calorimeter"*, arXiv:[2303.05580](https://arxiv.org/abs/2303.05580) (2023).
2. C. Perez-Lara et al., *"Study of time resolution measurements and prospects for energy resolution of an ultra-compact sampling calorimeter (RADiCAL)"*, NIM A **1068** (2024) 169737, arXiv:[2401.01747](https://arxiv.org/abs/2401.01747).

## Repository layout

```
RADiCAL/                     Test-beam data and ROOT analysis
  Data/                      DRS4 waveform files (RUN1211, RUN1259–1261)
  Analysis/                  ROOT macros for data analysis
  legacy/                    Older analysis code (reference)

simulations/                 Geant4 simulations
  RADiCALsimDSB/             ★ Primary full-module sim — DSB1 WLS fiber (fast,
                               3.5 ns decay). Solid quartz capillary rods (the
                               paper's "hollow tube" core is filled and fused
                               with quartz — an earlier air-bore model broke
                               light transport and was reverted), optically
                               active LYSO→DSB1 WLS chain, dual-gain SiPM readout
                               (onsemi MicroFJ-30035), full CERN test-beam line,
                               DRS4 waveform emulation. Actively developed.
  RADiCALsimLuAG/            Identical geometry with LuAG:Ce WLS fiber (slow,
                               60 ns decay) — material comparison against DSB.
  RADiCALsimFig8/            Focused recreation of ref [2] Fig. 8: single
                               downstream-SiPM timing resolution vs detected
                               light yield (npe/MeV), 50 GeV e⁻, rise-time (CFD)
                               method. LY sweep via run_fig8_sweep.sh.
  RADiCALsimHoleScan/        Light output at the capillary ends vs the diameter
                               of the tile holes (1.2–2.0 mm sweep, 50 GeV); all
                               five capillaries scale to fill their hole. Center
                               EJ309 capillary made optically active (with its
                               own end PDs) so it counts as the 5th light source
                               alongside the 4 corner WLS caps. run_hole_scan.sh.
  RADiCALphotonorigin/       Position→SiPM mapping study (where the beam hits vs
                               which corner SiPM lights up; S-curve reconstruction).
  RADiCALopticalcrosstalk/   Optical-photon trajectory viewer, colored by
                               originating corner capillary (crosstalk visual).
  H3generation/              Standalone generation study.
  firstsim/                  Early prototype (reference only).
```

The four `RADiCALsim{DSB,LuAG,Fig8,HoleScan}` directories share the same core
Geant4 code (they were forked from a common base) and differ in the WLS
material, geometry parametrization, observables, and study focus. `RADiCALsimDSB`
is the canonical, most up-to-date version; the others inherit fixes from it.

## Quick start

```bash
cd simulations/RADiCALsimDSB
source setup_env.sh            # sources Geant4 (auto-detects conda `g4` env)
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=$CONDA_PREFIX \
         -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX
make -j$(nproc)
./radical ../run_batch.mac    # 120 GeV electrons

# energy/timing scan across energies (embarrassingly parallel, all cores):
RADICAL_OPTICAL=1 RADICAL_ENERGIES="5 10 25 50 100 120" \
  bash ../run_scan.sh 2000 1
```

Optical-photon tracking is enabled with `RADICAL_OPTICAL=1` and is far slower
than the energy-only mode; the scan splits events into many single-thread chunks
across all cores and hadd-merges them per energy.

See [simulations/RADiCALsimDSB/README.md](simulations/RADiCALsimDSB/README.md)
and its `CLAUDE.md` for full detector, physics, and workflow documentation.

## Key runtime knobs (RADiCALsimDSB / Fig8)

| Env var | Purpose |
|---------|---------|
| `RADICAL_OPTICAL` | 1 = track optical photons (timing); 0 = energy/shower only (fast) |
| `RADICAL_BEAM_ENERGY_GEV` | Beam energy per run |
| `RADICAL_ENERGIES` | Space-separated energy list for `run_scan.sh` |
| `RADICAL_LYSO_SCINT_SCALE` | Scale LYSO scintillation yield (tractability; default 1e-3) |
| `RADICAL_SCINT_YIELD` | Scale DSB1 self-scintillation yield |
| `RADICAL_QUARTZ_CHER_KEEP` | Binomial-thin quartz Cherenkov (restore real scint:Cher ratio) |
| `RADICAL_SIPM_NPIX` | SiPM microcell count (default 5676 = MicroFJ-30035) |
| `RADICAL_SPTR_PS` | SiPM single-photon time resolution RMS (ps) |
| `RADICAL_OPT_MAXSTEP` / `RADICAL_OPT_TMAX` | Optical-photon step / time caps (tractability) |
