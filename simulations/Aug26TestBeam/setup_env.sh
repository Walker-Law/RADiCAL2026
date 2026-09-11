#!/usr/bin/env bash
# RADiCALsim1 — Geant4 environment setup
# Source this file before running the simulation:
#   source ../setup_env.sh   (from build/) or
#   source setup_env.sh      (from RADiCALsim1/)
#
# Auto-detects the Geant4 installation via geant4-config, then falls back
# to the hardcoded local Mac path if geant4-config is not in PATH.

# If the Geant4 data env is ALREADY valid (e.g. `conda activate g4` exported the
# paths), do nothing. Never overwrite a working environment — this is what broke
# runs launched from the g4 env: the script clobbered a correct G4ENSDFSTATEDATA
# with a wrong fallback path. Safe no-op when the file it needs already resolves.
if [ -n "${G4ENSDFSTATEDATA:-}" ] && [ -f "$G4ENSDFSTATEDATA/ENSDFSTATE.dat" ]; then
    echo "Geant4 data env already valid (G4ENSDFSTATEDATA=$G4ENSDFSTATEDATA) — keeping it."
    return 0 2>/dev/null || exit 0
fi

# Find geant4-config: PATH first, then the known conda env location — so the
# sim runs correctly even from the `base` env (previously that silently fell
# through to the Mac fallback below and died with "ENSDFSTATE.dat is not found").
G4CONFIG=$(command -v geant4-config 2>/dev/null)
if [ -z "$G4CONFIG" ] && [ -x "$HOME/miniforge3/envs/g4/bin/geant4-config" ]; then
    G4CONFIG="$HOME/miniforge3/envs/g4/bin/geant4-config"
fi

if [ -n "$G4CONFIG" ]; then
    G4INSTALL=$("$G4CONFIG" --prefix)
    # Export every dataset env var FIRST, from the AUTHORITATIVE source
    # (--datasets), and unconditionally overwrite — so any stale/wrong value
    # inherited from the environment is corrected. Doing this before anything
    # that could abort guarantees the data paths are set even when this file is
    # sourced under a caller's `set -u`.
    #   --datasets line format:  <dataset-name>  <ENV_VAR>  <path>
    eval "$("$G4CONFIG" --datasets 2>/dev/null \
            | awk '$2 ~ /^G4[A-Z]+DATA$/ && NF>=3 {print "export "$2"=\""$NF"\""}')"
    # Deliberately DO NOT source "$G4INSTALL/bin/geant4.sh": the conda-forge
    # build's copy prints "not needed with conda" and returns non-zero, which
    # under `set -u` aborted this source mid-way and left a stale (Mac-path)
    # G4ENSDFSTATEDATA in place -> "ENSDFSTATE.dat is not found". The Geant4
    # libraries resolve via the binary's rpath, so no lib setup is needed here.
    if [ -z "${G4ENSDFSTATEDATA:-}" ]; then
        echo "WARNING: G4ENSDFSTATEDATA unset after --datasets parse." >&2
        echo "         Run '$G4CONFIG --datasets' and check the column format." >&2
    fi
    # The install can be MOVED after build (2026-07-22: ~/geant4-install ->
    # ~/Research/geant4-install): --datasets paths are ABSOLUTE, baked at build
    # time, and keep pointing at the old prefix -> "ENSDFSTATE.dat is not
    # found". If the parsed path doesn't resolve, remap every G4*DATA var onto
    # THIS geant4-config's actual prefix (dataset dir names are unchanged).
    # NOTE the dataset DIRECTORY NAMES differ between installs: a source build
    # uses the upstream "G4ENSDFSTATE3.0" form, while conda-forge drops the
    # prefix ("ENSDFSTATE3.0"). So try the name as-is, then without a leading
    # "G4", then with one added, and only export a path that actually exists.
    if [ ! -f "${G4ENSDFSTATEDATA:-/nonexistent}/ENSDFSTATE.dat" ]; then
        G4DATA="$G4INSTALL/share/Geant4/data"
        for _v in $(env | sed -n 's/^\(G4[A-Z]*DATA\)=.*/\1/p'); do
            _old=$(eval echo "\$$_v")
            _b=$(basename "$_old")
            for _cand in "$_b" "${_b#G4}" "G4$_b"; do
                if [ -d "$G4DATA/$_cand" ]; then export "$_v"="$G4DATA/$_cand"; break; fi
            done
        done
        echo "Remapped stale --datasets paths onto $G4DATA"
    fi
    echo "Geant4 environment loaded via $G4CONFIG from $G4INSTALL"
else
    # Fallback: hardcoded local Mac installation
    G4INSTALL=/Users/macro-2/Research/geant4-install
    G4DATA=$G4INSTALL/share/Geant4/data

    source $G4INSTALL/bin/geant4.sh 2>/dev/null

    export G4ENSDFSTATEDATA=$G4DATA/G4ENSDFSTATE3.0
    export G4LEVELGAMMADATA=$G4DATA/PhotonEvaporation6.1.2
    export G4RADIOACTIVEDATA=$G4DATA/RadioactiveDecay6.1.2
    export G4PARTICLEXSDATA=$G4DATA/G4PARTICLEXS4.2
    export G4PIIDATA=$G4DATA/G4PII1.3
    export G4REALSURFACEDATA=$G4DATA/RealSurface2.2
    export G4SAIDXSDATA=$G4DATA/G4SAIDDATA2.0
    export G4ABLADATA=$G4DATA/G4ABLA3.3
    export G4INCLDATA=$G4DATA/G4INCL1.3
    export G4LEDATA=$G4DATA/G4EMLOW8.8
    export G4NEUTRONHPDATA=$G4DATA/G4NDL4.7.1

    echo "Geant4 environment loaded from $G4INSTALL"
fi
