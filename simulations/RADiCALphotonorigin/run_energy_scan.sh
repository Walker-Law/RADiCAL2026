#!/usr/bin/env bash
# Energy scan for the photon-origin study: runs run_origin_batch.sh at each
# energy in sequence (all cores per energy), saving separate ROOT files and
# per-energy plots.  Finishes with plot_energy_summary.C across all energies.
#
#   bash run_energy_scan.sh [EVENTS_PER_ENERGY]    (default 10000)
#
# Output:
#   build/radical_output_5GeV.root  ... _120GeV.root
#   build/plots/5GeV/corner_light_maps.png  etc.
#   build/plots/energy_summary/diagonal_fraction.png  etc.
#
# Estimated runtime on 512 cores, 500-step cap, 10k events/energy: ~3-5 hours.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"

NEVT=${1:-10000}
MAXSTEP=${RADICAL_OPT_MAXSTEP:-500}
ENERGIES=(5 10 20 25 50 100 120)

ROOTEXE=$(command -v root 2>/dev/null || ls "$HOME"/miniforge3/envs/*/bin/root 2>/dev/null | head -1)

echo "============================================================"
echo " RADiCAL photon-origin energy scan"
echo " Energies: ${ENERGIES[*]} GeV"
echo " Events/energy: $NEVT   Step cap: $MAXSTEP   Cores: $(nproc 2>/dev/null || echo '?')"
echo "============================================================"

for E in "${ENERGIES[@]}"; do
    OUTFILE="radical_output_${E}GeV.root"
    echo ""
    echo ">>> Starting ${E} GeV  (output: build/${OUTFILE})  $(date '+%H:%M:%S')"
    RADICAL_BEAM_ENERGY_GEV=$E RADICAL_OPT_MAXSTEP=$MAXSTEP \
        bash run_origin_batch.sh "$NEVT" "$OUTFILE"
    echo "<<< Done ${E} GeV  $(date '+%H:%M:%S')"
done

echo ""
echo "All energies done. Running cross-energy summary..."
if [ -n "$ROOTEXE" ]; then
    cd build
    FILES=""
    for E in "${ENERGIES[@]}"; do
        F="radical_output_${E}GeV.root"
        [ -f "$F" ] && FILES="$FILES $F"
    done
    cd "$HERE"
    "$ROOTEXE" -l -b -q "analysis/plot_energy_summary.C()"
else
    echo "root not found — run analysis/plot_energy_summary.C manually."
fi
echo "Energy scan complete. $(date)"
