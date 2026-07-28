#!/usr/bin/env bash
# run_logging.sh — ONE place every simulation writes its live progress log.
#
# THE RULE (repo-wide, see simulations/README_LOGGING.md):
#   every run script's live output goes to  <sim>/build/logs/<script-name>.log
# so you never have to hunt for it. To watch any run, anywhere in this repo:
#
#     tail -f <sim>/build/logs/*.log
#
# Usage — source this near the top of a run script, after HERE is known:
#
#     HERE="$(cd "$(dirname "$0")" && pwd)"
#     . "$HERE/../lib/run_logging.sh"
#     start_logging "$HERE"
#
# After start_logging, everything the script prints (and everything its child
# processes print) goes to BOTH the terminal and the log file. You do NOT need
# to add "> something.log" yourself — under nohup the tee keeps working, so
#     nohup bash run_whatever.sh &
# is enough, and the log is already in the standard place.
#
# The previous run's log is kept as <name>.log.prev so launching a new run does
# not destroy the evidence from the one before it.

start_logging() {
    local sim_root="${1:?start_logging needs the sim directory}"
    local name="${2:-$(basename "${0%.sh}")}"
    local dir="$sim_root/build/logs"

    # build/ may not exist yet if someone runs the script before cmake.
    mkdir -p "$dir" || {
        echo "[log] WARNING: cannot create $dir — logging to terminal only" >&2
        return 0
    }

    local f="$dir/$name.log"
    [ -f "$f" ] && mv -f "$f" "$f.prev"

    # Send stdout+stderr through tee: terminal AND file. Process substitution
    # keeps this working when the script is backgrounded under nohup.
    exec > >(tee "$f") 2>&1
    # Give tee a moment to attach before anything is written, otherwise the
    # first lines can race and land only on the terminal.
    sleep 0.1

    echo "[log] writing to $f"
    [ -f "$f.prev" ] && echo "[log] previous run kept as $f.prev"
    return 0
}
