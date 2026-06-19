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
    local TMPD="tmprun_E${E}_c${C}"
    local LOG="$OUTDIR/log_E${E}_c${C}.log"
    mkdir -p "$TMPD"
    local SEED1=$(( (E * 10000 + C * 17 + TS) % 900000000 + 1 ))
    local SEED2=$(( (E * 7919  + C * 31337 + TS * 3) % 900000000 + 1 ))
    printf '/random/setSeeds %d %d\n/run/numberOfThreads 1\n/run/initialize\n/run/beamOn %d\n' \
        "$SEED1" "$SEED2" "$EVT_PER_CHUNK" > "$TMPD/run.mac"
    ( cd "$TMPD" && RADICAL_BEAM_ENERGY_GEV=$E ../radical run.mac ) > "$LOG" 2>&1
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

echo "[$(date '+%H:%M:%S')] ${#pids[@]} chunks launched — waiting..."
for pid in "${pids[@]}"; do
    wait "$pid"
done
echo "[$(date '+%H:%M:%S')] All chunks complete — merging per energy..."

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
