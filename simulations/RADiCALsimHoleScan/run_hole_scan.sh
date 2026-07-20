#!/usr/bin/env bash
# HOLE-DIAMETER scan — light output at the capillary ends vs the tile hole size.
#
# Sweeps RADICAL_HOLE_DIAM_MM over 1.2 … 2.0 mm at FIXED 50 GeV. All five holes
# are set to the swept diameter D and every capillary scales to FILL its hole
# (see DetectorConstruction.cc). Optical photons ON: the observable is detected
# p.e. at the capillary ends (H1[38] Light_Corners, H1[39] Light_Center,
# H1[40] Light_Total).
#
# Each hole size is split into equal single-thread chunks that run in parallel
# (all points share one energy, so no cost-weighting needed), each in its own
# /tmp dir; per-chunk ROOT files are hadd-merged per hole size at the end.
#
#   bash run_hole_scan.sh [NEVT]
#     NEVT   events per hole size   (default 2000)
#
# Survive disconnect:  nohup bash run_hole_scan.sh 2000 > hole.log 2>&1 &
#
# Physics knobs (env, with defaults chosen for a tractable, well-lit run):
#   RADICAL_HOLES              "1.2 1.3 … 2.0"  hole diameters (mm) to sweep
#   RADICAL_BEAM_ENERGY_GEV    50               fixed beam energy
#   RADICAL_LYSO_SCINT_SCALE   1e-2             LYSO 420 nm yield scale (WLS feed)
#   RADICAL_EJ309_SCINT_SCALE  1e-2             EJ309 424 nm yield scale (center cap)
#   RADICAL_OPT_MAXSTEP        5000             optical-photon step cap
#   RADICAL_OPT_TMAX           50               optical-photon global-time cut (ns)
set -u
cd "$(dirname "$0")/build" || exit 1
source ../setup_env.sh >/dev/null 2>&1

# Hole diameters to sweep (mm). Override with RADICAL_HOLES="1.2 1.5 2.0".
if [ -n "${RADICAL_HOLES:-}" ]; then
    read -ra HOLES <<< "$RADICAL_HOLES"
else
    HOLES=(1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0)
fi
NEVT=${1:-2000}
ENERGY=${RADICAL_BEAM_ENERGY_GEV:-50}

# Physics defaults (exported to every chunk). Light output is the point of this
# study, so keep ALL light (Cherenkov + scint + WLS); yields scaled for a
# tractable photon budget. Override any of these from the environment.
export RADICAL_OPTICAL=1
export RADICAL_LYSO_SCINT_SCALE=${RADICAL_LYSO_SCINT_SCALE:-1e-2}
export RADICAL_EJ309_SCINT_SCALE=${RADICAL_EJ309_SCINT_SCALE:-1e-2}
export RADICAL_OPT_MAXSTEP=${RADICAL_OPT_MAXSTEP:-5000}
export RADICAL_OPT_TMAX=${RADICAL_OPT_TMAX:-50}

OUTDIR="$(pwd)/scan/hole_scan_${NEVT}${RADICAL_RUN_TAG:+_$RADICAL_RUN_TAG}"
BINARY="$(pwd)/radical"

# Pre-flight: refuse to launch hundreds of chunks against a missing/broken binary.
if [ ! -x "$BINARY" ]; then
    echo "FATAL: $BINARY not found or not executable. Build it first:" >&2
    echo "  cd build && source ../setup_env.sh && cmake .. && make -j\$(nproc)" >&2
    exit 1
fi
PREFLIGHT_DIR=$(mktemp -d /tmp/radical_holepf_XXXXXX)
printf '/run/numberOfThreads 1\n/run/initialize\n/run/beamOn 2\n' > "$PREFLIGHT_DIR/run.mac"
( cd "$PREFLIGHT_DIR" && RADICAL_HOLE_DIAM_MM=1.5 RADICAL_BEAM_ENERGY_GEV="$ENERGY" \
    "$BINARY" run.mac ) > "$PREFLIGHT_DIR/preflight.log" 2>&1
if [ ! -f "$PREFLIGHT_DIR/radical_output.root" ]; then
    echo "FATAL: pre-flight smoke test failed — binary ran but produced no output." >&2
    tail -20 "$PREFLIGHT_DIR/preflight.log" >&2
    exit 1
fi
echo "Pre-flight OK: $BINARY runs and produces output."
rm -rf "$PREFLIGHT_DIR"

# Cores: split evenly across hole sizes (all points cost the same at fixed E).
TOTAL_CORES=${RADICAL_MAX_CORES:-$(nproc 2>/dev/null || echo 512)}
NHOLES=${#HOLES[@]}
CHUNKS_PER=$(( TOTAL_CORES / NHOLES ))
[ "$CHUNKS_PER" -lt 4 ] && CHUNKS_PER=4
NPER=$(( (NEVT + CHUNKS_PER - 1) / CHUNKS_PER ))
TS=$(date +%s); START_T=$TS

CONDA_ROOT_BIN="$HOME/miniforge3/envs/hep/bin"
HADD=$( [ -x "$CONDA_ROOT_BIN/hadd" ] && echo "$CONDA_ROOT_BIN/hadd" || command -v hadd 2>/dev/null )
ROOTEXE=$( [ -x "$CONDA_ROOT_BIN/root" ] && echo "$CONDA_ROOT_BIN/root" || command -v root 2>/dev/null )

mkdir -p "$OUTDIR" "$(pwd)/plots"
echo "Hole-diameter scan:  NEVT=$NEVT/hole   E=${ENERGY} GeV   optical=ON"
echo "  holes (mm): ${HOLES[*]}"
echo "  LYSO scale=$RADICAL_LYSO_SCINT_SCALE  EJ309 scale=$RADICAL_EJ309_SCINT_SCALE  MAXSTEP=$RADICAL_OPT_MAXSTEP  TMAX=$RADICAL_OPT_TMAX ns"
echo "  $CHUNKS_PER chunks x $NPER events per hole  ($TOTAL_CORES cores, $NHOLES holes)"
echo "  output -> $OUTDIR"
echo "  hadd   -> ${HADD:-<none found>}"
echo "------------------------------------------------------------"

# One chunk: NPER events single-threaded in /tmp, result moved back. A "tag" made
# from the hole diameter (e.g. 1.2 -> 12) keeps chunk filenames filesystem-safe.
run_chunk() {
    local D=$1 DT=$2 C=$3 NEV=$4
    local OUTF="$(pwd)/tmphole_D${DT}_c${C}.root"
    local LOG="$OUTDIR/log_D${DT}_c${C}.log"
    local TMPD
    TMPD=$(mktemp -d /tmp/radical_D${DT}_c${C}_XXXXXX)
    local SEED1=$(( (DT * 100003 + C * 17 + TS) % 900000000 + 1 ))
    local SEED2=$(( (DT * 7919   + C * 31337 + TS * 3) % 900000000 + 1 ))
    printf '/random/setSeeds %d %d\n/run/numberOfThreads 1\n/run/initialize\n/run/printProgress 10\n/run/beamOn %d\n' \
        "$SEED1" "$SEED2" "$NEV" > "$TMPD/run.mac"
    ( cd "$TMPD" && RADICAL_HOLE_DIAM_MM=$D RADICAL_BEAM_ENERGY_GEV=$ENERGY \
        "$BINARY" run.mac ) > "$LOG" 2>&1
    [ -f "$TMPD/radical_output.root" ] && mv -f "$TMPD/radical_output.root" "$OUTF"
    rm -rf "$TMPD"
}
export -f run_chunk
export OUTDIR TS BINARY ENERGY

# Launch every chunk for every not-yet-done hole size simultaneously.
pids=(); EVTARGET=0
for D in "${HOLES[@]}"; do
    DT=$(awk "BEGIN{printf \"%.0f\", $D*10}")   # 1.2 -> 12  (filesystem tag)
    EXIST="$OUTDIR/hole_D${D}.root"
    if [ -f "$EXIST" ]; then
        sz=$(stat -c%s "$EXIST" 2>/dev/null || stat -f%z "$EXIST" 2>/dev/null || echo 0)
        if [ "$sz" -gt 10240 ]; then
            echo "[$(date '+%H:%M:%S')] SKIP D=${D} mm (valid file, $(( sz/1024 )) KB)"; continue
        fi
        echo "[$(date '+%H:%M:%S')] REDO D=${D} mm (stale file, $sz bytes)"; rm -f "$EXIST"
    fi
    rm -f "$OUTDIR"/log_D${DT}_c*.log tmphole_D${DT}_c*.root
    EVTARGET=$(( EVTARGET + CHUNKS_PER * NPER ))
    for (( C=0; C<CHUNKS_PER; C++ )); do
        run_chunk "$D" "$DT" "$C" "$NPER" &
        pids+=($!)
    done
done
TOTAL_CHUNKS=${#pids[@]}
echo "[$(date '+%H:%M:%S')] $TOTAL_CHUNKS chunks launched ($EVTARGET events total) — monitoring..."

monitor() {
    while true; do
        sleep 30
        local ev
        ev=$(grep -sc -- "--> Event" "$OUTDIR"/log_D*_c*.log 2>/dev/null \
             | awk -F: '{s+=$NF} END{print (s+0)*10}')
        local elapsed=$(( $(date +%s) - START_T )) pct=0
        [ "$EVTARGET" -gt 0 ] && pct=$(( ev * 100 / EVTARGET )); [ "$pct" -gt 100 ] && pct=100
        local eta="--"
        if [ "$ev" -gt 0 ] && [ "$ev" -lt "$EVTARGET" ]; then
            local rem=$(( elapsed * (EVTARGET - ev) / ev ))
            eta=$(printf '%dh%02dm' $(( rem/3600 )) $(( (rem%3600)/60 )))
        fi
        printf "[%s] %3d%% (~%d/%d events)  elapsed %dh%02dm  ETA %s\n" \
            "$(date '+%H:%M:%S')" "$pct" "$ev" "$EVTARGET" \
            $(( elapsed/3600 )) $(( (elapsed%3600)/60 )) "$eta"
    done
}
monitor & MONITOR_PID=$!
for pid in "${pids[@]}"; do wait "$pid"; done
kill "$MONITOR_PID" 2>/dev/null

ELAPSED=$(( $(date +%s) - START_T ))
echo "------------------------------------------------------------"
echo "[$(date '+%H:%M:%S')] All chunks done in $(printf '%dh%02dm%02ds' $(( ELAPSED/3600 )) $(( (ELAPSED%3600)/60 )) $(( ELAPSED%60 )))"

if [ -z "$HADD" ] || [ ! -x "$HADD" ]; then
    echo "hadd not found — chunk files left in build/ as tmphole_D*_c*.root."
    echo "Activate ROOT (conda activate hep) and merge manually."
    exit 0
fi

echo "[$(date '+%H:%M:%S')] merging with $HADD ..."
any_failed=0
for D in "${HOLES[@]}"; do
    DT=$(printf '%.0f' "$(echo "$D * 10" | bc -l)")
    OUT="$OUTDIR/hole_D${D}.root"
    [ -f "$OUT" ] && continue
    CHUNKS=( tmphole_D${DT}_c*.root )
    if [ ! -e "${CHUNKS[0]}" ]; then
        echo "!! D=${D} mm: no chunk outputs. First chunk log:"
        tail -15 "$OUTDIR/log_D${DT}_c0.log" 2>/dev/null | sed 's/^/     /'
        any_failed=1; continue
    fi
    "$HADD" -f "$OUT" "${CHUNKS[@]}" > "$OUTDIR/log_D${DT}_merge.log" 2>&1
    sz=$([ -f "$OUT" ] && (stat -c%s "$OUT" 2>/dev/null || stat -f%z "$OUT" 2>/dev/null) || echo 0)
    if [ "$sz" -gt 10240 ]; then
        echo "[$(date '+%H:%M:%S')] D=${D} mm OK  (${#CHUNKS[@]} chunks, $(( sz/1024 )) KB)"
        rm -f tmphole_D${DT}_c*.root
    else
        echo "!! D=${D} mm merge FAILED ($sz bytes) — chunks kept."
        tail -8 "$OUTDIR/log_D${DT}_merge.log" 2>/dev/null | sed 's/^/     /'
        rm -f "$OUT"; any_failed=1
    fi
done

echo ""
echo "HOLE SCAN COMPLETE $(date '+%H:%M:%S')  total $(printf '%dh%02dm' $(( ELAPSED/3600 )) $(( (ELAPSED%3600)/60 )))"
ls -lh "$OUTDIR"/hole_D*.root 2>/dev/null || echo "(no output files found)"
[ "$any_failed" -eq 1 ] && { echo "WARNING: some holes failed — skipping analysis."; exit 1; }

if [ -n "$ROOTEXE" ] && [ -x "$ROOTEXE" ]; then
    echo "--- building light-output curve ---"
    cd ..
    "$ROOTEXE" -l -b -q "analysis/plot_holescan.C(\"build/scan/hole_scan_${NEVT}${RADICAL_RUN_TAG:+_$RADICAL_RUN_TAG}\")"
else
    echo "root not found — run analysis/plot_holescan.C after activating ROOT (conda activate hep)"
fi
