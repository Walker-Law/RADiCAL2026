#!/usr/bin/env bash
# Energy scan for the RADiCAL test-beam sim.
#   ./run_scan.sh [NEVT]    defaults: 1000 events -> build/scan/optical_scan_1000/
#   ./run_scan.sh 500       500 events/energy     -> build/scan/optical_scan_500/
#
# Energies run sequentially. Each job uses all available cores via Geant4 MT.
set -u
cd "$(dirname "$0")/build" || exit 1
source ../setup_env.sh >/dev/null 2>&1

ENERGIES=(5 10 20 25 50 100 120 150)
NEVT=${1:-1000}
OUTDIR="scan/optical_scan_${NEVT}"
export RADICAL_OPTICAL=1
mkdir -p "$OUTDIR"
PROG=5

NCORES=$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

echo "Scan: NEVT=$NEVT/energy  OUTDIR=$OUTDIR  optical=ON  sequential  threads=$NCORES"
printf '/run/numberOfThreads %d\n/run/initialize\n/run/printProgress %d\n/run/beamOn %d\n' \
    "$NCORES" "$PROG" "$NEVT" > /tmp/scan.mac

any_failed=0
for E in "${ENERGIES[@]}"; do
    OUT="$OUTDIR/optical_E${E}GeV.root"
    LOG="$OUTDIR/log_E${E}.log"
    TMPD="tmprun_E${E}"

    if [ -f "$OUT" ]; then
        echo "[$(date '+%H:%M:%S')] SKIP ${E} GeV (already exists)"
        continue
    fi

    mkdir -p "$TMPD"
    ok=0
    for attempt in 1 2 3; do
        rm -f "$TMPD"/radical_output*.root
        ( cd "$TMPD" && RADICAL_BEAM_ENERGY_GEV=$E ../radical /tmp/scan.mac ) > "$LOG" 2>&1
        sz=$(stat -c%s "$TMPD/radical_output.root" 2>/dev/null \
          || stat -f%z "$TMPD/radical_output.root" 2>/dev/null || echo 0)
        if [ "${sz:-0}" -gt 5000000 ]; then
            mv -f "$TMPD/radical_output.root" "$OUT"
            echo "[$(date '+%H:%M:%S')] ${E} GeV OK  ($(( sz/1024/1024 )) MB)  -> $OUT"
            ok=1; break
        fi
        echo "[$(date '+%H:%M:%S')] ${E} GeV attempt $attempt failed (size=${sz:-0} bytes)"
    done
    rm -rf "$TMPD"
    if [ "$ok" -eq 0 ]; then
        echo "!! ${E} GeV FAILED after 3 attempts"
        any_failed=1
    fi
done

echo ""
echo "SCAN COMPLETE $(date '+%H:%M:%S')"
ls -lh "$OUTDIR"/optical_E*GeV.root 2>/dev/null || echo "(no output files found)"
[ "$any_failed" -eq 1 ] && echo "WARNING: one or more energies failed — check logs in $OUTDIR/"

if command -v root >/dev/null 2>&1; then
    echo "--- building resolution curves ---"
    cd ..
    root -l -b -q "analysis/scan_resolution.C(\"build/$OUTDIR\",\"optical\")"
    echo "Resolution curves updated -> build/$OUTDIR/resolution_curves.root"
else
    echo "ROOT not found — run make_plots.sh locally after rsyncing build/$OUTDIR/"
fi
