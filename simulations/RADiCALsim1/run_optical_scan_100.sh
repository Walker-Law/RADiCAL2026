#!/bin/bash
# run_optical_scan_100.sh — 100-event optical scan at 6 energies, parallel.
# Each energy runs in its own tmpdir to avoid radical_output.root collisions.
# Run from RADiCALsim1/build/ after sourcing setup_env_cluster.sh.

OUTDIR="optical_scan_100"
mkdir -p "$OUTDIR"
BINARY="$(pwd)/radical"
MAC="$(pwd)/../opt100.mac"

run_one() {
    E=$1
    OUTFILE="$(pwd)/$OUTDIR/optical_E${E}GeV.root"
    if [ -f "$OUTFILE" ]; then
        echo "SKIP E=${E} GeV (already exists)"
        return
    fi
    TMPDIR=$(mktemp -d)
    cp "$MAC" "$TMPDIR/opt100.mac"
    cd "$TMPDIR"
    echo "START E=${E} GeV in $TMPDIR — $(date)"
    RADICAL_OPTICAL=1 RADICAL_BEAM_ENERGY_GEV=${E} "$BINARY" opt100.mac > "$OUTFILE.log" 2>&1
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

# Plots are generated locally (the cluster has no ROOT). After rsyncing
# optical_scan_100/ back to the local machine, run:  ./make_plots.sh 100
