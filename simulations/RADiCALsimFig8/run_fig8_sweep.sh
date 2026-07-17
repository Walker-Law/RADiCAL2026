#!/usr/bin/env bash
# Fig 8 recreation (arXiv:2401.01747): timing resolution vs DETECTED light yield.
# Single downstream SiPM readout, 50 GeV e- shower. Sweeps the LYSO->DSB1 WLS
# yield (RADICAL_LYSO_SCINT_SCALE) to span a range of detected LY (npe/MeV);
# each point writes build/scan/optical_scan_2000_ly<scale>/optical_E50GeV.root.
# The single-ended observable is H1[38] DeltaT_SingleDown (downstream WLS pulse
# 5% CFD leading-edge time - fiber signal time = the "rise-time" resolution the
# paper describes, pure photostatistics 1/sqrt(LY)). Analysis:
#   root -l -b -q 'analysis/plot_fig8.C()'
#
# SPTR is OFF here (RADICAL_SPTR_PS=0) so the curve is the clean photostatistics
# limit Fig 8 shows (add SPTR later to see the realistic floor). Cherenkov is
# heavily thinned (cheap) — it does not enter the WLS-only single-ended timing.
#
#   bash run_fig8_sweep.sh [EVENTS_PER_POINT]   (default 2000)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; cd "$HERE"
NEVT=${1:-2000}

# Yield scales spanning ~2.5 decades -> detected LY ~1..100 npe/MeV. The top is
# capped by tractability (photon budget); the bottom is cheap.
SCALES="1e-4 3e-4 1e-3 3e-3 1e-2 2e-2"

for S in $SCALES; do
    echo "============================================================"
    echo " Fig 8 point: LYSO_SCINT_SCALE=$S   (50 GeV, $NEVT evt)"
    echo "============================================================"
    RADICAL_OPT_MAXSTEP=5000 RADICAL_OPT_TMAX=50 \
    RADICAL_LYSO_SCINT_SCALE=$S RADICAL_SCINT_YIELD=$S \
    RADICAL_QUARTZ_CHER_KEEP=0.01 \
    RADICAL_SIPM_NPIX=5676 RADICAL_SPTR_PS=0 \
    RADICAL_MAX_OPT_PHOTONS=20000000 \
    RADICAL_ENERGIES="50" RADICAL_RUN_TAG="ly${S}" \
        bash run_scan.sh "$NEVT" 1
done

echo ""
echo "All Fig 8 points done. Build the curve:"
echo "  root -l -b -q 'analysis/plot_fig8.C()'"
