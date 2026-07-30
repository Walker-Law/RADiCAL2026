#!/usr/bin/env bash
# pull_lightscan_results.sh — rsync the light-scan ladder output back from a
# cluster (this study always runs on curiosity; see README.md).
#
#   bash pull_lightscan_results.sh              # curiosity (default)
#   bash pull_lightscan_results.sh perseverence  # if you ever run it there
#
# Syncs every rung folder (f<scale>/E<N>GeV.root) into the standard local
# location build/rootfiles/, so analysis/lightscan.C works right after with
# no extra setup.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"

# --- standard logging: writes to <sim>/build/logs/<script>.log --------------
. "$HERE/../lib/run_logging.sh"
start_logging "$HERE"

PORT=10022

case "${1:-curiosity}" in
  curiosity)     HOST="wlaw@172.16.17.188"
                 REMOTE_DIR="~/RADiCAL2026/simulations/RADiCALsimLightScan" ;;
  perseverence)  HOST="wlaw@172.16.17.252"
                 REMOTE_DIR="~/Research/RADiCAL2026/simulations/RADiCALsimLightScan" ;;
  *) echo "usage: bash pull_lightscan_results.sh [curiosity|perseverence]"; exit 1 ;;
esac

DEST="$HERE/build/rootfiles"
mkdir -p "$DEST"

echo "pulling from ${1:-curiosity}:build/rootfiles/ -> build/rootfiles/"
rsync -avz -e "ssh -p $PORT" \
  --exclude 'plots/' \
  "$HOST:$REMOTE_DIR/build/rootfiles/" "$DEST/"

echo ""
echo "pulled rungs:"
find "$DEST" -mindepth 1 -maxdepth 1 -type d | sed 's|.*/rootfiles/|  |' | sort
echo ""
echo "next: root -l -b -q analysis/lightscan.C"
