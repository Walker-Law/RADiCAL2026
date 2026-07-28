#!/usr/bin/env bash
# run_ladder.sh — photostatistics scale ladder for the RADiCAL timing sim.
#
# This is a STUDY that reuses the RADiCALsimDSB simulation (same binary, same
# paper-fidelity geometry) — it is not a separate detector. It sweeps a coherent
# light multiplier f and records how the timing resolution scales, so the
# stochastic term can be extrapolated to true light instead of tuned.
#
# WHAT IT VARIES: at each rung, ALL THREE light sources are thinned by the same
# f (LYSO->WLS chain, DSB1 self-scint, quartz Cherenkov). The light MIX stays
# physical at every rung, so f is a pure intensity lever and the fit
# a^2(f)=A^2/f+B^2 cleanly separates photon-counting (A) from the light-
# independent floor (B). Base config = "paperR" (realistic composition +
# 2026-07-23 waveform-realism electronics chain). ELEC_JITTER_PS is NOT set:
# amplifier noise is now modeled in the waveform (RADICAL_ELEC_NOISE_MV);
# the old ad-hoc knob would double-count it.
#
#   cd RADiCALsimLadder && bash run_ladder.sh [NEVT]      (default 500)
#
# Runs are SEQUENTIAL (each scan saturates the machine). Output lands in the
# DSB build (that is where the shared run_scan.sh writes) and is then copied
# into ./scan/ so this folder stays self-contained. Analyze with:
#   root -l -b -q 'analysis/ladder.C(NEVT)'
set -u

# --- standard logging: writes to <sim>/build/logs/<script>.log --------------
# Repo-wide convention, see simulations/README_LOGGING.md. No redirect needed:
#   nohup bash THIS_SCRIPT.sh &     ->   tail -f build/logs/<script>.log
. "$(cd "$(dirname "$0")" && pwd)/../lib/run_logging.sh"
start_logging "$(cd "$(dirname "$0")" && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"
DSB="$HERE/../RADiCALsimDSB"
NEVT=${1:-500}
FACTORS=${RADICAL_LADDER_FACTORS:-"0.1 0.3 1 3"}
BASE=1e-2                                   # paperR thinning at f=1

[ -x "$DSB/build/radical" ] || {
    echo "Build the DSB binary first: (cd $DSB/build && source ../setup_env.sh && make -j)"; exit 1; }

mkdir -p "$HERE/scan"
for f in $FACTORS; do
    LY=$(awk "BEGIN{printf \"%g\", $BASE * $f}")
    echo "=============================================================="
    echo " LADDER POINT f=$f  ->  all light knobs = $LY"
    echo "=============================================================="
    ( cd "$DSB/build" && source ../setup_env.sh >/dev/null 2>&1 && \
      RADICAL_OPTICAL=1 RADICAL_ENERGIES="25 50 75 100 125 150" \
      RADICAL_LYSO_SCINT_SCALE=$LY RADICAL_SCINT_YIELD=$LY RADICAL_QUARTZ_CHER_KEEP=$LY \
      RADICAL_MAX_OPT_PHOTONS=60000000 \
      RADICAL_SPTR_PS=60 RADICAL_SIPM_NPIX=5676 RADICAL_SM_COG_CUT_MM=2 \
      RADICAL_ROD_SIGMA_ALPHA_DEG=${RADICAL_ROD_SIGMA_ALPHA_DEG:-1.3} \
      RADICAL_RUN_TAG=paperR_lad$f \
      bash "$DSB/run_scan.sh" "$NEVT" 1 )
    # copy the merged result here (root files only — drop ~500 per-chunk logs)
    SRC="$DSB/build/scan/optical_scan_${NEVT}_paperR_lad$f"
    DST="$HERE/scan/optical_scan_${NEVT}_paperR_lad$f"
    if [ -d "$SRC" ]; then
        rm -rf "$DST"; mkdir -p "$DST"
        cp "$SRC"/*.root "$DST"/ 2>/dev/null
        echo "  -> results copied to $DST"
    fi
done
echo "LADDER COMPLETE. Analyze:  root -l -b -q 'analysis/ladder.C($NEVT)'"
