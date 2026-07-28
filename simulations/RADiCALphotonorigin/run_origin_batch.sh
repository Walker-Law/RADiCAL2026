#!/usr/bin/env bash
# Cluster batch for the photon-origin study: scan the beam impact RANDOM-UNIFORM
# over the tile face, score how the detected light splits among the 4 corner
# SiPMs vs beam (x,y), merge, and plot the position->SiPM maps.
#
#   bash run_origin_batch.sh [TOTAL_EVENTS]          (default 4096)
#   RADICAL_BEAM_ENERGY_GEV=20 bash run_origin_batch.sh 20000
#
# One single-thread process per core, each in its own /tmp dir, hadd-merged.
# Leaves RADICAL_BEAM_QUADRANT UNSET so every event samples a fresh random impact.
# Needs ROOT for the merge/plot (any conda env with hadd/root is auto-detected).
set -u

# --- standard logging: writes to <sim>/build/logs/<script>.log --------------
# Repo-wide convention, see simulations/README_LOGGING.md. No redirect needed:
#   nohup bash THIS_SCRIPT.sh &     ->   tail -f build/logs/<script>.log
. "$(cd "$(dirname "$0")" && pwd)/../lib/run_logging.sh"
start_logging "$(cd "$(dirname "$0")" && pwd)"
cd "$(dirname "$0")/build" || exit 1
source ../setup_env.sh >/dev/null 2>&1

NEVT=${1:-4096}
OUTFILE=${2:-radical_output.root}        # optional: output ROOT filename
ENERGY=${RADICAL_BEAM_ENERGY_GEV:-10}    # moderate E: localized shower, clearer position map
MAXSTEP=${RADICAL_OPT_MAXSTEP:-500}      # optical step cap
NCHUNK=$(nproc 2>/dev/null || echo 32)
[ "$NCHUNK" -gt "$NEVT" ] && NCHUNK=$NEVT
PER=$(( (NEVT + NCHUNK - 1) / NCHUNK ))
BIN="$(pwd)/radical"
TS=$(date +%s)
HADD=$(command -v hadd 2>/dev/null || ls "$HOME"/miniforge3/envs/*/bin/hadd 2>/dev/null | head -1)
ROOTEXE=$(command -v root 2>/dev/null || ls "$HOME"/miniforge3/envs/*/bin/root 2>/dev/null | head -1)

echo "Photon-origin batch: $NEVT events @ ${ENERGY} GeV  ($NCHUNK chunks x $PER)  random beam, optical ON, step cap $MAXSTEP"

run_chunk() {
    local i=$1
    local out="$(pwd)/origin_chunk_${i}.root"
    local tmp; tmp=$(mktemp -d /tmp/origin_${i}_XXXXXX)
    printf '/random/setSeeds %d %d\n/run/numberOfThreads 1\n/run/initialize\n/run/beamOn %d\n' \
        $(( i * 131 + TS )) $(( i * 977 + TS * 7 )) "$PER" > "$tmp/run.mac"
    # NOTE: RADICAL_BEAM_QUADRANT intentionally not exported -> random-uniform beam.
    ( cd "$tmp" && RADICAL_OPTICAL=1 RADICAL_OPT_MAXSTEP=$MAXSTEP RADICAL_BEAM_ENERGY_GEV=$ENERGY \
        "$BIN" run.mac ) > /dev/null 2>&1
    [ -f "$tmp/radical_output.root" ] && mv -f "$tmp/radical_output.root" "$out"
    rm -rf "$tmp"
}
export -f run_chunk
export BIN PER TS ENERGY MAXSTEP

rm -f origin_chunk_*.root
pids=()
for (( i = 0; i < NCHUNK; i++ )); do run_chunk "$i" & pids+=($!); done
for p in "${pids[@]}"; do wait "$p"; done

if [ -z "$HADD" ]; then
    echo "hadd not found — chunk files left as build/origin_chunk_*.root (merge where ROOT exists)."
    exit 0
fi
echo "merging $(ls origin_chunk_*.root 2>/dev/null | wc -l | tr -d ' ') chunks..."
"$HADD" -f "$OUTFILE" origin_chunk_*.root > /dev/null 2>&1 && rm -f origin_chunk_*.root

if [ -n "$ROOTEXE" ]; then
    echo "plotting..."
    cd ..
    "$ROOTEXE" -l -b -q "analysis/plot_photon_origin.C(\"build/${OUTFILE}\", \"${ENERGY}GeV\")"
else
    echo "root not found — run analysis/plot_photon_origin.C after activating ROOT."
fi
