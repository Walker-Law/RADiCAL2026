# Reproducible cluster setup (Geant4 + ROOT)

One conda environment provides the **entire** toolchain — Geant4 11.4.2 (with all
physics data), ROOT 6.40 (hadd + analysis), an ABI-matched C++ compiler, and
cmake — identically on any linux-64 cluster. No sudo, no source builds, no manual
Geant4 data download, no `setup_env.sh` path-juggling.

## Install on a new cluster (one command)

```bash
cd simulations
bash install_cluster_env.sh        # installs Miniforge + the 'radical' env
```

Then, in a new shell:

```bash
conda activate radical
geant4-config --version            # -> 11.4.2
which hadd root                     # -> .../miniforge3/envs/radical/bin/...
```

## Build & run any project

With `radical` activated, the conda env supplies Geant4 (data paths set
automatically), ROOT, and the compiler — so just build normally:

```bash
cd RADiCALsim1                      # or RADiCALopticalphotonlocator
mkdir -p build && cd build
cmake .. && make -j$(nproc)
cd ..
# run scripts work as-is; hadd/root come from the env:
bash run_scan.sh 1000
```

> The per-project `setup_env.sh` is **no longer needed** when the `radical` env is
> active — Geant4 and its data paths come from conda. The run scripts still source
> it harmlessly (it just re-derives the same conda Geant4).

## Why this is identical across clusters

- `environment.yml` pins **exact versions** (`geant4=11.4.2`, `root=6.40.2`), and
  conda-forge geant4 pins all 13 `geant4-data-*` packages — so every cluster
  resolves the same binaries and the same physics data.
- Run `install_cluster_env.sh` on each cluster (including the current one, to
  retire its source-built Geant4 + separate ROOT) and they match bit-for-bit at
  the package level.

## Want true bit-identical (gold standard)?

For guaranteed-identical execution regardless of host libraries, wrap the same
`environment.yml` in an **Apptainer/Singularity** image (the HPC-standard,
rootless container). Build once, `apptainer exec radical.sif <cmd>` everywhere.
Ask and I'll add the `Apptainer.def` recipe — but for "simple + reproducible," the
conda env above is the right tool.
