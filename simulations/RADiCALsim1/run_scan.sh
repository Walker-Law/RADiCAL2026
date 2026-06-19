#!/usr/bin/env bash
# Energy scan — 8 parallel energy jobs, each using Geant4 MT.
#
# Strategy: 8 energies run simultaneously, each with THREADS Geant4 worker
# threads. Each job runs in /tmp (local disk) to avoid NFS contention, then
# copies the merged ROOT file back to the shared filesystem.
#
# On 512 cores: 8 energies x 64 threads = 512 cores.
#
# Usage:
#   bash run_scan.sh [NEVT]   (default 1000)
set -u
cd "$(dirname "$0")/build" || exit 1
source ../setup_env.sh >/dev/null 2>&1

ENERGIES=(5 10 20 25 50 100 120 150)
NEVT=${1:-1000}
OUTDIR="$(pwd)/scan/optical_scan_${NEVT}"
TOTAL_CORES=$(nproc 2>/dev/null || echo 512)
THREADS=$(( TOTAL_CORES / ${#ENERGIES[@]} ))
BINARY="$(pwd)/radical"
PROG=$(( NEVT / 10 )); [ "$PROG" -lt 1 ] && PROG=1
START_T=$(date +%s)
TS=$(date +%s)

export RADICAL_OPTICAL=1
mkdir -p "$OUTDIR"

echo "Scan: NEVT=$NEVT/energy  optical=ON"
echo "      ${#ENERGIES[@]} parallel jobs x $THREADS threads = $TOTAL_CORES cores"
echo "      output -> $OUTDIR"
echo "------------------------------------------------------------"

run_energy() {
    local E=$1
    local OUT="$OUTDIR/optical_E${E}GeV.root"
    local LOG="$OUTDIR/log_E${E}.log"
    local TMPD
    TMPD=$(mktemp -d /tmp/radical_E${E}_XXXXXX)

    if [ -f "$OUT" ]; then
        echo "[$(date '+%H:%M:%S')] SKIP ${E} GeV (already exists)"
        return 0
    fi

    local SEED1=$(( (E * 10000 + TS) % 900000000 + 1 ))
    local SEED2=$(( (E * 7919 + TS * 3) % 900000000 + 1 ))
    printf '/random/setSeeds %d %d\n/run/numberOfThreads %d\n/run/initialize\n/run/printProgress %d\n/run/beamOn %d\n' \
        "$SEED1" "$SEED2" "$THREADS" "$PROG" "$NEVT" > "$TMPD/run.mac"

    echo "[$(date '+%H:%M:%S')] START ${E} GeV  (${THREADS} threads, $NEVT events) in $TMPD"
    ( cd "$TMPD" && RADICAL_BEAM_ENERGY_GEV=$E "$BINARY" run.mac ) > "$LOG" 2>&1

    local ok=0
    local sz
    sz=$(stat -c%s "$TMPD/radical_output.root" 2>/dev/null \
      || stat -f%z "$TMPD/radical_output.root" 2>/dev/null || echo 0)
    if [ "${sz:-0}" -gt 5000000 ]; then
        mv -f "$TMPD/radical_output.root" "$OUT"
        echo "[$(date '+%H:%M:%S')] DONE  ${E} GeV  ($(( sz/1024/1024 )) MB) -> $OUT"
        ok=1
    else
        echo "!! ${E} GeV FAILED (output ${sz} bytes) — see $LOG"
    fi
    rm -rf "$TMPD"
    return $(( 1 - ok ))
}
export -f run_energy
export OUTDIR THREADS BINARY PROG NEVT TS

# Launch all energies in parallel
pids=()
for E in "${ENERGIES[@]}"; do
    run_energy "$E" &
    pids+=($!)
done

# Background progress monitor
monitor() {
    while true; do
        sleep 15
        local elapsed=$(( $(date +%s) - START_T ))
        local status=""
        for E in "${ENERGIES[@]}"; do
            if [ -f "$OUTDIR/optical_E${E}GeV.root" ]; then
                status="$status  ${E}GeV:DONE"
            else
                local last
                last=$(grep -oE 'Event [0-9]+' "$OUTDIR/log_E${E}.log" 2>/dev/null | tail -1)
                status="$status  ${E}GeV:${last:-init}"
            fi
        done
        printf "[%s] elapsed %dm%ds |%s\n" \
            "$(date '+%H:%M:%S')" $(( elapsed/60 )) $(( elapsed%60 )) "$status"
    done
}
monitor &
MONITOR_PID=$!

any_failed=0
for pid in "${pids[@]}"; do
    wait "$pid" || any_failed=1
done
kill "$MONITOR_PID" 2>/dev/null

ELAPSED=$(( $(date +%s) - START_T ))
echo "------------------------------------------------------------"
echo "SCAN COMPLETE $(date '+%H:%M:%S')  total time: $(printf '%dm%ds' $(( ELAPSED/60 )) $(( ELAPSED%60 )))"
ls -lh "$OUTDIR"/optical_E*GeV.root 2>/dev/null || echo "(no output files found)"
[ "$any_failed" -eq 1 ] && echo "WARNING: one or more energies failed"

if command -v root >/dev/null 2>&1; then
    echo "--- building resolution curves ---"
    cd ..
    root -l -b -q "analysis/scan_resolution.C(\"build/scan/optical_scan_${NEVT}\",\"optical\")"
    echo "Resolution curves -> build/scan/optical_scan_${NEVT}/resolution_curves.root"
else
    echo "ROOT not found — run make_plots.sh locally after rsyncing build/$OUTDIR/"
fi
