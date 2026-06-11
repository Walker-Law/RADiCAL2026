# RADiCAL2026

Research repository for the **RADiCAL** project.
Based on [arXiv:2303.05580v3](https://arxiv.org/abs/2303.05580).

## Repository layout

```
RADiCAL/                  Test-beam data and ROOT analysis
  Data/                   DRS4 waveform files (RUN1211, RUN1259–1261)
  Analysis/               ROOT macros for data analysis

simulations/              Geant4 simulations
  RADiCALsim1/            Full RADiCAL module simulation (active)
  firstsim/               Early prototype (reference only)
```

## Quick start

```bash
cd simulations/RADiCALsim1
source setup_env.sh
mkdir build && cd build
cmake ..
make -j$(nproc)
./radical ../run_batch.mac   # 120 GeV electrons, 100 events
```

See [simulations/RADiCALsim1/README.md](simulations/RADiCALsim1/README.md) for full documentation.
