#!/usr/bin/env bash
# pull_results.sh — rsync the sweep results from curiosity back to this Mac.
#
# Run from your Mac (not on the cluster):
#   bash pull_results.sh
#
# Pulls build/E*GeV.root and build/plots/ from the cluster copy of this repo
# into the matching local paths, so `root -l -b -q analysis/scan.C` (which
# reads build/E*GeV.root) works locally right after this finishes.
set -eu
HOST="wlaw@172.16.17.188"
PORT=10022
REMOTE_DIR="~/RADiCAL2026/simulations/RADiCALsimSIMPLE"
HERE="$(cd "$(dirname "$0")" && pwd)"

rsync -avz -e "ssh -p $PORT" \
  "$HOST:$REMOTE_DIR/build/"'*GeV.root' \
  "$HERE/build/"

rsync -avz -e "ssh -p $PORT" \
  "$HOST:$REMOTE_DIR/build/plots/" \
  "$HERE/build/plots/" 2>/dev/null || echo "(no build/plots/ on the cluster yet — run analysis/scan.C there first, or skip and run it locally)"

echo "done -> $HERE/build/"
