# RADiCAL2026

Research repository for the **RADiCAL** (Radiation-hard Innovative Calorimeter) project.
Based on [[1]](https://arxiv.org/abs/2303.05580) and [[2]](https://arxiv.org/abs/2401.01747).

## References

1. V. Beresovskyi et al., *"RADiCAL: a Radiation-hard Innovative Calorimeter"*, arXiv:[2303.05580](https://arxiv.org/abs/2303.05580) (2023).
2. C. Perez-Lara et al., *"Study of time resolution measurements and prospects for energy resolution of an ultra-compact sampling calorimeter (RADiCAL)"*, NIM A **1068** (2024) 169737, arXiv:[2401.01747](https://arxiv.org/abs/2401.01747).

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
