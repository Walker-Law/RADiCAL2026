# archive/ — retired simulations

Sims moved here are **frozen, not deleted** — full history preserved (`git mv`,
not copy+delete). Nothing here is actively developed; treat any result in
here as a historical snapshot, not a live number.

| dir | why archived | still depended on by |
|---|---|---|
| `firstsim/` | earliest prototype, superseded by every sim in `simulations/` since. Reference only. | nothing |
| `RADiCALsimDSB/` | archived 2026-07-30 as project focus moved to the `RADiCALsimSIMPLE` family (SIMPLE, Wrap, and the light-scan study). Still the most physically complete single-module model built here (full CERN test-beam line, DRS4 waveform emulation, dual-gain SiPM) — see its own `CLAUDE.md` for the full history of findings. | **`RADiCALsimLadder`** (reuses its binary — path updated to `../archive/RADiCALsimDSB`) and `RADiCALsimFig8`/`RADiCALsimHoleScan`/`RADiCALsimLuAG` (forked from its source historically; they carry their own independent copies, no runtime dependency) |

`RADiCALsimLuAG` was **not** archived — it remains an active sibling.

To resurrect anything here: it's a normal source tree, `cd` in and build like
any other sim (`source setup_env.sh && mkdir build && cd build && cmake .. && make`).
