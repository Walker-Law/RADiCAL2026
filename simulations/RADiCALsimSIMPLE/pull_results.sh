#!/usr/bin/env bash
# pull_results.sh — rsync sweep results from a cluster back to this Mac.
#
# Run from your Mac (not on the cluster):
#   bash pull_results.sh                # curiosity   -> build/        (the long run)
#   bash pull_results.sh perseverence   # cross-check -> build/archive_perseverence/
#
# Each cluster lands in its own local folder so the two runs never overwrite
# each other. Analyze whichever you want:
#   root -l -b -q analysis/scan.C                  # build/
#   root -l -b -q 'analysis/scan.C("build/archive_perseverence")'
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
  curiosity)     HOST="wlaw@172.16.17.188"; DEST="$HERE/build/rootfiles"
                 REMOTE_DIR="~/RADiCAL2026/simulations/RADiCALsimSIMPLE" ;;
  perseverence)  HOST="wlaw@172.16.17.252"; DEST="$HERE/build/archive_perseverence"
                 REMOTE_DIR="~/Research/RADiCAL2026/simulations/RADiCALsimSIMPLE" ;;
  *) echo "usage: bash pull_results.sh [curiosity|perseverence]"; exit 1 ;;
esac

mkdir -p "$DEST"

# The CLUSTER-side path depends on when the run was launched:
#   build/rootfiles/   runs started after the 2026-07-29 layout change
#   build/            runs started before it
# Try the new path first, fall back to the old — so this works whether or not
# the cluster has pulled the layout commit yet. The LOCAL destination is always
# build/rootfiles/, so pulling never reintroduces the old layout here.
PULLED=""
for SRC in "build/rootfiles" "build"; do
    echo "trying ${1:-curiosity}:$SRC/*GeV.root ..."
    if rsync -avz -e "ssh -p $PORT" \
             "$HOST:$REMOTE_DIR/$SRC/"'*GeV.root' "$DEST/" 2>/dev/null; then
        PULLED="$SRC"; break
    fi
done

if [ -z "$PULLED" ]; then
    echo "ERROR: no E*GeV.root found on ${1:-curiosity} in build/rootfiles/ or build/."
    echo "Check on the cluster with:"
    echo "  ls -lh $REMOTE_DIR/build/rootfiles/*.root $REMOTE_DIR/build/*.root 2>/dev/null"
    exit 1
fi

echo ""
echo "pulled from cluster $PULLED/ -> $DEST"
ls -lh "$DEST"/*GeV.root 2>/dev/null | awk '{print "  "$5"  "$9}'
echo ""
echo "next: root -l -b -q analysis/scan.C"
