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

DEST="$HERE/build/rootfiles"
mkdir -p "$DEST"

# The CLUSTER-side path depends on when the run was launched:
#   build/rootfiles/<config>/   runs started after the 2026-07-29 layout change
#   results/<config>/           runs started before it
# Try the new path first, fall back to the old one — so this works whether or
# not the cluster has pulled the layout commit yet. Local destination is always
# build/rootfiles/, i.e. the pull never reintroduces the old layout here.
#
# 'none/' is excluded: that is the control, staged locally from
# RADiCALsimSIMPLE by stage_control.sh, never pulled from a cluster.
# 'plots/' is excluded: plots are generated locally, and the clusters have no
# usable ROOT anyway.
PULLED=""
for SRC in "build/rootfiles" "results"; do
    echo "trying ${1:-perseverence}:$SRC/ ..."
    if rsync -avz -e "ssh -p $PORT" \
             --exclude 'none/' --exclude 'plots/' \
             "$HOST:$REMOTE_DIR/$SRC/" "$DEST/" 2>/dev/null; then
        PULLED="$SRC"; break
    fi
done

if [ -z "$PULLED" ]; then
    echo "ERROR: found neither build/rootfiles/ nor results/ on ${1:-perseverence}."
    echo "Has the run produced output yet?  Check on the cluster with:"
    echo "  ls -lh $REMOTE_DIR/build/rootfiles/*/ $REMOTE_DIR/results/*/ 2>/dev/null"
    exit 1
fi

echo ""
echo "pulled from cluster $PULLED/ -> build/rootfiles/"
find "$DEST" -name '*GeV.root' | sed 's|.*/rootfiles/|  |' | sort
echo ""
echo "next:"
echo "  bash stage_control.sh ../RADiCALsimSIMPLE/build/rootfiles   # if not already staged"
echo "  root -l -b -q analysis/wrap_scan.C"
