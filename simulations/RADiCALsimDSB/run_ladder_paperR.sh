#!/usr/bin/env bash
# run_ladder_paperR.sh — scale ladder on the REALISTIC (paperR) composition.
#
# Coherently thins ALL THREE light sources by f (LYSO chain, DSB1 self-scint,
# quartz Cherenkov), so the light MIX stays physical at every rung and f is a
# pure intensity lever. Fit a^2(f) = A^2/f + B^2 (analysis/ladder_paperR.C) and
# extrapolate to true light (f = 1/base_scale = 100): that number — not any
# single thinned point — is what stands beside the paper's 256 ps/sqrt(E).
#
# Base config = paperR (paper183 geometry + realistic composition + the
# 2026-07-23 waveform-realism chain). ELEC_JITTER_PS is NOT set: amplifier
# noise is now modeled explicitly in the waveform (RADICAL_ELEC_NOISE_MV);
# keeping the old ad-hoc 50 ps knob would double-count it.
#
#   cd build && bash ../run_ladder_paperR.sh [NEVT]     (default 500)
#
# Runs are SEQUENTIAL (each scan saturates the machine on its own). Output:
#   scan/optical_scan_<NEVT>_paperR_lad<f>/   for f in the ladder.
set -u
NEVT=${1:-500}
FACTORS=${RADICAL_LADDER_FACTORS:-"0.1 0.3 1 3"}
BASE=1e-2                                  # paperR thinning at f=1

for f in $FACTORS; do
    LY=$(awk "BEGIN{printf \"%g\", $BASE * $f}")
    echo "================================================================"
    echo " LADDER POINT f=$f  ->  all light knobs = $LY"
    echo "================================================================"
    RADICAL_OPTICAL=1 RADICAL_ENERGIES="25 50 75 100 125 150" \
    RADICAL_LYSO_SCINT_SCALE=$LY RADICAL_SCINT_YIELD=$LY RADICAL_QUARTZ_CHER_KEEP=$LY \
    RADICAL_MAX_OPT_PHOTONS=60000000 \
    RADICAL_SPTR_PS=60 RADICAL_SIPM_NPIX=5676 RADICAL_SM_COG_CUT_MM=2 \
    RADICAL_RUN_TAG=paperR_lad$f \
    bash "$(dirname "$0")/run_scan.sh" "$NEVT" 1
done
echo "LADDER COMPLETE. Analyze with:"
echo "  root -l -b -q 'analysis/ladder_paperR.C($NEVT)'"
