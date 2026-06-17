#!/usr/bin/env bash
# Energy scan for the RADiCAL test-beam sim.
#   ./run_scan.sh [NEVT]    defaults: 1000 events -> build/scan/optical_scan_1000/
#   ./run_scan.sh 500       500 events/energy     -> build/scan/optical_scan_500/
#
# All energies run IN PARALLEL, each in an isolated tmprun_E<N>/ subdirectory
# so their radical_output.root files never collide. Optical photons always ON.
set -u
cd "$(dirname "$0")/build" || exit 1
source ../setup_env.sh >/dev/null 2>&1

ENERGIES=(5 10 20 25 50 100 120 150)
NEVT=${1:-1000}
OUTDIR="scan/optical_scan_${NEVT}"
export RADICAL_OPTICAL=1
mkdir -p "$OUTDIR"
THRESH=$(( NEVT/4 )); [ "$THRESH" -lt 5 ] && THRESH=5
PROG=$(( NEVT/5 ));  [ "$PROG"   -lt 1 ] && PROG=1

THREADS_PER_JOB=4   # 8 jobs x 4 threads = 32 cores total — cluster-friendly

echo "Scan: NEVT=$NEVT/energy  OUTDIR=$OUTDIR  optical=ON  parallel (${#ENERGIES[@]} jobs x $THREADS_PER_JOB threads)"
printf '/run/initialize\n/run/numberOfThreads %d\n/run/printProgress %d\n/run/beamOn %d\n' \
    "$THREADS_PER_JOB" "$PROG" "$NEVT" > /tmp/scan.mac

# Run one energy in its own isolated subdirectory, with retry logic.
run_energy() {
    local E=$1
    local TMPD="tmprun_E${E}"
    local OUT="$OUTDIR/optical_E${E}GeV.root"
    local LOG="$OUTDIR/log_E${E}.log"

    mkdir -p "$TMPD"
    local ok=0
    for attempt in 1 2 3; do
        rm -f "$TMPD"/radical_output*.root
        ( cd "$TMPD" && RADICAL_BEAM_ENERGY_GEV=$E ../radical /tmp/scan.mac ) > "$LOG" 2>&1
        local n
        n=$(root -l -b -q "$TMPD/radical_output.root" -e \
            'printf("%.0f\n",((TH1D*)gDirectory->Get("ECombined"))->Integral()); gApplication->Terminate();' \
            2>/dev/null | grep -oE '^[0-9]+$' | head -1)
        if [ "${n:-0}" -gt "$THRESH" ] 2>/dev/null; then
            mv -f "$TMPD/radical_output.root" "$OUT"
            echo "[$(date '+%H:%M:%S')] ${E} GeV OK  (N=$n)  -> $OUT"
            ok=1; break
        fi
        echo "[$(date '+%H:%M:%S')] ${E} GeV attempt $attempt failed (N=${n:-?})"
    done
    rm -rf "$TMPD"
    [ "$ok" -eq 0 ] && echo "!! ${E} GeV FAILED after 3 attempts"
    return $(( 1 - ok ))
}
export -f run_energy
export OUTDIR THRESH

# Launch all energies in parallel
pids=()
for E in "${ENERGIES[@]}"; do
    run_energy "$E" &
    pids+=($!)
done

# Wait for all jobs and collect exit codes
any_failed=0
for pid in "${pids[@]}"; do
    wait "$pid" || any_failed=1
done

echo ""
echo "SCAN COMPLETE $(date '+%H:%M:%S')"
ls -lh "$OUTDIR"/optical_E*GeV.root 2>/dev/null || echo "(no output files found)"
[ "$any_failed" -eq 1 ] && echo "WARNING: one or more energies failed — check logs in $OUTDIR/"

# Auto-refresh resolution curves (requires ROOT — skipped if not available)
if command -v root >/dev/null 2>&1; then
    echo "--- building resolution curves ---"
    cd ..
    root -l -b -q "analysis/scan_resolution.C(\"build/$OUTDIR\",\"optical\")"
    echo "Resolution curves updated -> build/$OUTDIR/resolution_curves.root"
else
    echo "ROOT not found — run make_plots.sh locally after rsyncing build/$OUTDIR/"
fi
