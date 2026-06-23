#!/usr/bin/env bash
# Small cluster batch for the optical photon locator: fills the SiPM-vs-origin
# cross-talk matrix (H2[15]) across many cores, merges, and plots it.
#
#   bash run_sipm_batch.sh [TOTAL_EVENTS]      (default 512)
#   RADICAL_BEAM_ENERGY_GEV=5 bash run_sipm_batch.sh 1024
#
# One single-thread process per core, each in its own /tmp dir, hadd-merged.
# Needs ROOT for the merge/plot (conda env `hep` is auto-detected).
set -u
cd "$(dirname "$0")/build" || exit 1
source ../setup_env.sh >/dev/null 2>&1

NEVT=${1:-512}
ENERGY=${RADICAL_BEAM_ENERGY_GEV:-2}
NCHUNK=$(nproc 2>/dev/null || echo 32)
[ "$NCHUNK" -gt "$NEVT" ] && NCHUNK=$NEVT
PER=$(( (NEVT + NCHUNK - 1) / NCHUNK ))
BIN="$(pwd)/radical"
TS=$(date +%s)
CONDA_BIN="$HOME/miniforge3/envs/hep/bin"
HADD=$( [ -x "$CONDA_BIN/hadd" ] && echo "$CONDA_BIN/hadd" || command -v hadd 2>/dev/null )
ROOTEXE=$( [ -x "$CONDA_BIN/root" ] && echo "$CONDA_BIN/root" || command -v root 2>/dev/null )

echo "Locator batch: $NEVT events @ ${ENERGY} GeV  ($NCHUNK chunks x $PER)  optical ON, step cap 200"

run_chunk() {
    local i=$1
    local out="$(pwd)/sipm_chunk_${i}.root"
    local tmp; tmp=$(mktemp -d /tmp/sipm_${i}_XXXXXX)
    printf '/random/setSeeds %d %d\n/run/numberOfThreads 1\n/run/initialize\n/run/beamOn %d\n' \
        $(( i * 131 + TS )) $(( i * 977 + TS * 7 )) "$PER" > "$tmp/run.mac"
    ( cd "$tmp" && RADICAL_OPTICAL=1 RADICAL_OPT_MAXSTEP=200 RADICAL_BEAM_ENERGY_GEV=$ENERGY \
        "$BIN" run.mac ) > /dev/null 2>&1
    [ -f "$tmp/radical_output.root" ] && mv -f "$tmp/radical_output.root" "$out"
    rm -rf "$tmp"
}
export -f run_chunk
export BIN PER TS ENERGY

rm -f sipm_chunk_*.root
pids=()
for (( i = 0; i < NCHUNK; i++ )); do run_chunk "$i" & pids+=($!); done
for p in "${pids[@]}"; do wait "$p"; done

if [ -z "$HADD" ]; then
    echo "hadd not found — chunk files left as build/sipm_chunk_*.root (merge where ROOT exists)."
    exit 0
fi
echo "merging $(ls sipm_chunk_*.root 2>/dev/null | wc -l | tr -d ' ') chunks..."
"$HADD" -f radical_output.root sipm_chunk_*.root > /dev/null 2>&1 && rm -f sipm_chunk_*.root

if [ -n "$ROOTEXE" ]; then
    echo "plotting..."
    cd ..
    "$ROOTEXE" -l -b -q 'analysis/plot_sipm_origin.C("build/radical_output.root")'
else
    echo "root not found — run analysis/plot_sipm_origin.C after activating ROOT."
fi
