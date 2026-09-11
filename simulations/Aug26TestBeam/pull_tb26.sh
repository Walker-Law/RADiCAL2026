#!/usr/bin/env bash
# pull_tb26.sh — copy the beam-test simulation output from the cluster to this Mac.
#
# Run from your Mac (not on the cluster):
#   bash pull_tb26.sh                 # curiosity -> build/rootfiles/<material>/...
#
# Copies EVERY material subfolder (luag/, dsb1/, ...) recursively — the older
# pull_results.sh only took top-level files and silently missed subfolders,
# which cost a round trip once (2026-08-04). Plots are never pulled: they are
# made here, from the data.
#
# DISK: true-light output is ~4 GB per material (see run_tb26.sh's estimate).
# Check `df -h ~` before pulling two materials onto a nearly full Mac.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/../lib/run_logging.sh"
start_logging "$HERE"

PORT=10022
HOST="wlaw@172.16.17.188"                                    # curiosity
REMOTE_DIR="~/RADiCAL2026/simulations/Aug26TestBeam"
DEST="$HERE/build/rootfiles"
mkdir -p "$DEST"

echo "pulling curiosity:$REMOTE_DIR/build/rootfiles/ -> $DEST/"
rsync -avz -e "ssh -p $PORT" \
  --exclude 'plots/' --exclude 'smoke/' \
  "$HOST:$REMOTE_DIR/build/rootfiles/" "$DEST/"

echo ""
echo "pulled:"
find "$DEST" -name 'E*GeV.root' | sed "s|$DEST/|  |" | sort
echo ""
echo "next: root -l -b -q analysis/tb26.C"
