#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "G4VisManager.hh"
#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"
#include "FTFP_BERT.hh"
#include "G4OpticalPhysics.hh"
#include "G4OpticalParameters.hh"

#include "DetectorConstruction.hh"
#include "ActionInitialization.hh"

int main(int argc, char** argv) {
    auto runManager = G4RunManagerFactory::CreateRunManager();
    runManager->SetUserInitialization(new DetectorConstruction());

    // FTFP_BERT + optical physics (Cherenkov, scintillation, boundary, WLS).
    // Optical photons are produced/tracked only in materials carrying a
    // G4MaterialPropertiesTable (here: fused quartz + LuAG:Ce only — see
    // DetectorConstruction), which keeps the photon count tractable.
    auto physics = new FTFP_BERT();
    physics->RegisterPhysics(new G4OpticalPhysics());
    runManager->SetUserInitialization(physics);

    auto* op = G4OpticalParameters::Instance();
    op->SetCerenkovMaxPhotonsPerStep(50);
    op->SetCerenkovMaxBetaChange(10.0);
    op->SetCerenkovTrackSecondariesFirst(true);
    op->SetScintTrackSecondariesFirst(true);

    runManager->SetUserInitialization(new ActionInitialization());
    runManager->Initialize();

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
