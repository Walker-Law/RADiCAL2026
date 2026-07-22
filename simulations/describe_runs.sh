#!/usr/bin/env bash
# describe_runs.sh — write a RUN_INFO.txt into every scan/run directory that
# contains Geant4 chunk logs, explaining what those logs are.
#
# WHY ONE FILE PER RUN DIRECTORY (not per .log):
#   A scan directory holds hundreds of per-chunk logs (log_E<E>_c<N>.log). Every
#   chunk in a directory runs the SAME binary with the SAME configuration —
#   they differ only by beam energy, chunk index and RNG seed (the scan is
#   embarrassingly parallel over events; see run_scan.sh). So one description
#   per directory fully explains every log inside it; a per-log file would be
#   hundreds of identical copies. Logs live under build/ and are gitignored,
#   so per-log files would not be version-controlled anyway.
#
# Facts are EXTRACTED from the logs themselves (config banner, energies, chunk
# counts, event totals, timestamps) rather than asserted, so this stays true if
# runs are re-done. The "PURPOSE" line is curated per known run tag; unknown
# tags are explicitly marked as unrecorded rather than guessed.
#
#   bash simulations/describe_runs.sh          # from the repo root
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

purpose_for() {   # $1 = run directory path
  case "$1" in
    *optical_scan_1000_paperJ)   echo "PAPER-GRID timing run WITH electronics: paper energies (25-150 GeV)
             + DRS4 timebase + per-channel amplifier jitter (ELEC_JITTER_PS=50)
             + beam-core COG cut (2 mm). Produced sigma_t = 247.9 ps/sqrt(E) (+)
             18.7 ps vs paper 256 (+) 17.5. NOTE: the scale ladder later showed
             this is ~90% photostatistics at 100x-thinned light, so the apparent
             agreement is NOT a clean reproduction (see CLAUDE.md)." ;;
    *optical_scan_1000_paper)    echo "First run on the PAPER energy grid (25/50/75/100/125/150 GeV,
             replacing the old 5-120 grid) and first use of the paper-matched
             4-capillary-mean estimator H1[32]/H1[34]. No amplifier jitter yet." ;;
    *optical_scan_1000_paperLY)  echo "FAILED light-yield calibration attempt (kept as a negative result).
             Cut all light ~36x chasing '25 npe/MeV'; timing blew up 15x to
             3830 ps/sqrt(E) (+) 223 because the estimator went photon-starved.
             Confirms the standing rule: do NOT chase the paper with global
             light-yield knobs. Its one useful output was the Fig-17-like energy
             SHAPE (12.61% (+) 88.2%/sqrt(E))." ;;
    *optical_scan_1000_lad*)     echo "PHOTOSTATISTICS SCALE LADDER point. Both yield knobs scaled
             coherently by f (LYSO_SCINT_SCALE=1e-2*f, SCINT_YIELD=1.0*f) so the
             light COMPOSITION is fixed and only total photon count varies.
             Used to fit a^2(f) = A^2/f + B^2, separating photostatistics from
             the light-independent floor. Result (with paperJ as f=1):
             A = 233.6, B = 93.7 ps/sqrt(E)." ;;
    *optical_scan_1000_fig17)    echo "Energy-resolution-shape config (low light) aimed at the paper's
             Fig 17 stochastic term. Timing from this run is NOT meaningful
             (deliberately photon-starved); quote timing from paperJ instead." ;;
    *yield_sweep_2000*)          echo "LYSO-yield sweep — LARGELY INVALIDATED. Points above 3e-3 hit the
             then-default 4M/event RADICAL_MAX_OPT_PHOTONS cap, so stacking
             silently killed photons and detected N_pe saturated. Only the
             3e-3 point is trustworthy. Source of the 'photon budget' guardrail
             and the RADICAL_QUARTZ_CHER_KEEP knob. See CLAUDE.md postmortem." ;;
    *optical_scan_20000_lysowls) echo "Large (20k evt/E) validation run with the LYSO->DSB1 WLS chain
             active. Long-running reference dataset for the timing curves." ;;
    *archive/*preLYSOoptical)    echo "ARCHIVED: predates LYSO being made optically active, so it has NO
             LYSO->DSB1 wavelength-shifting chain (fiber self-scint + Cherenkov
             only). Kept for history; not comparable to current runs." ;;
    *archive/*preBudgetFix)      echo "ARCHIVED: taken before the photon-budget fix, so it may be affected
             by silent MAX_OPT_PHOTONS truncation. Not trustworthy for
             light-yield-sensitive conclusions." ;;
    *optical_scan_2000_microfj)  echo "Run adopting the real SiPM: onsemi MicroFJ-30035-TSV, 5676
             microcells (RADICAL_SIPM_NPIX=5676), replacing an earlier guessed
             pixel count. Pixel saturation is first-order at high E." ;;
    *optical_scan_2000_hiyield)  echo "High-light-yield variant, used to probe how sigma_t moves with
             increased scintillation yield." ;;
    *optical_scan_2000_lysowls5x) echo "LYSO->WLS chain at 5x the nominal yield scale." ;;
    *Fig8/build/scan/optical_scan_*_ly*) echo "Fig 8 light-yield sweep point (arXiv:2401.01747 Fig 8:
             single-SiPM timing vs detected light yield at 50 GeV). One point per
             RADICAL_LYSO_SCINT_SCALE value. IMPORTANT: 5000-event dirs predate
             the single-capillary geometry fix and used 4 POOLED corner
             capillaries -> bimodal, light-yield-independent sigma_t. Only the
             1000-event set taken after the single centred-capillary rebuild is
             physically meaningful. See RADiCALsimFig8/CLAUDE.md." ;;
    *HoleScan*hole_scan_1000)    echo "Tile-hole-diameter sweep: light output at the capillary ends vs
             the common hole diameter D (1.2-2.0 mm, 9 points, 50 GeV), with all
             five capillaries scaled to FILL their hole. Analysis:
             analysis/plot_holescan.C." ;;
    *LuAG*datacomp)              echo "Short comparison run against the real DRS4 test-beam waveforms
             (5% CFD convention)." ;;
    *LuAG*shower_profiles)       echo "Longitudinal/lateral shower-profile run." ;;
    *) echo "Purpose not recorded in project notes — treat with caution and
             confirm the configuration from the banner below before using." ;;
  esac
}

n=0
while read -r d; do
  [ -d "$d" ] || continue
  sample=$(ls "$d"/log_*_c0.log 2>/dev/null | head -1)
  [ -n "$sample" ] || sample=$(ls "$d"/log_*.log 2>/dev/null | head -1)

  sim=$(echo "$d" | sed -E 's|.*/(RADiCALsim[A-Za-z0-9]*)/.*|\1|')
  nlogs=$(ls "$d"/*.log 2>/dev/null | wc -l | tr -d ' ')
  nroot=$(ls "$d"/*.root 2>/dev/null | wc -l | tr -d ' ')
  energies=$(ls "$d"/log_E*_c*.log 2>/dev/null | sed -E 's/.*log_E([0-9.]+)_c.*/\1/' | sort -n -u | tr '\n' ' ')
  [ -n "$energies" ] || energies=$(ls "$d"/log_D*_c*.log 2>/dev/null | sed -E 's/.*log_D([0-9]+)_c.*/D=\1/' | sort -u | tr '\n' ' ')
  when=$(stat -f '%Sm' -t '%Y-%m-%d %H:%M' "$sample" 2>/dev/null)

  {
    echo "================================================================"
    echo " RUN_INFO — $(basename "$d")"
    echo " sim: ${sim}     generated: $(date '+%Y-%m-%d %H:%M') by describe_runs.sh"
    echo "================================================================"
    echo
    echo "WHAT THE .log FILES IN THIS DIRECTORY ARE"
    echo "  log_E<E>_c<N>.log  : stdout of ONE Geant4 chunk — a single-threaded"
    echo "                       process running a slice of the events for beam"
    echo "                       energy <E> GeV, chunk index <N>. The scan is"
    echo "                       embarrassingly parallel: run_scan.sh splits each"
    echo "                       energy into many chunks (count weighted ~E, so"
    echo "                       high energies get more), runs them concurrently"
    echo "                       in /tmp, then hadd-merges the per-chunk ROOT"
    echo "                       files into optical_E<E>GeV.root."
    echo "  log_E<E>_merge.log : output of the hadd merge for that energy."
    echo "  ALL chunks in this directory share ONE binary and ONE configuration;"
    echo "  they differ only by beam energy, chunk index and RNG seed. That is"
    echo "  why this single file describes all ${nlogs} logs here."
    echo
    echo "CONTENTS OF THIS DIRECTORY"
    echo "  .log files : ${nlogs}"
    echo "  .root files: ${nroot}   (merged per-energy outputs + any curve files)"
    echo "  energies   : ${energies:-n/a}"
    echo "  run date   : ${when:-unknown}"
    echo
    echo "PURPOSE / WHY THIS RUN WAS MADE"
    echo "  $(purpose_for "$d")"
    echo
    echo "CONFIGURATION (extracted from this run's own logs, not asserted)"
    if [ -n "$sample" ]; then
      grep -E "^\[RADiCAL\]" "$sample" 2>/dev/null | sed 's/^/  /' | sort -u | head -12
      echo
      echo "  physics list / threads (from log):"
      grep -m1 -iE "FTFP|physics list" "$sample" 2>/dev/null | sed 's/^/    /'
      grep -m1 -iE "beamOn|Number of events" "$sample" 2>/dev/null | sed 's/^/    /'
    else
      echo "  (no representative chunk log found)"
    fi
    echo
    echo "GEOMETRY"
    echo "  Defined in ${sim}/src/DetectorConstruction.cc as of the run date"
    echo "  above. The geometry is NOT stored per-run, so to know exactly what"
    echo "  was simulated, check that file at the matching commit:"
    echo "      git log --before='${when:-2026-07-22}' -1 -- ${sim}/src/DetectorConstruction.cc"
    echo "  Milestones that changed geometry (see ${sim}/CLAUDE.md):"
    echo "    2026-07-09  capillary confirmed SOLID quartz (an air-bore 'hollow'"
    echo "                model was tried and reverted — it broke TIR transport)."
    echo "    2026-07-21  RADiCALsimFig8 only: reduced to ONE centred T-type"
    echo "                capillary + one downstream SiPM (paper's Fig 8 setup)."
    echo "    2026-07-22  RADiCALsimDSB: center EJ309 capillary REMOVED (paper:"
    echo "                the 5th hole was unused) and corner capillaries"
    echo "                extended to the paper's full 183 mm, PDs moved outside"
    echo "                the housing. Runs BEFORE this date have the center"
    echo "                capillary and 124.88 mm capillaries."
    echo
    echo "HOW TO REPRODUCE"
    echo "  Driver: ${sim}/run_scan.sh (or run_fig8_sweep.sh / run_hole_scan.sh /"
    echo "  run_scale_ladder.sh for the sweep-style runs). Set the RADICAL_* env"
    echo "  vars shown in the CONFIGURATION block above, then:"
    echo "      bash run_scan.sh <events-per-energy> 1     # 1 = optical ON"
    echo "  Analysis: analysis/scan_resolution.C (timing/energy curves),"
    echo "  plot_fig8.C, plot_holescan.C, plot_scale_ladder.C as appropriate."
    echo
    echo "CAVEAT"
    echo "  These logs are under build/ and are gitignored — they are local"
    echo "  artifacts, not version-controlled history."
  } > "$d/RUN_INFO.txt"
  n=$((n+1))
done < <(find . -name 'log_*.log' -not -path './.git/*' | xargs -n1 dirname | sort -u)

echo "wrote $n RUN_INFO.txt files"
