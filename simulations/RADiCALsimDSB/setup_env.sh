#!/usr/bin/env bash
# RADiCALsim1 — Geant4 environment setup
# Source this file before running the simulation:
#   source ../setup_env.sh   (from build/) or
#   source setup_env.sh      (from RADiCALsim1/)
#
# Auto-detects the Geant4 installation via geant4-config, then falls back
# to the hardcoded local Mac path if geant4-config is not in PATH.

# Find geant4-config: PATH first, then the known conda env location — so the
# sim runs correctly even from the `base` env (previously that silently fell
# through to the Mac fallback below and died with "ENSDFSTATE.dat is not found").
G4CONFIG=$(command -v geant4-config 2>/dev/null)
if [ -z "$G4CONFIG" ] && [ -x "$HOME/miniforge3/envs/g4/bin/geant4-config" ]; then
    G4CONFIG="$HOME/miniforge3/envs/g4/bin/geant4-config"
fi

if [ -n "$G4CONFIG" ]; then
    G4INSTALL=$("$G4CONFIG" --prefix)
    # geant4-config --sh-setup exports all G4*DATA paths for this installation
    eval "$("$G4CONFIG" --sh-setup)"
    # Also source geant4.sh for library paths / LD_LIBRARY_PATH
    [ -f "$G4INSTALL/bin/geant4.sh" ] && source "$G4INSTALL/bin/geant4.sh" 2>/dev/null
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
