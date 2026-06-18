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

#include "DetectorConstruction.hh"
#include "ActionInitialization.hh"

int main(int argc, char** argv) {
    // Bare `./radical` (no macro arg) = interactive 1 GeV optical viewer.
    // Default the beam to 1 GeV for that case; overwrite=0 means an explicit
    // `RADICAL_BEAM_ENERGY_GEV=...` still wins, and batch/scan runs (which pass
    // a macro arg) are untouched.
    if (argc < 2) setenv("RADICAL_BEAM_ENERGY_GEV", "1", 0);

    auto runManager = G4RunManagerFactory::CreateRunManager();
    runManager->SetUserInitialization(new DetectorConstruction());

    // FTFP_BERT, optionally + optical physics (Cherenkov, scintillation, WLS).
    // Optical photon tracking is OFF by default (it is ~100x slower); enable it
    // with env var  RADICAL_OPTICAL=1 ./radical ...  for the photon-based timing.
    // When off, the optical material tables / photodetectors simply sit unused.
    auto physics = new FTFP_BERT();
    physics->ReplacePhysics(new G4EmStandardPhysics_option4());
    bool useOptical = true;
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
    G4cout << "[RADiCAL] optical photons: ON (use /run/numberOfThreads before /run/initialize)" << G4endl;

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
        UI->ApplyCommand("/control/execute vis_purple.mac");
        uiExec->SessionStart();
        delete uiExec;
    }

    delete visManager;
    delete runManager;
    return 0;
}
