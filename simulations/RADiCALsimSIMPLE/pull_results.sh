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
PORT=10022
REMOTE_DIR="~/RADiCAL2026/simulations/RADiCALsimSIMPLE"

case "${1:-curiosity}" in
  curiosity)     HOST="wlaw@172.16.17.188"; DEST="$HERE/build" ;;
  perseverence)  HOST="wlaw@172.16.17.252"; DEST="$HERE/build/short" ;;
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
