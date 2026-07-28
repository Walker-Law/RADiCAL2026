#!/usr/bin/env bash
# run_wrap_scan.sh — the whole wrap study in one command.
#
#   bash run_wrap_scan.sh                    # the standard run: 2500 events x 6 energies x 7 configs
#   bash run_wrap_scan.sh 500                # quick look: 500 events per energy
#   RADWRAP_CONFIGS="tyvek black" bash run_wrap_scan.sh    # only these configs
#   RADWRAP_ENERGIES="25" bash run_wrap_scan.sh            # only this energy
#
# On a cluster, always under nohup so it survives an SSH drop:
#   nohup bash run_wrap_scan.sh > scan.log 2>&1 &
#
# THE CONTROL IS NOT RUN HERE. RADiCALsimWrap with no RADWRAP_* flags builds
# geometry byte-identical to RADiCALsimSIMPLE, so a SIMPLE sweep you have
# already IS the no-wrap control. Stage it instead of re-running it:
#   bash stage_control.sh /path/to/RADiCALsimSIMPLE/build
# (Pass RADWRAP_CONFIGS="none" here only if you specifically want a fresh
# same-random-seed control run through this binary.)
#
# Output: results/<config>/E<N>GeV.root, plus sweep.mac and run.log per config
# so every number is traceable to the macro and the banner that produced it.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="$HERE/build/radwrap"
NEV="${1:-2500}"
# Three energies by default, not SIMPLE's six. The wrap's effect on light yield
# is a roughly energy-independent RATIO, so three points show the trend at half
# the cost of six. All three exist in SIMPLE's sweep, so the staged control
# lines up. For the full grid:
#   RADWRAP_ENERGIES="5 10 25 50 100 120" bash run_wrap_scan.sh
ENERGIES="${RADWRAP_ENERGIES:-10 50 120}"
WANT="${RADWRAP_CONFIGS:-}"

# columns: name  sides  ends  reflectivity  finish  gap_mm
#
# ORDER MATTERS — most informative first, so if you have to kill the job early
# you already have the answer to the actual question.
#
# Where the reflectivity numbers come from (be honest about which are real):
#   tyvek  0.98  same value this project already uses for the inter-layer foils
#   black  0.02  absorbing bound; also the fast sanity check (runs at no-wrap speed)
#   esr    0.985 3M ESR / VM2000 specular film, manufacturer spec
#   mylar  0.90  aluminized mylar, typical vendor spec
#   delrin 0.60  ESTIMATE, NOT a measurement. Stands in for the milled Delrin
#                housing the real module already sits in (2303.05580 sec II).
#                Read it as "some diffuse reflection happens", not a quotable number.
CONFIG_TABLE="
tyvek         1 0 0.98  diffuse  0.1
black         1 0 0.02  diffuse  0.1
esr           1 0 0.985 specular 0.1
tyvek_ends    1 1 0.98  diffuse  0.1
tyvek_contact 1 0 0.98  diffuse  0.0
delrin        1 0 0.60  diffuse  0.1
mylar         1 0 0.90  specular 0.1
none          0 0 0.98  diffuse  0.1
"

# Should this config run? Default: everything except the control.
want_this() {
    if [ -n "$WANT" ]; then
        case " $WANT " in (*" $1 "*) return 0 ;; (*) return 1 ;; esac
    fi
    [ "$1" = "none" ] && return 1
    return 0
}

[ -x "$BIN" ] || { echo "ERROR: no binary at $BIN"; echo "Build it first — see README step 1."; exit 1; }

NCFG=0
while read -r name sides ends refl finish gap; do
    [ -n "${name:-}" ] || continue
    want_this "$name" && NCFG=$((NCFG + 1))
done <<EOF
$CONFIG_TABLE
EOF
[ "$NCFG" -gt 0 ] || { echo "ERROR: no configs selected (RADWRAP_CONFIGS='$WANT')"; exit 1; }

NE=$(echo $ENERGIES | wc -w | tr -d ' ')
ESUM=$(echo $ENERGIES | tr ' ' '\n' | awk '{s+=$1} END {print s}')

# ETA from a MEASURED calibration (2026-07-28, 1 core):
#   no wrap : 149 s for 12 events @ 10 GeV  ->  ~1.24 core-s per event per GeV
#   wrapped : 323 s for 12 events @ 10 GeV  ->  ~2.2x the no-wrap cost
# Runtime scales roughly linearly with beam energy and event count. Sanity
# check on real cluster hardware: this constant predicts 1.0 h for SIMPLE's
# 5000-event x 6-energy sweep on curiosity's 512 cores, which is what it
# actually took — so it transfers, it is not just a laptop number.
# 2.0 is used below as the average wrap factor (black/none run at ~1.0x).
CORES="${RADSIMPLE_THREADS:-$( (nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1) )}"
ETA=$(awk -v n="$NEV" -v es="$ESUM" -v c="$NCFG" -v k="$CORES" \
      'BEGIN {printf "%.1f", 1.24*n*es*2.0*c/k/3600}')

echo "=================================================================="
echo " wrap scan"
echo "   configs   : $NCFG  (order: most informative first)"
echo "   energies  : $ENERGIES GeV"
echo "   events    : $NEV per energy per config"
echo "   total     : $((NCFG * NE * NEV)) events"
echo "   cores     : $CORES"
echo "   rough ETA : ~${ETA} h  (optimistic; watch the per-config times below)"
echo "=================================================================="
if [ -z "$WANT" ]; then
    echo "The no-wrap control is NOT run here (it would be redundant)."
    echo "Stage it from an existing SIMPLE run when you analyze:"
    echo "   bash stage_control.sh /path/to/RADiCALsimSIMPLE/build"
    echo ""
fi

while read -r name sides ends refl finish gap; do
    [ -n "${name:-}" ] || continue
    want_this "$name" || continue

    d="$HERE/results/$name"
    mkdir -p "$d"

    # The macro is GENERATED here, next to its own output — never copied by
    # CMake. That removes RADiCALsimSIMPLE's most-repeated failure mode, where
    # editing a .mac and running only `make` silently executes a stale copy.
    {
        echo "# generated by run_wrap_scan.sh for config '$name'"
        echo "# $(date)"
        echo "/run/initialize"
        # Same seeds for every config, so shower fluctuations are common mode
        # and a config-to-config difference is the wrap, not the beam.
        echo "/random/setSeeds 20260728 1"
        echo "/run/printProgress $((NEV / 4 + 1))"
        for E in $ENERGIES; do
            echo ""
            echo "/analysis/setFileName E${E}GeV"
            echo "/gun/energy $E GeV"
            echo "/run/beamOn $NEV"
        done
    } > "$d/sweep.mac"

    echo "--- $name  (sides=$sides ends=$ends R=$refl $finish gap=${gap}mm) ---"
    start=$(date +%s)
    (
        cd "$d"
        RADWRAP_SIDES="$sides" RADWRAP_ENDS="$ends" \
        RADWRAP_REFLECTIVITY="$refl" RADWRAP_FINISH="$finish" \
        RADWRAP_GAP_MM="$gap" \
        "$BIN" sweep.mac > run.log 2>&1
    ) || { echo "    FAILED — see results/$name/run.log"; continue; }
    echo "    done in $(( $(date +%s) - start ))s  -> results/$name/"
    grep -h "outer wrap" "$d/run.log" | head -1 | sed 's/^/    /' || true
    echo ""
done <<EOF
$CONFIG_TABLE
EOF

echo "=================================================================="
echo " all done. Next:"
echo "   bash stage_control.sh /path/to/RADiCALsimSIMPLE/build   # if not done"
echo "   root -l -b -q analysis/wrap_scan.C"
echo "=================================================================="
