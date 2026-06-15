#!/bin/bash
# job_optical_scan.sh — self-contained cluster job: pull code, build, run a scan.
# No rsync: the code arrives via `git pull`. Works as a Slurm batch script,
# an SGE/PBS job, or a plain detached bash run.
#
# Submit (pick the one your cluster has — see `command -v sbatch qsub`):
#   Slurm:    sbatch job_optical_scan.sh            # N defaults to 100
#             sbatch --export=ALL,N=1000 job_optical_scan.sh
#   SGE/PBS:  qsub  -v N=1000 job_optical_scan.sh
#   No sched: N=1000 nohup bash job_optical_scan.sh > scan_1000.out 2>&1 &
#
# ── Slurm directives (ignored by bash / qsub) ─────────────────────────────────
#SBATCH --job-name=radical_optscan
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=512        # one fat node; the scan self-parallelizes
#SBATCH --time=12:00:00
#SBATCH --output=scan_%j.out       # %j = job id
#
# ── SGE/PBS directives (ignored by bash / Slurm) ──────────────────────────────
#$ -N radical_optscan
#$ -cwd
#$ -j y
#$ -pe smp 512

set -e

# Where the repo lives ON THE CLUSTER. Override with REPO=... if different.
REPO="${REPO:-$HOME/RADiCAL2026}"
N="${N:-100}"                       # event count: 100 or 1000

echo "=== job start $(date) — N=${N} events, repo=${REPO} ==="

# 1. Get the latest code (replaces rsync). First time: git clone instead.
cd "$REPO"
git pull --ff-only

# 2. Build (incremental; re-runs cmake in case sources/CMakeLists changed)
cd "$REPO/simulations/RADiCALsim1/build"
cmake .. >/dev/null
make -j"$(nproc)"

# 3. Run the scan for the requested event count
bash "../run_optical_scan_${N}.sh"

echo "=== job done $(date) — results in $(pwd)/optical_scan_${N}/ ==="
