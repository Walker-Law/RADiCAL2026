#!/bin/bash
# make_plots.sh — generate the full paper-matching plot set from an optical scan.
# Run LOCALLY (the cluster has no ROOT), AFTER rsyncing the scan dir back.
#
#   ./make_plots.sh         # defaults to the 1000-event scan
#   ./make_plots.sh 1000    # the 1000-event scan
#   ./make_plots.sh 100     # the 100-event scan
#
# Each scan's plots land in their own folder so 100 and 1000 stay separate:
#   build/plots/optical_scan_<N>/
#
# Plots produced (cf. arXiv:2401.01747):
#   Summary curves (all energies):
#     energy_resolution_curve.png   sigma_E/E vs E, fit a/sqrtE (+) b   [Fig 17R / Eq 1]
#     timing_resolution_curve.png   sigma_t vs E,  fit a/sqrtE (+) b    [Fig 27R / Eq 2]
#     shower_long_overlay.png       longitudinal profiles, all E        [Fig 7]
#     energy_linearity.png          E_reco vs E_beam + residuals        [Fig 17L]
#     energy_response.png           E_reco/E_beam uniformity
#     timing_vs_LY_optical_scan_<N>.png  sigma_t vs light yield         [Fig 8]
#   Per energy (5/10/20/50/100/120 GeV):
#     energy_resolution_E<E>.png    energy peak + Gaussian fit          [Fig 16]
#     timing_resolution_E<E>.png    DeltaT peak + Gaussian fit          [Fig 20]
#     shower_long_E<E>.png          longitudinal profile
#     shower_lat_E<E>.png           lateral profile
#
# Must be run from the RADiCALsim1 root: the analysis macros hardcode build/plots.

set -e
cd "$(dirname "$(realpath "$0")")"   # RADiCALsim1 root

N="${1:-1000}"                       # event count (1000 or 100)
SCAN="build/optical_scan_${N}"
DEST="build/plots/optical_scan_${N}"
ENERGIES=(5 10 20 50 100 120)

if ! command -v root >/dev/null 2>&1; then
    echo "ERROR: 'root' not found in PATH. Source thisroot.sh first." >&2
    exit 1
fi
if [ ! -d "$SCAN" ]; then
    echo "ERROR: $SCAN not found. rsync the scan results from the cluster first." >&2
    exit 1
fi

mkdir -p build/plots "$DEST"
# Clear any stale top-level PNGs so we only collect THIS run's output.
rm -f build/plots/*.png

# 1. Summary resolution curves: sigma_E/E vs E, sigma_t vs E, longitudinal overlay
root -l -b -q "analysis/scan_resolution.C(\"${SCAN}\",\"optical\")"

# 2. Energy linearity and response uniformity
root -l -b -q "analysis/plot_linearity.C(\"${SCAN}\",\"optical\")"

# 3. Timing resolution vs detected light yield (all energies, one plot)
root -l -b -q "analysis/plot_timing_vs_LY.C(\"${SCAN}\")"

# 4. Per-energy plots: energy res, timing res, shower profiles
for E in "${ENERGIES[@]}"; do
    ROOTFILE="${SCAN}/optical_E${E}GeV.root"
    if [ -f "$ROOTFILE" ]; then
        root -l -b -q "analysis/plot_testbeam.C(\"${ROOTFILE}\",${E})"
    else
        echo "skip E=${E} GeV (missing $ROOTFILE)"
    fi
done

# Move this run's plots into the per-scan folder (keeps 100 vs 1000 separate).
mv -f build/plots/*.png "$DEST"/ 2>/dev/null || true

echo ""
echo "Plots saved to $DEST/"
ls -lh "$DEST"/
