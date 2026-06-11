#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "G4VisManager.hh"
#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"
#include "FTFP_BERT.hh"
#include "G4OpticalPhysics.hh"
#include "G4OpticalParameters.hh"
#include <cstdlib>

#include "DetectorConstruction.hh"
#include "ActionInitialization.hh"

int main(int argc, char** argv) {
    auto runManager = G4RunManagerFactory::CreateRunManager();
    runManager->SetUserInitialization(new DetectorConstruction());

    // FTFP_BERT, optionally + optical physics (Cherenkov, scintillation, WLS).
    // Optical photon tracking is OFF by default (it is ~100x slower); enable it
    // with env var  RADICAL_OPTICAL=1 ./radical ...  for the photon-based timing.
    // When off, the optical material tables / photodetectors simply sit unused.
    auto physics = new FTFP_BERT();
    bool useOptical = true;
    if (useOptical) {
        physics->RegisterPhysics(new G4OpticalPhysics());
    }
    runManager->SetUserInitialization(physics);

    if (useOptical) {
        auto* op = G4OpticalParameters::Instance();
        op->SetCerenkovMaxPhotonsPerStep(50);
        op->SetCerenkovMaxBetaChange(10.0);
        op->SetCerenkovTrackSecondariesFirst(true);
        op->SetScintTrackSecondariesFirst(true);
    }

    runManager->SetUserInitialization(new ActionInitialization());
    runManager->Initialize();
    G4cout << "[RADiCAL] optical photons: " << (useOptical ? "ON" : "OFF (fast)") << G4endl;

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
        UI->ApplyCommand("/control/execute vis.mac");
        uiExec->SessionStart();
        delete uiExec;
    }

    delete visManager;
    delete runManager;
    return 0;
}
