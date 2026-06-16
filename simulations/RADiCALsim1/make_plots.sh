#!/bin/bash
# make_plots.sh — generate the paper-matching plot set from an optical scan.
# Run LOCALLY (the cluster has no ROOT), AFTER rsyncing the scan dir back.
#
#   ./make_plots.sh                 # 1000-event scan, 6 summary plots + transverse heatmap
#   ./make_plots.sh 100             # the 100-event scan
#   ./make_plots.sh 1000 --per-energy   # ALSO emit the per-energy plots
#
# Each scan's plots land in their own folder so 100 and 1000 stay separate:
#   build/plots/optical_scan_<N>/
#
# DEFAULT = 6 summary plots (all energies) + transverse heatmap (cf. arXiv:2401.01747):
#   energy_resolution_curve.png   sigma_E/E vs E, fit a/sqrtE (+) b   [Fig 17R / Eq 1]
#   timing_resolution_curve.png   sigma_t vs E,  fit a/sqrtE (+) b    [Fig 27R / Eq 2]
#   shower_long_overlay.png       longitudinal profiles, all E        [Fig 7]
#   energy_linearity.png          E_reco vs E_beam + residuals        [Fig 17L]
#   energy_response.png           E_reco/E_beam uniformity
#   timing_vs_LY.png              sigma_t vs light yield              [Fig 8]
#   transverse_heatmap_E<E>.png   x-y shower at early/max/tail depths [Fig 9]
#
# --per-energy ALSO emits (5/10/20/50/100/120 GeV):
#   energy_resolution_E<E>.png  timing_resolution_E<E>.png
#   shower_long_E<E>.png        shower_lat_E<E>.png
#
# Must be run from the RADiCALsim1 root: the analysis macros hardcode build/plots.

set -e
cd "$(dirname "$(realpath "$0")")"   # RADiCALsim1 root

# ── Parse args: a bare number = event count; --per-energy = extra plots ───────
N="1000"
PER_ENERGY=0
for arg in "$@"; do
    case "$arg" in
        --per-energy) PER_ENERGY=1 ;;
        *[0-9])       N="$arg" ;;
    esac
done

SCAN="build/optical_scan_${N}"
DEST="build/plots/optical_scan_${N}"
ENERGIES=(5 10 20 50 100 120)
HEATMAP_E=100   # representative energy for the transverse heatmap

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

# ── 6 summary plots (all energies) ────────────────────────────────────────────
# 1-3. sigma_E/E vs E, sigma_t vs E, longitudinal overlay
root -l -b -q "analysis/scan_resolution.C(\"${SCAN}\",\"optical\")"
# 4-5. energy linearity and response uniformity
root -l -b -q "analysis/plot_linearity.C(\"${SCAN}\",\"optical\")"
# 6. timing resolution vs detected light yield
root -l -b -q "analysis/plot_timing_vs_LY.C(\"${SCAN}\")"

# ── Transverse shower heatmap (early / shower-max / tail) ──────────────────────
HEATFILE="${SCAN}/optical_E${HEATMAP_E}GeV.root"
if [ -f "$HEATFILE" ]; then
    root -l -b -q "analysis/plot_transverse.C(\"${HEATFILE}\",${HEATMAP_E})"
else
    echo "skip transverse heatmap (missing $HEATFILE)"
fi

# ── Transverse radial profile (azimuthal average, 6 depth slices in X₀) ──────
RADFILE="${SCAN}/optical_E120GeV.root"
if [ -f "$RADFILE" ]; then
    root -l -b -q "analysis/plot_radial_profile.C(\"${RADFILE}\",120)"
else
    echo "skip radial profile (missing $RADFILE)"
fi

# ── Optional per-energy plots ─────────────────────────────────────────────────
if [ "$PER_ENERGY" -eq 1 ]; then
    for E in "${ENERGIES[@]}"; do
        ROOTFILE="${SCAN}/optical_E${E}GeV.root"
        if [ -f "$ROOTFILE" ]; then
            root -l -b -q "analysis/plot_testbeam.C(\"${ROOTFILE}\",${E})"
        else
            echo "skip E=${E} GeV (missing $ROOTFILE)"
        fi
    done
fi

# Move this run's plots into the per-scan folder (keeps 100 vs 1000 separate).
mv -f build/plots/*.png "$DEST"/ 2>/dev/null || true

echo ""
echo "Plots saved to $DEST/"
ls -lh "$DEST"/
