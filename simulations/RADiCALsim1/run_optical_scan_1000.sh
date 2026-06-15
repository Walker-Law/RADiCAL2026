#!/bin/bash
# run_optical_scan_1000.sh — 1000-event optical scan at 6 energies, parallel.
# Run from ANYWHERE inside the repo — paths are resolved from script location.
# Builds the binary automatically if not already built.
#
# Usage:
#   bash run_optical_scan_1000.sh
#   RADICAL_OPTICAL=1 bash run_optical_scan_1000.sh   # force optical mode

set -e

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
