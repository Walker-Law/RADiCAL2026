#!/usr/bin/env bash
# time_probe.sh — measure seconds/event for every config BEFORE committing to
# a full run_wrap_scan.sh sweep.
#
# WHY THIS EXISTS: a highly-reflective wrap doesn't just change the detected
# light — it traps photons in far more boundary bounces before they're
# absorbed or reach a SiPM, so optical tracking gets substantially SLOWER per
# event than the no-wrap control (confirmed empirically: a 0.98-reflectivity
# side wrap at the default 1e-2 light thinning ran roughly two orders of
# magnitude slower per event than RADiCALsimSIMPLE's baseline in a local
# smoke test). The per-photon step cap (RADSIMPLE_PHOTON_STEP_CAP, default
# 20000) bounds any ONE photon's runaway, but does nothing about the total
# NUMBER of photons now surviving many more steps before termination — so
# wall-clock time is genuinely a property of the reflectivity, not a hang.
#
# Run this FIRST, look at the printed sec/event, and pick NEV / ENERGIES for
# run_wrap_scan.sh accordingly:
#   bash time_probe.sh              # 20 events/config at 1 energy (25 GeV)
#   bash time_probe.sh 50 10        # 50 events/config at 10 GeV
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="$HERE/build/radwrap"
NEV="${1:-20}"
E="${2:-25}"
[ -x "$BIN" ] || { echo "no binary at $BIN — build first"; exit 1; }

# Same config table as run_wrap_scan.sh — keep in sync if you edit one.
CONFIGS=(
  "tyvek:1:0:0.98:diffuse:0.1"
  "tyvek_ends:1:1:0.98:diffuse:0.1"
  "esr:1:0:0.985:specular:0.1"
  "mylar:1:0:0.90:specular:0.1"
  "delrin:1:0:0.60:diffuse:0.1"
  "tyvek_contact:1:0:0.98:diffuse:0.0"
  "black:1:0:0.02:diffuse:0.1"
)

printf "%-16s %10s %12s\n" "config" "sec/event" "sec (all cfgs @ 5000ev x 6E)"
for cfg in "${CONFIGS[@]}"; do
    IFS=: read -r name sides ends refl finish gap <<< "$cfg"
    mac=$(mktemp)
    { echo "/run/initialize"; echo "/run/printProgress 1000";
      echo "/gun/energy $E GeV"; echo "/run/beamOn $NEV"; } > "$mac"
    start=$(date +%s)
    RADWRAP_SIDES="$sides" RADWRAP_ENDS="$ends" RADWRAP_REFLECTIVITY="$refl" \
    RADWRAP_FINISH="$finish" RADWRAP_GAP_MM="$gap" RADSIMPLE_THREADS=1 \
      "$BIN" "$mac" > /dev/null 2>&1
    elapsed=$(( $(date +%s) - start ))
    rm -f "$mac" radsimple_output.root
    persec=$(echo "$elapsed $NEV" | awk '{printf "%.2f", $1/$2}')
    fullsweep=$(echo "$persec" | awk '{printf "%.0f", $1*5000*6*7/3600}')  # hours, 7 wrapped configs, 1 thread
    printf "%-16s %10s %9sh (1 thread)\n" "$name" "$persec" "$fullsweep"
done
echo ""
echo "Divide the 'hours' column by however many threads/cores you actually use"
echo "on perseverence. If a config is impractically slow, drop it from"
echo "RADWRAP_CONFIGS, lower NEV, or raise RADWRAP_GAP_MM (a larger air gap"
echo "does not reduce trapping much on its own — reflectivity is what matters)."
