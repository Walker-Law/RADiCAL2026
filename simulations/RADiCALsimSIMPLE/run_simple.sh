#!/usr/bin/env bash
# run_simple.sh — launch the standard RADiCALsimSIMPLE sweep with standard logging.
#
#   bash run_simple.sh                  # run.mac (the 6-energy sweep)
#   bash run_simple.sh run_short.mac    # any other macro in this folder
#
# On a cluster, under nohup so it survives an SSH drop — no redirect needed:
#   nohup bash run_simple.sh &
#   tail -f build/logs/run_simple.log
#
# This is a THIN WRAPPER. It changes nothing about how the sim runs; it only
# puts the live Geant4 output in the repo-standard place
# (<sim>/build/logs/<script>.log — see simulations/README_LOGGING.md) so you
# never have to remember where you redirected it.
#
# Running `./radsimple run.mac` by hand from build/ still works exactly as
# before and is unaffected by this script.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"

# --- standard logging: writes to <sim>/build/logs/<script>.log --------------
. "$HERE/../lib/run_logging.sh"
start_logging "$HERE"

MACRO="${1:-run.mac}"
BIN="$HERE/build/radsimple"

[ -x "$BIN" ] || { echo "ERROR: no binary at $BIN"; echo "Build it first: see README."; exit 1; }
# The macro is read from build/, where CMake's configure_file(COPYONLY) puts it.
# REMINDER (the classic trap): editing a .mac in the source folder and running
# only `make` leaves a STALE copy in build/. Re-run `cmake ..` after macro edits.
[ -f "$HERE/build/$MACRO" ] || { echo "ERROR: no $MACRO in $HERE/build/"; \
    echo "If you just edited/added it, re-run 'cmake ..' from build/ to copy it."; exit 1; }

echo "=================================================================="
echo " RADiCALsimSIMPLE"
echo "   macro   : $MACRO"
echo "   threads : ${RADSIMPLE_THREADS:-all cores}"
echo "=================================================================="

start=$(date +%s)
cd "$HERE/build"
./radsimple "$MACRO"
echo "done in $(( $(date +%s) - start ))s"
