#!/usr/bin/env bash
# stage_control.sh — use an EXISTING RADiCALsimSIMPLE run as this study's
# "none" (no-wrap) control, instead of burning compute re-running one.
#
#   bash stage_control.sh                                   # default: ../RADiCALsimSIMPLE/build
#   bash stage_control.sh /path/to/RADiCALsimSIMPLE/build/rootfiles
#
# WHY THIS IS VALID, NOT A SHORTCUT: RADiCALsimWrap is RADiCALsimSIMPLE plus
# one addition — the outer wrap — which is OFF BY DEFAULT (RADWRAP_SIDES and
# RADWRAP_ENDS both default to 0). A SIMPLE run therefore already IS this
# study's "none" config: same geometry, same ntuple schema, same flag defaults.
# The .root files are copied and used as-is; nothing is converted.
#
# TWO THINGS TO CHECK BEFORE TRUSTING THE COMPARISON:
#   1. SEEDS DIFFER. run_wrap_scan.sh pins /random/setSeeds so its configs share
#      common-mode shower fluctuations. RADiCALsimSIMPLE's run.mac sets no
#      seeds. So this control is a STATISTICAL baseline — compare means and
#      widths within their errors, not event by event. Fine at thousands of
#      events; it only matters if you are chasing a sub-percent effect.
#   2. PHYSICS FLAGS MUST MATCH. RADSIMPLE_LIGHT_SCALE, RADSIMPLE_BEAM_SPOT_MM,
#      RADSIMPLE_PDE etc. are the same names with the same defaults in both
#      sims. If the SIMPLE run overrode any of them, this is not
#      apples-to-apples. The run's console banner records what it used.
# Event counts do NOT need to match — different statistics just means
# different error bars. Only the ENERGIES need to overlap; energies present in
# one and not the other simply have no comparison row.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"

# --- standard logging: writes to <sim>/build/logs/<script>.log --------------
# Repo-wide convention, see simulations/README_LOGGING.md. No redirect needed.
. "$HERE/../lib/run_logging.sh"
start_logging "$HERE"

SRC="${1:-$HERE/../RADiCALsimSIMPLE/build/rootfiles}"
DST="$HERE/build/rootfiles/none"

if [ ! -d "$SRC" ]; then
    echo "ERROR: no such directory: $SRC"
    echo "Point this at a RADiCALsimSIMPLE build/rootfiles folder holding E<N>GeV.root files."
    exit 1
fi

N=$(ls "$SRC"/E*GeV.root 2>/dev/null | wc -l | tr -d ' ')
if [ "$N" -eq 0 ]; then
    echo "ERROR: no E*GeV.root files in $SRC"
    echo "Finish (or pull) a RADiCALsimSIMPLE sweep first."
    exit 1
fi

mkdir -p "$DST"
cp "$SRC"/E*GeV.root "$DST"/

{
    echo "# STAGED from an existing RADiCALsimSIMPLE run — NOT run by this study."
    echo "# This file exists so analysis/wrap_scan.C recognises the folder as a config."
    echo "# Source: $SRC"
    echo "# Staged: $(date)"
    echo "# Files:"
    ls "$DST"/E*GeV.root | sed 's|.*/|#   |'
    echo "# See stage_control.sh's header for the seed / flag caveats."
} > "$DST/sweep.mac"

echo "staged $N control file(s) -> build/rootfiles/none/"
ls "$DST"/E*GeV.root | sed 's|.*/|  |'
echo ""
echo "Next: root -l -b -q analysis/wrap_scan.C"
