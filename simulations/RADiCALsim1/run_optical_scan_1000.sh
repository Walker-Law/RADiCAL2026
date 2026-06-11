#!/bin/bash
# run_optical_scan_1000.sh — 1000-event optical scan at 6 energies, all in parallel.
# Run from RADiCALsim1/build/ after sourcing setup_env_cluster.sh.
#   source ../setup_env_cluster.sh
#   bash ../run_optical_scan_1000.sh

mkdir -p optical_scan_1000

for E in 5 10 20 50 100 120; do
    OUTFILE="optical_scan_1000/optical_E${E}GeV.root"
    if [ -f "$OUTFILE" ]; then
        echo "SKIP E=${E} GeV (already exists)"
        continue
    fi
    echo "START E=${E} GeV — $(date)"
    RADICAL_BEAM_ENERGY_GEV=${E} nohup ./radical ../opt1000.mac \
        > optical_scan_1000/log_E${E}GeV.txt 2>&1 &
done

echo "All jobs launched. Waiting..."
wait

for E in 5 10 20 50 100 120; do
    mv -f radical_output_E${E}GeV.root optical_scan_1000/optical_E${E}GeV.root 2>/dev/null || true
done

echo "Done — $(date)"
ls -lh optical_scan_1000/
