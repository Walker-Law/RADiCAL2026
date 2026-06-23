#!/usr/bin/env bash
# One-shot, reproducible Geant4 + ROOT setup for the RADiCAL sims on a fresh
# linux-64 cluster. Installs Miniforge (if absent), then creates the 'radical'
# conda env (Geant4 11.4.2 + data + ROOT 6.40 + compiler + cmake) from
# environment.yml. Run the SAME script on every cluster -> identical toolchain.
#
#   bash install_cluster_env.sh
#   conda activate radical        # then build/run any project normally
#
# No sudo, no source builds, no manual Geant4 data download.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${MINIFORGE_PREFIX:-$HOME/miniforge3}"

if [ ! -x "$PREFIX/bin/conda" ]; then
    echo ">> Installing Miniforge to $PREFIX ..."
    url="https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-Linux-x86_64.sh"
    curl -fsSL -o /tmp/Miniforge3.sh "$url" || wget -O /tmp/Miniforge3.sh "$url"
    bash /tmp/Miniforge3.sh -b -p "$PREFIX"
fi

source "$PREFIX/etc/profile.d/conda.sh"
if conda env list | awk '{print $1}' | grep -qx radical; then
    echo ">> Updating existing 'radical' env ..."
    conda env update -n radical -f "$HERE/environment.yml"
else
    echo ">> Creating 'radical' env (Geant4 + ROOT, a few minutes) ..."
    conda env create -f "$HERE/environment.yml"
fi
"$PREFIX/bin/conda" init bash >/dev/null 2>&1 || true

echo
echo ">> Done. In a new shell (or: source $PREFIX/etc/profile.d/conda.sh):"
echo "     conda activate radical"
echo "     geant4-config --version    # sanity check"
echo "     which hadd root            # sanity check"
echo "   Then build a project:"
echo "     cd <project>/ && mkdir -p build && cd build && cmake .. && make -j\$(nproc)"
