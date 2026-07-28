# Where the logs are

**Every simulation in this repo writes its live progress log to the same place:**

```
<sim>/build/logs/<script-name>.log
```

So to watch any run, anywhere in this repo, it's always:

```bash
tail -f <sim>/build/logs/*.log
```

No hunting. No remembering which sim put its log where.

## You don't need to redirect anything

Run scripts log themselves. This is enough:

```bash
nohup bash run_wrap_scan.sh &
```

and the log is already at `build/logs/run_wrap_scan.log`. Adding your own
`> whatever.log` still works and does no harm, but it's redundant — and if you
do it, you'll end up with the output in *two* places, which is exactly the
problem this convention exists to solve.

When run in the foreground you still see everything on screen; the log is a
copy, not a redirect (it goes through `tee`).

## The previous run is kept

Launching a run moves the old log to `<script-name>.log.prev` instead of
deleting it, so starting a new run never destroys the evidence from the one
before it.

## Two logs for a wrapped run

Scripts that drive a Geant4 binary produce two files, both in the same folder:

| file | what's in it |
|------|--------------|
| `build/logs/<script>.log` | the *script's* progress — config banners, per-stage timing, the ETA |
| `build/logs/<name>_geant4.log` | **Geant4's own output** — this is the one with the `--> Event N starts` progress lines |

**If you're waiting to see events tick by, tail the `_geant4.log` one.** The
script log stays quiet for long stretches because the script is just waiting on
Geant4 to finish a stage.

## The one exception: per-chunk scan logs

`RADiCALsimDSB`, `RADiCALsimLuAG`, `RADiCALsimFig8` and `RADiCALsimHoleScan`
split a scan into hundreds of parallel single-thread chunks, each writing its
own `log_E<E>_c<N>.log`. Those stay **inside the scan output directory**, not in
`build/logs/` — there can be ~500 of them per run, they'd swamp the folder, and
`describe_runs.sh`, `make_run_manifests.sh` and the `RUNS.md` files all document
them where they are. The scan's *top-level* log still follows the standard rule.

## Sims driven by the binary directly

`RADiCALsimSIMPLE` is normally launched through `run_simple.sh`, which follows
the standard rule like everything else:

```bash
nohup bash run_simple.sh &            # -> build/logs/run_simple.log
nohup bash run_simple.sh run_short.mac &
```

Invoking the binary by hand (`./radsimple run.mac` from `build/`) still works
and is completely unaffected — it just doesn't log itself, so you name the
destination:

```bash
mkdir -p build/logs && nohup ./radsimple run.mac > build/logs/sweep.log 2>&1 &
```

## Deliberate exceptions

These are **intentionally not logged** — don't "fix" them:

| file | why |
|------|-----|
| `*/setup_env.sh` | **sourced, not run** (`source setup_env.sh`). `start_logging` would redirect your *interactive shell's* stdout into a file — your terminal would go silent until you closed it. |
| `describe_runs.sh`, `make_run_manifests.sh`, `install_cluster_env.sh` | Top-level utilities, not tied to any one sim, so there is no `<sim>/build/logs/` to write to. They're short and interactive — not the thing you're hunting for at 2am. |

## Implementation

One shared helper, `simulations/lib/run_logging.sh`, sourced by every run
script right after its `set -` line:

```bash
. "$(cd "$(dirname "$0")" && pwd)/../lib/run_logging.sh"
start_logging "$(cd "$(dirname "$0")" && pwd)"
```

Add those two lines to any new run script and it follows the convention
automatically. Don't copy the tee logic into individual scripts — that's how
17 slightly-different versions get created.
