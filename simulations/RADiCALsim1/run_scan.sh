#!/usr/bin/env bash
# Energy scan — OPTICAL OFF, embarrassingly parallel over events.
#
# With optical photon tracking off, each event is ~0.18 s, so we split each
# energy into many single-thread processes that run simultaneously and saturate
# all cores. Per-chunk ROOT files are hadd-merged per energy at the end. No
# Geant4 MT merge bottleneck.
#
# On 512 cores: 8 energies x 64 chunks x ~16 events = 512 simultaneous processes.
#
# NOTE: DeltaT (optical-only timing) will be EMPTY here. This scan is for the
# energy resolution (sigma/E) and shower shapes. Timing/light-yield is a
# separate optical run (see CLAUDE.md).
#
# Usage:
#   bash run_scan.sh [NEVT]   (default 1000)
set -u
cd "$(dirname "$0")/build" || exit 1
source ../setup_env.sh >/dev/null 2>&1

ENERGIES=(5 10 20 25 50 100 120 150)
NEVT=${1:-1000}
OUTDIR="$(pwd)/scan/optical_scan_${NEVT}"     # name kept for analysis-macro compat
BINARY="$(pwd)/radical"
TOTAL_CORES=$(nproc 2>/dev/null || echo 512)
CHUNKS_PER_E=$(( TOTAL_CORES / ${#ENERGIES[@]} ))
[ "$CHUNKS_PER_E" -lt 1 ] && CHUNKS_PER_E=1
EVT_PER_CHUNK=$(( (NEVT + CHUNKS_PER_E - 1) / CHUNKS_PER_E ))
TS=$(date +%s)
START_T=$(date +%s)

export RADICAL_OPTICAL=0        # <-- OPTICAL PHOTONS OFF (fast energy scan)
mkdir -p "$OUTDIR"

echo "Energy scan: NEVT=$NEVT/energy   optical=OFF"
echo "  ${#ENERGIES[@]} energies x $CHUNKS_PER_E chunks x $EVT_PER_CHUNK events = $(( ${#ENERGIES[@]} * CHUNKS_PER_E )) simultaneous processes"
echo "  output -> $OUTDIR"
echo "------------------------------------------------------------"

# One chunk: run EVT_PER_CHUNK events single-threaded in /tmp (local disk), then
# move the result back to the shared filesystem as tmprun_E<E>_c<C>.root.
run_chunk() {
    local E=$1 C=$2
    local OUTF="$(pwd)/tmprun_E${E}_c${C}.root"
    local LOG="$OUTDIR/log_E${E}_c${C}.log"
    local TMPD
    TMPD=$(mktemp -d /tmp/radical_E${E}_c${C}_XXXXXX)
    local SEED1=$(( (E * 100003 + C * 17 + TS) % 900000000 + 1 ))
    local SEED2=$(( (E * 7919   + C * 31337 + TS * 3) % 900000000 + 1 ))
    printf '/random/setSeeds %d %d\n/run/numberOfThreads 1\n/run/initialize\n/run/beamOn %d\n' \
        "$SEED1" "$SEED2" "$EVT_PER_CHUNK" > "$TMPD/run.mac"
    ( cd "$TMPD" && RADICAL_BEAM_ENERGY_GEV=$E "$BINARY" run.mac ) > "$LOG" 2>&1
    [ -f "$TMPD/radical_output.root" ] && mv -f "$TMPD/radical_output.root" "$OUTF"
    rm -rf "$TMPD"
}
export -f run_chunk
export OUTDIR EVT_PER_CHUNK TS BINARY

# Launch all chunks for all (not-yet-done) energies simultaneously
pids=()
for E in "${ENERGIES[@]}"; do
    if [ -f "$OUTDIR/optical_E${E}GeV.root" ]; then
        echo "[$(date '+%H:%M:%S')] SKIP ${E} GeV (already exists)"
        continue
    fi
    rm -f tmprun_E${E}_c*.root
    for (( C=0; C<CHUNKS_PER_E; C++ )); do
        run_chunk "$E" "$C" &
        pids+=($!)
    done
done
TOTAL_CHUNKS=${#pids[@]}
echo "[$(date '+%H:%M:%S')] $TOTAL_CHUNKS chunks launched — monitoring..."

# Progress monitor: count completed chunk files on the shared fs every 10 s.
monitor() {
    while true; do
        sleep 10
        local done=0 parts=""
        for E in "${ENERGIES[@]}"; do
            [ -f "$OUTDIR/optical_E${E}GeV.root" ] && continue
            local n
            n=$(ls tmprun_E${E}_c*.root 2>/dev/null | wc -l | tr -d ' ')
            done=$(( done + n ))
            parts="$parts  ${E}:${n}/${CHUNKS_PER_E}"
        done
        local elapsed=$(( $(date +%s) - START_T ))
        local pct=0
        [ "$TOTAL_CHUNKS" -gt 0 ] && pct=$(( done * 100 / TOTAL_CHUNKS ))
        local eta="--"
        if [ "$done" -gt 0 ]; then
            local rem=$(( elapsed * (TOTAL_CHUNKS - done) / done ))
            eta=$(printf '%dm%02ds' $(( rem/60 )) $(( rem%60 )))
        fi
        printf "[%s] %3d%% (%d/%d)  ETA %s |%s\n" \
            "$(date '+%H:%M:%S')" "$pct" "$done" "$TOTAL_CHUNKS" "$eta" "$parts"
    done
}
monitor & MONITOR_PID=$!

for pid in "${pids[@]}"; do wait "$pid"; done
kill "$MONITOR_PID" 2>/dev/null

ELAPSED=$(( $(date +%s) - START_T ))
echo "------------------------------------------------------------"
echo "[$(date '+%H:%M:%S')] All chunks done in $(printf '%dm%02ds' $(( ELAPSED/60 )) $(( ELAPSED%60 )))"

# Merge per energy with hadd IF ROOT is available; otherwise leave the chunk
# files in place to be rsynced and merged on a machine that has ROOT.
if ! command -v hadd >/dev/null 2>&1; then
    echo ""
    echo "hadd/ROOT not found here — chunk files left in build/ for local merge."
    echo "On your Mac: rsync the tmprun_E*_c*.root files over, then for each E run"
    echo "  hadd -f optical_E\${E}GeV.root tmprun_E\${E}_c*.root"
    echo ""
    echo "SCAN COMPLETE (chunks only) $(date '+%H:%M:%S')  total $(printf '%dm%02ds' $(( ELAPSED/60 )) $(( ELAPSED%60 )))"
    exit 0
fi

# hadd merge per energy
any_failed=0
for E in "${ENERGIES[@]}"; do
    OUT="$OUTDIR/optical_E${E}GeV.root"
    [ -f "$OUT" ] && continue
    CHUNKS=( tmprun_E${E}_c*.root )
    if [ ! -e "${CHUNKS[0]}" ]; then
        echo "!! ${E} GeV: no chunk outputs — check logs in $OUTDIR/"
        any_failed=1; continue
    fi
    hadd -f "$OUT" "${CHUNKS[@]}" > "$OUTDIR/log_E${E}_merge.log" 2>&1
    if [ -f "$OUT" ]; then
        sz=$(stat -c%s "$OUT" 2>/dev/null || stat -f%z "$OUT" 2>/dev/null || echo 0)
        echo "[$(date '+%H:%M:%S')] ${E} GeV OK  (${#CHUNKS[@]} chunks, $(( sz/1024 )) KB) -> $OUT"
        rm -f tmprun_E${E}_c*.root
    else
        echo "!! ${E} GeV merge failed — see $OUTDIR/log_E${E}_merge.log"
        any_failed=1
    fi
done

echo ""
echo "SCAN COMPLETE $(date '+%H:%M:%S')  total $(printf '%dm%02ds' $(( ELAPSED/60 )) $(( ELAPSED%60 )))"
ls -lh "$OUTDIR"/optical_E*GeV.root 2>/dev/null || echo "(no output files found)"
[ "$any_failed" -eq 1 ] && echo "WARNING: one or more energies failed"

if command -v root >/dev/null 2>&1; then
    echo "--- building resolution curves ---"
    cd ..
    root -l -b -q "analysis/scan_resolution.C(\"build/scan/optical_scan_${NEVT}\",\"optical\")"
    echo "Resolution curves -> build/scan/optical_scan_${NEVT}/resolution_curves.root"
else
    echo "ROOT not found — run make_plots.sh locally after rsyncing the scan dir"
fi
