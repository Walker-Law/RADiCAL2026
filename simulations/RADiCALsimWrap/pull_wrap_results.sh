#!/usr/bin/env bash
# pull_wrap_results.sh — rsync the wrap-study results/ tree back from a cluster.
#
# Run from your Mac (not on the cluster):
#   bash pull_wrap_results.sh perseverence   # this study runs on perseverence
#   bash pull_wrap_results.sh curiosity      # if you ever run it there instead
#
# Syncs the WHOLE results/ directory (every config's E*GeV.root + sweep.mac +
# run.log), so analysis/wrap_scan.C works locally right after with no
# additional setup. Does NOT touch results/none — that is staged locally from
# RADiCALsimSIMPLE via stage_control.sh, not pulled from a cluster.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"

# --- standard logging: writes to <sim>/build/logs/<script>.log --------------
# Repo-wide convention, see simulations/README_LOGGING.md. No redirect needed.
. "$HERE/../lib/run_logging.sh"
start_logging "$HERE"

PORT=10022

case "${1:-perseverence}" in
  perseverence)  HOST="wlaw@172.16.17.252"
                 REMOTE_DIR="~/Research/RADiCAL2026/simulations/RADiCALsimWrap" ;;
  curiosity)     HOST="wlaw@172.16.17.188"
                 REMOTE_DIR="~/RADiCAL2026/simulations/RADiCALsimWrap" ;;
  *) echo "usage: bash pull_wrap_results.sh [perseverence|curiosity]"; exit 1 ;;
esac

echo "pulling results/ from ${1:-perseverence} -> $HERE/results/"
rsync -avz -e "ssh -p $PORT" \
  --exclude 'none/' \
  "$HOST:$REMOTE_DIR/results/" \
  "$HERE/results/"

echo "done. If you haven't yet, stage the control with:"
echo "  bash stage_control.sh /path/to/RADiCALsimSIMPLE/build"
echo "then: root -l -b -q analysis/wrap_scan.C"
