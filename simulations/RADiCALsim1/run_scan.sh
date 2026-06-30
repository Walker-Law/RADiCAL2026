#!/usr/bin/env bash
# Energy/timing scan — embarrassingly parallel over events on all cores.
#
# Each energy is split into CHUNKS_PER_E single-thread processes that run
# simultaneously, each in its own /tmp dir (local disk). Per-chunk ROOT files
# are hadd-merged per energy at the end. No Geant4 MT merge bottleneck.
#
#   bash run_scan.sh [NEVT] [OPTICAL]
#     NEVT     events per energy           (default 1000)
#     OPTICAL  0 = optical OFF (fast, energy/shower only, ~0.18 s/evt)
#              1 = optical ON  (slow, photon timing/light-capture, ~30 s/evt @120)
#     (default 0)
#
# Examples:
#   bash run_scan.sh 1000           # fast energy scan
#   bash run_scan.sh 20000 1        # big optical timing run (weekend job)
#
# Survive disconnect:  nohup bash run_scan.sh 20000 1 > weekend.log 2>&1 &
set -u
cd "$(dirname "$0")/build" || exit 1
source ../setup_env.sh >/dev/null 2>&1

# Energy list: override with RADICAL_ENERGIES="5 10 20 ..." to run a subset.
if [ -n "${RADICAL_ENERGIES:-}" ]; then
    read -ra ENERGIES <<< "$RADICAL_ENERGIES"
else
    ENERGIES=(5 10 20 25 50 100 120 150)
fi
NEVT=${1:-1000}
OPTICAL=${2:-0}
TAG=$([ "$OPTICAL" = "1" ] && echo optical || echo energy)
OUTDIR="$(pwd)/scan/${TAG}_scan_${NEVT}"
BINARY="$(pwd)/radical"
TOTAL_CORES=$(nproc 2>/dev/null || echo 512)
CHUNKS_PER_E=$(( TOTAL_CORES / ${#ENERGIES[@]} ))
[ "$CHUNKS_PER_E" -lt 1 ] && CHUNKS_PER_E=1
EVT_PER_CHUNK=$(( (NEVT + CHUNKS_PER_E - 1) / CHUNKS_PER_E ))
TS=$(date +%s)
START_T=$(date +%s)

# Find ROOT's hadd/root — prefer the conda 'hep' env, fall back to PATH. We call
# these by full path so the Geant4 chunks can run in a clean env (no conda
# libstdc++ in the way) while merge/analysis still use ROOT.
CONDA_ROOT_BIN="$HOME/miniforge3/envs/hep/bin"
HADD=$( [ -x "$CONDA_ROOT_BIN/hadd" ] && echo "$CONDA_ROOT_BIN/hadd" || command -v hadd 2>/dev/null )
ROOTEXE=$( [ -x "$CONDA_ROOT_BIN/root" ] && echo "$CONDA_ROOT_BIN/root" || command -v root 2>/dev/null )

export RADICAL_OPTICAL=$OPTICAL
# Optical runs: optionally cap optical-photon steps so trapped photons (bouncing
# ~forever in the quartz/Tyvek light guide) can't stall an event. NO cap by
# default (preserves the full LuAG:Ce 60 ns decay tail -> full N_pe). Set
# RADICAL_OPT_MAXSTEP=... (and/or RADICAL_OPT_TMAX=... ns) before launching to cap.
[ "$OPTICAL" = "1" ] && [ -n "${RADICAL_OPT_MAXSTEP:-}" ] && export RADICAL_OPT_MAXSTEP
[ "$OPTICAL" = "1" ] && [ -n "${RADICAL_OPT_TMAX:-}" ] && export RADICAL_OPT_TMAX
mkdir -p "$OUTDIR"

echo "Scan: NEVT=$NEVT/energy   optical=$([ "$OPTICAL" = 1 ] && echo ON || echo OFF)   tag=$TAG"
echo "  ${#ENERGIES[@]} energies x $CHUNKS_PER_E chunks x $EVT_PER_CHUNK events = $(( ${#ENERGIES[@]} * CHUNKS_PER_E )) simultaneous processes"
echo "  output -> $OUTDIR"
[ "$OPTICAL" = "1" ] && echo "  optical photon step cap -> ${RADICAL_OPT_MAXSTEP}"
echo "  hadd   -> ${HADD:-<none found>}"
echo "------------------------------------------------------------"

# One chunk: EVT_PER_CHUNK events single-threaded in /tmp, result moved back.
run_chunk() {
    local E=$1 C=$2
    local OUTF="$(pwd)/tmprun_${TAG}_E${E}_c${C}.root"
    local LOG="$OUTDIR/log_E${E}_c${C}.log"
    local TMPD
    TMPD=$(mktemp -d /tmp/radical_E${E}_c${C}_XXXXXX)
    local SEED1=$(( (E * 100003 + C * 17 + TS) % 900000000 + 1 ))
    local SEED2=$(( (E * 7919   + C * 31337 + TS * 3) % 900000000 + 1 ))
    printf '/random/setSeeds %d %d\n/run/numberOfThreads 1\n/run/initialize\n/run/beamOn %d\n' \
        "$SEED1" "$SEED2" "$EVT_PER_CHUNK" > "$TMPD/run.mac"
    ( cd "$TMPD" && RADICAL_OPTICAL=$OPTICAL RADICAL_BEAM_ENERGY_GEV=$E "$BINARY" run.mac ) > "$LOG" 2>&1
    [ -f "$TMPD/radical_output.root" ] && mv -f "$TMPD/radical_output.root" "$OUTF"
    rm -rf "$TMPD"
}
export -f run_chunk
export OUTDIR EVT_PER_CHUNK TS BINARY TAG OPTICAL

# Launch all chunks for all (not-yet-done) energies simultaneously
pids=()
for E in "${ENERGIES[@]}"; do
    if [ -f "$OUTDIR/optical_E${E}GeV.root" ]; then
        echo "[$(date '+%H:%M:%S')] SKIP ${E} GeV (already exists)"
        continue
    fi
    rm -f tmprun_${TAG}_E${E}_c*.root
    for (( C=0; C<CHUNKS_PER_E; C++ )); do
        run_chunk "$E" "$C" &
        pids+=($!)
    done
done
TOTAL_CHUNKS=${#pids[@]}
echo "[$(date '+%H:%M:%S')] $TOTAL_CHUNKS chunks launched — monitoring..."

# Progress monitor: count completed chunk files every 30 s.
monitor() {
    while true; do
        sleep 30
        local done=0 parts=""
        for E in "${ENERGIES[@]}"; do
            [ -f "$OUTDIR/optical_E${E}GeV.root" ] && continue
            local n
            n=$(ls tmprun_${TAG}_E${E}_c*.root 2>/dev/null | wc -l | tr -d ' ')
            done=$(( done + n ))
            parts="$parts ${E}:${n}"
        done
        local elapsed=$(( $(date +%s) - START_T ))
        local pct=0
        [ "$TOTAL_CHUNKS" -gt 0 ] && pct=$(( done * 100 / TOTAL_CHUNKS ))
        local eta="--"
        if [ "$done" -gt 0 ]; then
            local rem=$(( elapsed * (TOTAL_CHUNKS - done) / done ))
            eta=$(printf '%dh%02dm' $(( rem/3600 )) $(( (rem%3600)/60 )))
        fi
        printf "[%s] %3d%% (%d/%d chunks done)  elapsed %dh%02dm  ETA %s | per-E done:%s\n" \
            "$(date '+%H:%M:%S')" "$pct" "$done" "$TOTAL_CHUNKS" \
            $(( elapsed/3600 )) $(( (elapsed%3600)/60 )) "$eta" "$parts"
    done
}
monitor & MONITOR_PID=$!

for pid in "${pids[@]}"; do wait "$pid"; done
kill "$MONITOR_PID" 2>/dev/null

ELAPSED=$(( $(date +%s) - START_T ))
echo "------------------------------------------------------------"
echo "[$(date '+%H:%M:%S')] All chunks done in $(printf '%dh%02dm%02ds' $(( ELAPSED/3600 )) $(( (ELAPSED%3600)/60 )) $(( ELAPSED%60 )))"

# Merge per energy. If no hadd anywhere, leave chunks for a local merge.
if [ -z "$HADD" ] || [ ! -x "$HADD" ]; then
    echo ""
    echo "hadd not found — chunk files left in build/ as tmprun_${TAG}_E*_c*.root."
    echo "Activate ROOT (conda activate hep) and re-run, or merge manually."
    exit 0
fi

echo "[$(date '+%H:%M:%S')] merging with $HADD ..."
any_failed=0
for E in "${ENERGIES[@]}"; do
    OUT="$OUTDIR/optical_E${E}GeV.root"           # filename kept for analysis macro
    [ -f "$OUT" ] && continue
    CHUNKS=( tmprun_${TAG}_E${E}_c*.root )
    if [ ! -e "${CHUNKS[0]}" ]; then
        echo "!! ${E} GeV: no chunk outputs — check logs in $OUTDIR/"
        any_failed=1; continue
    fi
    "$HADD" -f "$OUT" "${CHUNKS[@]}" > "$OUTDIR/log_E${E}_merge.log" 2>&1
    if [ -f "$OUT" ]; then
        sz=$(stat -c%s "$OUT" 2>/dev/null || stat -f%z "$OUT" 2>/dev/null || echo 0)
        echo "[$(date '+%H:%M:%S')] ${E} GeV OK  (${#CHUNKS[@]} chunks, $(( sz/1024 )) KB) -> $OUT"
        rm -f tmprun_${TAG}_E${E}_c*.root
    else
        echo "!! ${E} GeV merge failed — see $OUTDIR/log_E${E}_merge.log"
        any_failed=1
    fi
done

echo ""
echo "SCAN COMPLETE $(date '+%H:%M:%S')  total $(printf '%dh%02dm' $(( ELAPSED/3600 )) $(( (ELAPSED%3600)/60 )))"
ls -lh "$OUTDIR"/optical_E*GeV.root 2>/dev/null || echo "(no output files found)"
[ "$any_failed" -eq 1 ] && echo "WARNING: one or more energies failed"

if [ -n "$ROOTEXE" ] && [ -x "$ROOTEXE" ]; then
    echo "--- building resolution curves ---"
    cd ..
    "$ROOTEXE" -l -b -q "analysis/scan_resolution.C(\"build/scan/${TAG}_scan_${NEVT}\",\"optical\")"
    echo "Resolution curves -> build/scan/${TAG}_scan_${NEVT}/resolution_curves.root"
else
    echo "root not found — run make_plots.sh after activating ROOT (conda activate hep)"
fi
