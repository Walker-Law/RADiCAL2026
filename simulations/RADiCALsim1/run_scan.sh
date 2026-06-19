#!/usr/bin/env bash
# Energy scan — embarrassingly parallel over events.
#
# Strategy: split each energy into CHUNKS_PER_E single-threaded processes,
# all launched simultaneously. No Geant4 MT merge bottleneck.
# At the end, hadd merges the per-chunk ROOT files per energy.
#
# On 512 cores: 8 energies x 64 chunks x ~16 events = 512 simultaneous processes.
#
# Usage:
#   bash run_scan.sh [NEVT]   (default 1000)
set -u
cd "$(dirname "$0")/build" || exit 1
source ../setup_env.sh >/dev/null 2>&1

ENERGIES=(5 10 20 25 50 100 120 150)
NEVT=${1:-1000}
OUTDIR="scan/optical_scan_${NEVT}"
TOTAL_CORES=$(nproc 2>/dev/null || echo 512)
CHUNKS_PER_E=$(( TOTAL_CORES / ${#ENERGIES[@]} ))
EVT_PER_CHUNK=$(( (NEVT + CHUNKS_PER_E - 1) / CHUNKS_PER_E ))
TS=$(date +%s)

export RADICAL_OPTICAL=1
mkdir -p "$OUTDIR"

echo "Scan: NEVT=$NEVT/energy  OUTDIR=$OUTDIR  optical=ON"
echo "      ${#ENERGIES[@]} energies x $CHUNKS_PER_E chunks x $EVT_PER_CHUNK events = $TOTAL_CORES simultaneous processes"

run_chunk() {
    local E=$1 C=$2
    local OUTF="$(pwd)/tmprun_E${E}_c${C}.root"  # final location on shared fs
    local LOG="$OUTDIR/log_E${E}_c${C}.log"
    local TMPD
    TMPD=$(mktemp -d /tmp/radical_E${E}_c${C}_XXXXXX)  # local disk — no NFS contention
    local SEED1=$(( (E * 10000 + C * 17 + TS) % 900000000 + 1 ))
    local SEED2=$(( (E * 7919  + C * 31337 + TS * 3) % 900000000 + 1 ))
    printf '/random/setSeeds %d %d\n/run/numberOfThreads 1\n/run/initialize\n/run/beamOn %d\n' \
        "$SEED1" "$SEED2" "$EVT_PER_CHUNK" > "$TMPD/run.mac"
    ( cd "$TMPD" && RADICAL_BEAM_ENERGY_GEV=$E ../radical run.mac ) > "$LOG" 2>&1
    # Copy result from local /tmp back to shared fs, then clean up
    [ -f "$TMPD/radical_output.root" ] && mv "$TMPD/radical_output.root" "$OUTF"
    rm -rf "$TMPD"
}
export -f run_chunk
export OUTDIR EVT_PER_CHUNK TS

# Launch all chunks for all energies simultaneously
pids=()
for E in "${ENERGIES[@]}"; do
    if [ -f "$OUTDIR/optical_E${E}GeV.root" ]; then
        echo "[$(date '+%H:%M:%S')] SKIP ${E} GeV (already exists)"
        continue
    fi
    for (( C=0; C<CHUNKS_PER_E; C++ )); do
        run_chunk "$E" "$C" &
        pids+=($!)
    done
done

TOTAL_CHUNKS=${#pids[@]}
START_T=$(date +%s)
echo "[$(date '+%H:%M:%S')] $TOTAL_CHUNKS chunks launched across ${#ENERGIES[@]} energies"
echo "------------------------------------------------------------"

# Background progress monitor — prints a status line every 15 seconds
monitor() {
    while true; do
        sleep 15
        local now=$(date +%s)
        local elapsed=$(( now - START_T ))
        local done=0
        local parts=""
        for E in "${ENERGIES[@]}"; do
            local e_done=0
            for (( C=0; C<CHUNKS_PER_E; C++ )); do
                [ -f "tmprun_E${E}_c${C}/radical_output.root" ] && (( e_done++ )) || true
            done
            done=$(( done + e_done ))
            parts="$parts  ${E}GeV:${e_done}/${CHUNKS_PER_E}"
        done
        local pct=$(( done * 100 / TOTAL_CHUNKS ))
        local eta="--"
        if [ "$done" -gt 0 ]; then
            local remaining=$(( elapsed * (TOTAL_CHUNKS - done) / done ))
            eta=$(printf '%dm%ds' $(( remaining/60 )) $(( remaining%60 )))
        fi
        printf "[%s] %3d%% (%d/%d chunks)  ETA: %s |%s\n" \
            "$(date '+%H:%M:%S')" "$pct" "$done" "$TOTAL_CHUNKS" "$eta" "$parts"
    done
}
monitor &
MONITOR_PID=$!

for pid in "${pids[@]}"; do
    wait "$pid"
done
kill "$MONITOR_PID" 2>/dev/null

ELAPSED=$(( $(date +%s) - START_T ))
echo "------------------------------------------------------------"
echo "[$(date '+%H:%M:%S')] All chunks complete in $(printf '%dm%ds' $(( ELAPSED/60 )) $(( ELAPSED%60 ))) — merging per energy..."

# hadd merge per energy
any_failed=0
for E in "${ENERGIES[@]}"; do
    OUT="$OUTDIR/optical_E${E}GeV.root"
    [ -f "$OUT" ] && continue

    CHUNKS=()
    for (( C=0; C<CHUNKS_PER_E; C++ )); do
        f="tmprun_E${E}_c${C}/radical_output.root"
        [ -f "$f" ] && CHUNKS+=("$f")
    done

    if [ ${#CHUNKS[@]} -eq 0 ]; then
        echo "!! ${E} GeV: no chunk outputs found — check logs in $OUTDIR/"
        any_failed=1; continue
    fi

    echo "[$(date '+%H:%M:%S')] ${E} GeV: merging ${#CHUNKS[@]} chunks..."
    hadd -f "$OUT" "${CHUNKS[@]}" > "$OUTDIR/log_E${E}_merge.log" 2>&1
    sz=$(stat -c%s "$OUT" 2>/dev/null || stat -f%z "$OUT" 2>/dev/null || echo 0)
    if [ "${sz:-0}" -gt 5000000 ]; then
        echo "[$(date '+%H:%M:%S')] ${E} GeV OK  ($(( sz/1024/1024 )) MB)  -> $OUT"
        rm -rf tmprun_E${E}_c*/
    else
        echo "!! ${E} GeV merge failed (${sz} bytes) — chunk logs in $OUTDIR/"
        any_failed=1
    fi
done

echo ""
echo "SCAN COMPLETE $(date '+%H:%M:%S')"
ls -lh "$OUTDIR"/optical_E*GeV.root 2>/dev/null || echo "(no output files found)"
[ "$any_failed" -eq 1 ] && echo "WARNING: one or more energies failed"

if command -v root >/dev/null 2>&1; then
    echo "--- building resolution curves ---"
    cd ..
    root -l -b -q "analysis/scan_resolution.C(\"build/$OUTDIR\",\"optical\")"
    echo "Resolution curves -> build/$OUTDIR/resolution_curves.root"
else
    echo "ROOT not found — run make_plots.sh locally after rsyncing build/$OUTDIR/"
fi
