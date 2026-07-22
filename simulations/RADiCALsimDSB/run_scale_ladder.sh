#!/usr/bin/env bash
# PHOTOSTATISTICS SCALE LADDER
#
# Purpose: decompose the timing stochastic term into a light-DEPENDENT
# (photostatistics) part and a light-INDEPENDENT (shower-development /
# geometric) part, so sigma_t can be extrapolated ANALYTICALLY to the true
# light yield instead of being tuned to match the paper.
#
#   a^2(f) = A^2/f + B^2
#     f = coherent light multiplier (1 = the paperJ configuration)
#     A = photostatistics coefficient at f=1   (scales as 1/sqrt(light))
#     B = light-INDEPENDENT floor              (does not move with light)
#
# WHY BOTH YIELD KNOBS MOVE TOGETHER:
# The scint-only timing population is self-scint (cat 1) + OpWLS (cat 2).
# RADICAL_LYSO_SCINT_SCALE thins ONLY the LYSO->WLS chain (cat 2);
# RADICAL_SCINT_YIELD scales ONLY the DSB1 self-scintillation (cat 1).
# At the paperJ point (LYSO 1e-2, SCINT_YIELD 1.0) the mix is ~30% WLS /
# ~70% self-scint, because WLS is thinned 100x while self-scint is NOT thinned
# at all. Moving only one knob would change the COMPOSITION as well as the
# total, confounding the measurement. Scaling BOTH by f keeps the composition
# fixed and varies only the total photon count -> a clean 1/sqrt(N) lever.
#
# (Separate known issue, deliberately NOT corrected here: that ~30/70 mix is
# itself unphysical. Unthinned, the WLS chain would be ~100x larger and would
# dominate at ~98%. This ladder measures the SCALING law; fixing the
# composition is a different change.)
#
# Everything else is pinned to the paperJ configuration so f is the only
# variable across the ladder.
#
#   bash run_scale_ladder.sh [NEVT]     (default 1000)
#
# Survive disconnect:  setsid nohup bash run_scale_ladder.sh 1000 > lad.log 2>&1 &
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; cd "$HERE"
NEVT=${1:-1000}

# Coherent light multipliers relative to paperJ. f=1 is re-run (not reused from
# paperJ) so every ladder point comes from the same binary and run conditions.
FACTORS=${RADICAL_LADDER_FACTORS:-"0.1 0.3 1 3"}

echo "Scale ladder: NEVT=$NEVT/energy, factors: $FACTORS"
echo "  (f=1 == paperJ config: LYSO_SCINT_SCALE=1e-2, SCINT_YIELD=1.0)"
echo ""

for F in $FACTORS; do
    LY=$(awk "BEGIN{printf \"%g\", 1e-2*$F}")
    SY=$(awk "BEGIN{printf \"%g\", 1.0*$F}")
    echo "============================================================"
    echo " Ladder point f=$F   LYSO_SCINT_SCALE=$LY   SCINT_YIELD=$SY"
    echo "============================================================"
    RADICAL_OPT_MAXSTEP=5000 RADICAL_OPT_TMAX=50 \
    RADICAL_LYSO_SCINT_SCALE="$LY" RADICAL_SCINT_YIELD="$SY" \
    RADICAL_QUARTZ_CHER_KEEP=0.01 \
    RADICAL_SIPM_NPIX=5676 RADICAL_SPTR_PS=60 \
    RADICAL_ELEC_JITTER_PS=50 RADICAL_SM_COG_CUT_MM=2 \
    RADICAL_MAX_OPT_PHOTONS=80000000 \
    RADICAL_RUN_TAG="lad${F}" \
        bash run_scan.sh "$NEVT" 1
done

echo ""
echo "Ladder complete. Decompose with:"
echo "  root -l -b -q 'analysis/plot_scale_ladder.C($NEVT)'"
