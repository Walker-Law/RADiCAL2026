#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "G4VisManager.hh"
#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"
#include "FTFP_BERT.hh"

#include "DetectorConstruction.hh"
#include "ActionInitialization.hh"

int main(int argc, char** argv) {
    auto runManager = G4RunManagerFactory::CreateRunManager();

    runManager->SetUserInitialization(new DetectorConstruction());
    runManager->SetUserInitialization(new FTFP_BERT());
    runManager->SetUserInitialization(new ActionInitialization());

    runManager->Initialize();

    auto visManager = new G4VisExecutive();
    visManager->Initialize();

    auto UI = G4UImanager::GetUIpointer();

    if (argc >= 2) {
        // We received a macro file argument! 
        G4String macroName = argv[1];
        
        // If the macro name contains "vis", open the GUI and run it
        if (macroName.contains("vis") || macroName.contains("gui")) {
            auto uiExec = new G4UIExecutive(argc, argv);
            UI->ApplyCommand("/control/execute " + macroName);
            uiExec->SessionStart();
            delete uiExec;
        } 
        // Otherwise, run in clean, ultra-fast terminal batch mode
        else {
            UI->ApplyCommand("/control/execute " + macroName);
        }
    } 
    else {
        // No arguments given! Open the GUI with an idle setup
        auto uiExec = new G4UIExecutive(argc, argv);
        UI->ApplyCommand("/control/execute vis.mac");
        uiExec->SessionStart();
        delete uiExec;
    }

    delete visManager;
    delete runManager;
    return 0;
}
