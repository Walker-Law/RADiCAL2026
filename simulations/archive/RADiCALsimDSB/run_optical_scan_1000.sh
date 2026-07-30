#!/bin/bash
# run_optical_scan_1000.sh — 1000-event optical scan at 6 energies, parallel.
# Run from ANYWHERE inside the repo — paths are resolved from script location.
# Builds the binary automatically if not already built.
#
# Usage:
#   bash run_optical_scan_1000.sh
#   RADICAL_OPTICAL=1 bash run_optical_scan_1000.sh   # force optical mode

set -e

# --- standard logging: writes to <sim>/build/logs/<script>.log --------------
# Repo-wide convention, see simulations/README_LOGGING.md. No redirect needed:
#   nohup bash THIS_SCRIPT.sh &     ->   tail -f build/logs/<script>.log
. "$(cd "$(dirname "$0")" && pwd)/../lib/run_logging.sh"
start_logging "$(cd "$(dirname "$0")" && pwd)"

# ── Set Geant4 data-path environment by globbing the real data directory ──────
# Do NOT trust geant4.sh — the build-tree copy has broken relative paths.
# Find the directory that actually contains the data tables (G4ENSDFSTATE*, etc.)
# and point each G4*DATA var at the correct versioned subdirectory.
set_g4_data() {
    local DATADIR=""
    for d in ~/geant4-install/share/Geant4/data \
             /home/*/geant4-install/share/Geant4/data \
             /usr/share/Geant4/data /opt/geant4*/share/Geant4*/data; do
        if [ -d "$d" ] && ls "$d"/G4ENSDFSTATE* >/dev/null 2>&1; then
            DATADIR="$d"; break
        fi
    done
    if [ -z "$DATADIR" ]; then
        DATADIR=$(dirname "$(find /home /usr /opt -maxdepth 8 -name 'ENSDFSTATE.dat' 2>/dev/null | head -1)" 2>/dev/null)
        [ -n "$DATADIR" ] && DATADIR=$(dirname "$DATADIR")
    fi
    if [ -z "$DATADIR" ] || ! ls "$DATADIR"/G4ENSDFSTATE* >/dev/null 2>&1; then
        echo "ERROR: Geant4 data tables not found. Set G4*DATA vars manually." >&2
        return 1
    fi
    pick() { ls -d "$DATADIR"/$1* 2>/dev/null | head -1; }
    export G4ENSDFSTATEDATA="$(pick G4ENSDFSTATE)"
    export G4LEDATA="$(pick G4EMLOW)"
    export G4LEVELGAMMADATA="$(pick PhotonEvaporation)"
    export G4RADIOACTIVEDATA="$(pick RadioactiveDecay)"
    export G4PARTICLEXSDATA="$(pick G4PARTICLEXS)"
    export G4PIIDATA="$(pick G4PII)"
    export G4REALSURFACEDATA="$(pick RealSurface)"
    export G4SAIDXSDATA="$(pick G4SAIDDATA)"
    export G4ABLADATA="$(pick G4ABLA)"
    export G4INCLDATA="$(pick G4INCL)"
    export G4NEUTRONHPDATA="$(pick G4NDL)"
    echo "Geant4 data dir: $DATADIR"
}

if [ -z "$G4ENSDFSTATEDATA" ] || [ ! -d "$G4ENSDFSTATEDATA" ]; then
    set_g4_data || echo "WARNING: run will likely crash without Geant4 data paths." >&2
fi

# ── Resolve paths from script location (platform-independent) ─────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
BINARY="$BUILD_DIR/radical"
MAC="$SCRIPT_DIR/opt1000.mac"
OUTDIR="$SCRIPT_DIR/optical_scan_1000"

# ── Auto-build if binary missing or source is newer ───────────────────────────
if [ ! -f "$BINARY" ]; then
    echo "Binary not found — building..."
    mkdir -p "$BUILD_DIR" && cd "$BUILD_DIR"

    # Try to find Geant4 if not already in environment
    if [ -z "$Geant4_DIR" ] && [ -z "$CMAKE_PREFIX_PATH" ]; then
        G4CONFIG=$(find /usr /opt /home /sw /cvmfs -name "Geant4Config.cmake" 2>/dev/null | head -1)
        if [ -n "$G4CONFIG" ]; then
            export CMAKE_PREFIX_PATH="$(dirname "$G4CONFIG"):$CMAKE_PREFIX_PATH"
            echo "Found Geant4 at: $(dirname "$G4CONFIG")"
        else
            echo "ERROR: Geant4 not found. Source your Geant4 setup script first." >&2
            echo "  e.g.: source /path/to/geant4-install/bin/geant4.sh" >&2
            exit 1
        fi
    fi

    cmake "$SCRIPT_DIR" -DCMAKE_BUILD_TYPE=Release
    make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
    cd "$SCRIPT_DIR"
    echo "Build complete."
fi

mkdir -p "$OUTDIR"

# ── Per-energy worker ─────────────────────────────────────────────────────────
run_one() {
    E=$1
    OUTFILE="$OUTDIR/optical_E${E}GeV.root"
    if [ -f "$OUTFILE" ]; then
        echo "SKIP E=${E} GeV (already exists)"
        return
    fi
    TMPDIR=$(mktemp -d)
    cp "$MAC" "$TMPDIR/opt1000.mac"
    cd "$TMPDIR"
    echo "START E=${E} GeV in $TMPDIR — $(date)"
    RADICAL_OPTICAL=1 RADICAL_BEAM_ENERGY_GEV=${E} "$BINARY" opt1000.mac > "$OUTFILE.log" 2>&1
    mv radical_output.root "$OUTFILE"
    cd - > /dev/null
    rm -rf "$TMPDIR"
    echo "DONE  E=${E} GeV -> $OUTFILE — $(date)"
}

export -f run_one
export OUTDIR BINARY MAC

for E in 5 10 20 50 100 120; do
    run_one $E &
done

wait
echo "All done — $(date)"
ls -lh "$OUTDIR"/
