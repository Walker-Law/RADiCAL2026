#!/bin/bash
# run_optical_scan_1000.sh — 1000-event optical scan at 6 energies, parallel.
# Each energy runs in its own tmpdir to avoid radical_output.root collisions.
# Run from RADiCALsim1/build/ after sourcing setup_env_cluster.sh.

OUTDIR="optical_scan_1000"
mkdir -p "$OUTDIR"
BINARY="$(pwd)/radical"
MAC="$(pwd)/../opt1000.mac"

run_one() {
    E=$1
    OUTFILE="$(pwd)/$OUTDIR/optical_E${E}GeV.root"
    if [ -f "$OUTFILE" ]; then
        echo "SKIP E=${E} GeV (already exists)"
        return
    fi
    TMPDIR=$(mktemp -d)
    cp "$MAC" "$TMPDIR/opt1000.mac"
    cd "$TMPDIR"
    echo "START E=${E} GeV in $TMPDIR — $(date)"
    RADICAL_BEAM_ENERGY_GEV=${E} "$BINARY" opt1000.mac > "$OUTFILE.log" 2>&1
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
