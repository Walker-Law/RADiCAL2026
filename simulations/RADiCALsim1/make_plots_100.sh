#!/bin/bash
# make_plots_100.sh — generate plots from the 100-event optical scan.
# Run LOCALLY (the cluster has no ROOT), AFTER rsyncing optical_scan_100/ back.
#
#   ./make_plots_100.sh
#
# Must be run from the RADiCALsim1 root: the analysis macros hardcode
# build/plots and build/... output paths.

set -e
cd "$(dirname "$(realpath "$0")")"   # RADiCALsim1 root

SCAN="build/optical_scan_100"
ENERGIES=(5 10 20 50 100 120)

if ! command -v root >/dev/null 2>&1; then
    echo "ERROR: 'root' not found in PATH. Source thisroot.sh first." >&2
    exit 1
fi
if [ ! -d "$SCAN" ]; then
    echo "ERROR: $SCAN not found. rsync the scan results from the cluster first." >&2
    exit 1
fi

mkdir -p build/plots

# 1. Timing resolution vs detected light yield (all energies, one plot)
root -l -b -q "analysis/plot_timing_vs_LY.C(\"${SCAN}\")"

# 2. Per-energy testbeam plots: energy res, timing res, shower profiles
for E in "${ENERGIES[@]}"; do
    ROOTFILE="${SCAN}/optical_E${E}GeV.root"
    if [ -f "$ROOTFILE" ]; then
        root -l -b -q "analysis/plot_testbeam.C(\"${ROOTFILE}\",${E})"
    else
        echo "skip E=${E} GeV (missing $ROOTFILE)"
    fi
done

echo "Plots saved to build/plots/"
ls -lh build/plots/
