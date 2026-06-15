#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "G4VisManager.hh"
#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"
#include "FTFP_BERT.hh"

#include "DetectorConstruction.hh"
#include "ActionInitialization.hh"

int main(int argc, char** argv) {
    // Must be created first on macOS so Qt owns the main thread
    G4UIExecutive* uiExec = (argc == 1) ? new G4UIExecutive(argc, argv) : nullptr;

    auto runManager = G4RunManagerFactory::CreateRunManager();
    runManager->SetUserInitialization(new DetectorConstruction());
    runManager->SetUserInitialization(new FTFP_BERT());
    runManager->SetUserInitialization(new ActionInitialization());
    runManager->Initialize();

    auto visManager = new G4VisExecutive();
    visManager->Initialize();

    auto UI = G4UImanager::GetUIpointer();

    if (argc >= 2) {
        G4String macro = argv[1];
        if (macro.find("vis") != std::string::npos) {
            uiExec = new G4UIExecutive(argc, argv);
            UI->ApplyCommand("/control/execute " + macro);
            uiExec->SessionStart();
            delete uiExec;
        } else {
            UI->ApplyCommand("/control/execute " + macro);
        }
    } else {
        UI->ApplyCommand("/control/execute vis.mac");
        uiExec->SessionStart();
        delete uiExec;
    }

    delete visManager;
    delete runManager;
    return 0;
}
