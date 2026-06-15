#!/bin/bash
# make_plots.sh — generate plots from an optical scan.
# Run LOCALLY (the cluster has no ROOT), AFTER rsyncing the scan dir back.
#
#   ./make_plots.sh         # defaults to the 100-event scan
#   ./make_plots.sh 1000    # the 1000-event scan
#   ./make_plots.sh 100
#
# Must be run from the RADiCALsim1 root: the analysis macros hardcode
# build/plots and build/... output paths.

set -e
cd "$(dirname "$(realpath "$0")")"   # RADiCALsim1 root

N="${1:-100}"                        # event count (100 or 1000)
SCAN="build/optical_scan_${N}"
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
