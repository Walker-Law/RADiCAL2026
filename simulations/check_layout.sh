#!/usr/bin/env bash
# check_layout.sh — verify the repo-wide file layout (see README_LAYOUT.md).
#
#   bash simulations/check_layout.sh
#
# Read-only: reports, never moves anything. Exit 0 = clean, 1 = drift found.
# Run it after adding a sim or changing where something writes.
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"
FAIL=0

# Sims held to the FULL convention. The DSB family is excluded on purpose —
# see README_LAYOUT.md "Deliberate exceptions" (their scan/ trees are wired
# into scan_resolution.C and the RUNS.md manifests).
FULL="RADiCALsimSIMPLE RADiCALsimWrap RADiCALsimLightScan"

say()  { printf "  %-58s %s\n" "$1" "$2"; }
bad()  { say "$1" "FAIL  $2"; FAIL=1; }
good() { say "$1" "ok"; }

echo "=== full-convention sims ==="
for s in $FULL; do
    [ -d "$s" ] || { bad "$s" "missing"; continue; }
    echo "$s"

    # 1. no loose .root anywhere under build/ except rootfiles/
    stray=$(find "$s/build" -name '*.root' -not -path "*/rootfiles/*" \
                 -not -path "*/archive*/*" -not -path "*/old_smoketests/*" 2>/dev/null | wc -l | tr -d ' ')
    [ "$stray" -eq 0 ] && good "  .root only under build/rootfiles/" \
                       || bad  "  .root only under build/rootfiles/" "$stray stray file(s)"

    # 2. macros live in macros/, not the sim root
    loose=$(ls "$s"/*.mac 2>/dev/null | wc -l | tr -d ' ')
    [ "$loose" -eq 0 ] && good "  *.mac in macros/ (none loose at root)" \
                       || bad  "  *.mac in macros/ (none loose at root)" "$loose at sim root"

    # 3. every run script logs to build/logs/
    for f in "$s"/*.sh; do
        [ -e "$f" ] || continue
        case "$(basename "$f")" in setup_env.sh) continue ;; esac
        grep -q start_logging "$f" && good "  $(basename "$f") logs to build/logs/" \
                                   || bad  "  $(basename "$f") logs to build/logs/" "no start_logging"
    done

    # 4. plots are not nested inside rootfiles/
    [ -d "$s/build/rootfiles/plots" ] \
        && bad "  plots NOT inside rootfiles/" "build/rootfiles/plots exists" \
        || good "  plots NOT inside rootfiles/"
done

echo ""
echo "=== all sims: logs convention ==="
for f in */*.sh; do
    case "$(basename "$f")" in setup_env.sh) continue ;; esac
    grep -q start_logging "$f" || bad "$f" "no start_logging"
done
[ "$FAIL" -eq 0 ] && echo "  all run scripts log to build/logs/"

echo ""
echo "=== git hygiene: run output must not be tracked ==="
tracked=$(git -C .. ls-files 2>/dev/null \
          | grep -E '/(rootfiles|logs)/|\.root$|\.log$' | wc -l | tr -d ' ')
[ "$tracked" -eq 0 ] && say "no run output tracked in git" "ok" \
                     || bad "no run output tracked in git" "$tracked file(s) tracked"

echo ""
[ "$FAIL" -eq 0 ] && echo "LAYOUT CLEAN" || echo "LAYOUT DRIFT — see README_LAYOUT.md"
exit $FAIL
