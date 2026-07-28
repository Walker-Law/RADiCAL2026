#!/usr/bin/env bash
# stage_control.sh — use an EXISTING RADiCALsimSIMPLE run as the "none"
# (no-wrap) control for this study, instead of burning compute re-running one.
#
# WHY THIS IS VALID, NOT AN APPROXIMATION: RADiCALsimWrap is a straight fork of
# RADiCALsimSIMPLE with one addition (the outer wrap), OFF BY DEFAULT
# (RADWRAP_SIDES / RADWRAP_ENDS both default to 0 — see DetectorConstruction.cc,
# "THE STUDY"). So a SIMPLE run with no RADWRAP_* vars set (they don't even
# exist in that binary) already IS this study's "none" config, geometry- and
# ntuple-schema-identical. There is nothing to "convert" — the .root files are
# used as-is.
#
# Usage:
#   bash stage_control.sh                                  # default: ../RADiCALsimSIMPLE/build
#   bash stage_control.sh /path/to/RADiCALsimSIMPLE/build   # e.g. build/short
#
# CAVEATS (read before trusting a point-by-point difference):
#   1. DIFFERENT RANDOM SEEDS. run_wrap_scan.sh pins /random/setSeeds so its
#      configs share common-mode shower fluctuations; RADiCALsimSIMPLE's
#      run.mac does not set seeds at all. So this control is a STATISTICAL
#      baseline (compare means/widths within their errors), not an
#      event-by-event one. With 5000+ events/energy this is normally fine —
#      it only matters if you're chasing a sub-percent effect.
#   2. FLAGS MUST MATCH. This study's physics defaults (RADSIMPLE_LIGHT_SCALE,
#      RADSIMPLE_BEAM_SPOT_MM, RADSIMPLE_PDE, ...) are the same env-var names
#      and same defaults as SIMPLE, so as long as the SIMPLE run didn't
#      override any of them, this is apples-to-apples. Check the top of
#      run.log / the run's own console banner if unsure.
#   3. EVENT COUNT / ENERGY LIST. run_wrap_scan.sh's defaults (5000 events,
#      5/10/25/50/100/120 GeV) match RADiCALsimSIMPLE's run.mac exactly so
#      this lines up point-for-point. If you ran run_wrap_scan.sh with
#      different args, only the SHARED energies will have a "none" row.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="${1:-$HERE/../RADiCALsimSIMPLE/build}"
DST="$HERE/results/none"

[ -d "$SRC" ] || { echo "no such directory: $SRC"; exit 1; }
shopt -s nullglob
FILES=("$SRC"/E*GeV.root)
[ ${#FILES[@]} -gt 0 ] || { echo "no E*GeV.root files in $SRC — pull results first"; exit 1; }

mkdir -p "$DST"
cp "${FILES[@]}" "$DST"/
cat > "$DST/sweep.mac" <<EOF
# STAGED from an existing RADiCALsimSIMPLE run — not generated/run by this
# study. Source: $SRC
# Staged:  $(date)
# Files:   ${#FILES[@]} (${FILES[@]##*/})
# See stage_control.sh's header for the same-seed / flag-matching caveats.
EOF

echo "staged ${#FILES[@]} file(s) -> $DST"
echo "  $(printf '%s ' "${FILES[@]##*/}")"
echo "run: root -l -b -q analysis/wrap_scan.C"
