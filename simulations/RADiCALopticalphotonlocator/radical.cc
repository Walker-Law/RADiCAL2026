#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "G4VisManager.hh"
#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"
#include "FTFP_BERT.hh"
#include "G4EmStandardPhysics_option4.hh"
#include "G4OpticalPhysics.hh"
#include "G4OpticalParameters.hh"
#include <cstdlib>
#include <string>

#include "DetectorConstruction.hh"
#include "ActionInitialization.hh"

int main(int argc, char** argv) {
    // Bare `./radical` (no macro arg) = interactive photon-origin viewer: a 5 GeV
    // event with optical photons coloured by which corner capillary they were born
    // in (vis_corners.mac). The step cap (2000) bounds per-photon work for a quick,
    // readable image; raise it for fuller propagation. overwrite=0 means explicit
    // env values still win.
    if (argc < 2) {
        setenv("RADICAL_BEAM_ENERGY_GEV", "5",    0);
        setenv("RADICAL_OPT_MAXSTEP",     "2000", 0);
    }

    auto runManager = G4RunManagerFactory::CreateRunManager();
    runManager->SetUserInitialization(new DetectorConstruction());

    // FTFP_BERT + optical physics (Cherenkov, scintillation, WLS).
    // Optical photon tracking is ON by default (the interactive viewer shows the
    // purple photons). Turn it OFF with  RADICAL_OPTICAL=0 ./radical ...  for the
    // fast energy scan — energy/shower observables are identical and optical
    // tracking is ~190x slower. When off, the optical tables / photodetectors sit
    // unused and DeltaT (optical-only timing) is empty.
    auto physics = new FTFP_BERT();
    physics->ReplacePhysics(new G4EmStandardPhysics_option4());
    bool useOptical = true;
    if (const char* o = std::getenv("RADICAL_OPTICAL")) useOptical = (std::string(o) != "0");
    if (useOptical) {
        physics->RegisterPhysics(new G4OpticalPhysics());
    }
    runManager->SetUserInitialization(physics);

    if (useOptical) {
        auto* op = G4OpticalParameters::Instance();
        op->SetCerenkovMaxPhotonsPerStep(300);
        op->SetCerenkovMaxBetaChange(10.0);
        op->SetCerenkovTrackSecondariesFirst(true);
        op->SetScintTrackSecondariesFirst(true);
    }

    runManager->SetUserInitialization(new ActionInitialization());
    G4cout << "[RADiCAL] optical photons: " << (useOptical ? "ON" : "OFF")
           << " (set RADICAL_OPTICAL=0 to disable for the fast energy scan)" << G4endl;

    auto visManager = new G4VisExecutive();
    visManager->Initialize();
    auto UI = G4UImanager::GetUIpointer();

    if (argc >= 2) {
        G4String macro = argv[1];
        if (macro.find("vis") != G4String::npos || macro.find("gui") != G4String::npos) {
            auto uiExec = new G4UIExecutive(argc, argv);
            UI->ApplyCommand("/control/execute " + macro);
            uiExec->SessionStart();
            delete uiExec;
        } else {
            UI->ApplyCommand("/control/execute " + macro);
        }
    } else {
        auto uiExec = new G4UIExecutive(argc, argv);
        UI->ApplyCommand("/control/execute vis_corners.mac");
        uiExec->SessionStart();
        delete uiExec;
    }

    delete visManager;
    delete runManager;
    return 0;
}
