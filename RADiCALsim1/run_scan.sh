#!/usr/bin/env bash
# Energy scan for the RADiCAL test-beam sim.
# Runs each beam energy in its own process (MT-safe via RADICAL_BEAM_ENERGY_GEV),
# writes one ROOT file per energy, and guards against the G4 MT merge bug
# (kill stale procs + rm output + validate integral + retry).
set -u
cd "$(dirname "$0")/build" || exit 1
source ../setup_env.sh >/dev/null 2>&1

ENERGIES=(5 10 20 50 100 120)
NEVT=1500
OUTDIR=scan
mkdir -p "$OUTDIR"

printf '/run/initialize\n/run/printProgress 500\n/run/beamOn %d\n' "$NEVT" > /tmp/scan.mac

integral() {  # ECombined integral from a root file
  root -l -b -q "$1" -e \
    'printf("%.0f\n",((TH1D*)gDirectory->Get("ECombined"))->Integral()); gApplication->Terminate();' \
    2>/dev/null | grep -oE '^[0-9]+$' | head -1
}

for E in "${ENERGIES[@]}"; do
  OUT="$OUTDIR/radical_E${E}GeV.root"
  ok=0
  for attempt in 1 2 3; do
    # merge-safety: clear stale processes + any existing output
    ps aux | grep '/radical' | grep -v grep | awk '{print $2}' | xargs -r kill -9 2>/dev/null
    rm -f radical_output*.root
    RADICAL_BEAM_ENERGY_GEV=$E ./radical /tmp/scan.mac > "$OUTDIR/log_E${E}.log" 2>&1
    n=$(integral radical_output.root)
    if [ "${n:-0}" -gt 1000 ] 2>/dev/null; then
      mv -f radical_output.root "$OUT"
      echo "[$(date '+%H:%M:%S')] ${E} GeV OK  (ECombined N=$n)  -> $OUT"
      ok=1; break
    fi
    echo "[$(date '+%H:%M:%S')] ${E} GeV merge failed (N=${n:-?}), retry $attempt"
  done
  [ "$ok" -eq 0 ] && echo "!! ${E} GeV FAILED after 3 attempts"
done
echo "SCAN COMPLETE $(date '+%H:%M:%S')"
ls -lh "$OUTDIR"/radical_E*GeV.root
