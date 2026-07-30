# Where everything lives

One layout, every simulation. Nothing to remember per-sim.

```
<sim>/
  macros/            *.mac        — what to run (energies, event counts)
  analysis/          *.C          — ROOT macros, run from the sim root
  src/  include/     *.cc *.hh    — the simulation itself
  *.sh                            — run/pull/stage scripts (sim root, so
                                    commands stay short: bash run_simple.sh)
  build/
    rootfiles/       *.root       — ALL simulation output
    logs/            *.log        — ALL live progress logs
    plots/           *.png        — ALL analysis output
```

Three rules, and they are the whole convention:

| I want... | it is always in |
|---|---|
| the data | `<sim>/build/rootfiles/` |
| the log I can `tail -f` | `<sim>/build/logs/` |
| the plots | `<sim>/build/plots/` |

```bash
tail -f <sim>/build/logs/*.log      # watch any run, any sim
ls      <sim>/build/rootfiles/      # find any data, any sim
open    <sim>/build/plots/          # find any plot, any sim
```

## Why build/

`build/` is gitignored, so run output never enters version control and never
collides on `git pull`. (It did once, 2026-07-29: run output had been committed
from the Mac, and pulling on perseverence aborted because that cluster had its
own files at the same paths. Hence this rule.)

## Who enforces it

- **rootfiles/** — created in C++ (`RunAction`), so it holds no matter how the
  binary is started: `./radsimple`, a run script, or a bare macro.
- **logs/** — created by `lib/run_logging.sh`, sourced by every run script.
  See [README_LOGGING.md](README_LOGGING.md).
- **plots/** — created by the analysis macros, derived from the data path
  (`build/rootfiles` -> `build/plots`).

Check any time with:

```bash
bash simulations/check_layout.sh
```

## Deliberate exceptions

| what | why |
|---|---|
| `*/setup_env.sh` | **sourced, not run** — logging it would redirect your interactive shell's stdout into a file and your terminal would go quiet. |
| `describe_runs.sh`, `make_run_manifests.sh`, `install_cluster_env.sh` | repo-level utilities, not tied to one sim, so there is no `<sim>/build/` to write to. Short and interactive. |
| `archive/RADiCALsimDSB`, `RADiCALsim{LuAG,Fig8,HoleScan}` output | these predate the convention and write per-scan trees under `build/scan/<tag>_scan_<N>/`, because a scan is hundreds of parallel chunk files plus their own `RUNS.md` manifest. Their `scan_resolution.C` and manifest tooling are wired to that path. Moving them buys little and risks breaking working analysis — **their logs still follow the standard rule**, which is the part you actually tail. |
| `RADiCALsimLadder/results/` | two hand-kept study outputs, deliberately tracked in git. |

Fully on the convention today: **RADiCALsimSIMPLE**, **RADiCALsimWrap**.
