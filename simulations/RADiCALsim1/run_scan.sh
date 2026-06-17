#!/usr/bin/env bash
# Energy scan for the RADiCAL test-beam sim.
#   ./run_scan.sh [NEVT]          defaults: 1000 events, output -> build/scan/optical_scan_1000/
#   ./run_scan.sh 500             500 events/energy       -> build/scan/optical_scan_500/
#
# Optical photons are always ON (RADICAL_OPTICAL=1 hardcoded).
# All output (ROOT files + logs) goes into build/scan/optical_scan_<NEVT>/.
# ROOT files are named optical_E<N>GeV.root to match make_plots.sh expectations.
set -u
cd "$(dirname "$0")/build" || exit 1
source ../setup_env.sh >/dev/null 2>&1

ENERGIES=(5 10 20 25 50 100 120 150)
NEVT=${1:-1000}
OUTDIR="scan/optical_scan_${NEVT}"
export RADICAL_OPTICAL=1
mkdir -p "$OUTDIR"
THRESH=$(( NEVT/4 )); [ "$THRESH" -lt 5 ] && THRESH=5
PROG=$(( NEVT/5 ));  [ "$PROG"   -lt 1 ] && PROG=1

echo "Scan: NEVT=$NEVT/energy  OUTDIR=$OUTDIR  optical=ON"
printf '/run/initialize\n/run/printProgress %d\n/run/beamOn %d\n' "$PROG" "$NEVT" > /tmp/scan.mac

integral() {
  root -l -b -q "$1" -e \
    'printf("%.0f\n",((TH1D*)gDirectory->Get("ECombined"))->Integral()); gApplication->Terminate();' \
    2>/dev/null | grep -oE '^[0-9]+$' | head -1
}

for E in "${ENERGIES[@]}"; do
  OUT="$OUTDIR/optical_E${E}GeV.root"
  ok=0
  for attempt in 1 2 3; do
    ps aux | grep '/radical' | grep -v grep | awk '{print $2}' | xargs -r kill -9 2>/dev/null
    rm -f radical_output*.root
    RADICAL_BEAM_ENERGY_GEV=$E ./radical /tmp/scan.mac > "$OUTDIR/log_E${E}.log" 2>&1
    n=$(integral radical_output.root)
    if [ "${n:-0}" -gt "$THRESH" ] 2>/dev/null; then
      mv -f radical_output.root "$OUT"
      echo "[$(date '+%H:%M:%S')] ${E} GeV OK  (ECombined N=$n)  -> $OUT"
      ok=1; break
    fi
    echo "[$(date '+%H:%M:%S')] ${E} GeV merge failed (N=${n:-?}), retry $attempt"
  done
  [ "$ok" -eq 0 ] && echo "!! ${E} GeV FAILED after 3 attempts"
done
echo "SCAN COMPLETE $(date '+%H:%M:%S')"
ls -lh "$OUTDIR"/optical_E*GeV.root

# Auto-refresh resolution curves from the new scan.
echo "--- building resolution curves ---"
cd ..
root -l -b -q "analysis/scan_resolution.C(\"build/$OUTDIR\",\"optical\")"
echo "Resolution curves updated -> build/$OUTDIR/resolution_curves.root"
