#!/usr/bin/env bash
# pull_results.sh — rsync sweep results from a cluster back to this Mac.
#
# Run from your Mac (not on the cluster):
#   bash pull_results.sh                # curiosity   -> build/        (the long run)
#   bash pull_results.sh perseverence   # perseverence -> build/short/ (the quick run)
#
# Each cluster lands in its own local folder so the two runs never overwrite
# each other. Analyze whichever you want:
#   root -l -b -q analysis/scan.C                  # build/
#   root -l -b -q 'analysis/scan.C("build/short")' # build/short/
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"

# --- standard logging: writes to <sim>/build/logs/<script>.log --------------
# Repo-wide convention, see simulations/README_LOGGING.md. No redirect needed.
. "$HERE/../lib/run_logging.sh"
start_logging "$HERE"

PORT=10022

# NOTE: the two clusters do NOT have the repo at the same path (July 2026) —
# curiosity: ~/RADiCAL2026, perseverence: ~/Research/RADiCAL2026. Do not
# collapse this back into one shared REMOTE_DIR.
case "${1:-curiosity}" in
  curiosity)     HOST="wlaw@172.16.17.188"; DEST="$HERE/build"
                 REMOTE_DIR="~/RADiCAL2026/simulations/RADiCALsimSIMPLE" ;;
  perseverence)  HOST="wlaw@172.16.17.252"; DEST="$HERE/build/short"
                 REMOTE_DIR="~/Research/RADiCAL2026/simulations/RADiCALsimSIMPLE" ;;
  *) echo "usage: bash pull_results.sh [curiosity|perseverence]"; exit 1 ;;
esac

mkdir -p "$DEST/plots"
echo "pulling from ${1:-curiosity} -> $DEST"

rsync -avz -e "ssh -p $PORT" \
  "$HOST:$REMOTE_DIR/build/"'*GeV.root' \
  "$DEST/"

# plots only exist if scan.C was run ON the cluster (perseverence has no usable
# ROOT, so normally there are none — you analyze locally instead).
rsync -avz -e "ssh -p $PORT" \
  "$HOST:$REMOTE_DIR/build/plots/" \
  "$DEST/plots/" 2>/dev/null || echo "(no build/plots/ on that cluster — analyze locally with scan.C)"

echo "done -> $DEST"
