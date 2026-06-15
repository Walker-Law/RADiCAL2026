#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "G4VisManager.hh"
#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"
#include "Shielding.hh"

#include "DetectorConstruction.hh"
#include "ActionInitialization.hh"

int main(int argc, char** argv) {
    // Determine interactive vs batch before touching anything
    bool interactive = (argc == 1) ||
                       (argc >= 2 && G4String(argv[1]).find("vis") != std::string::npos);

    // G4UIExecutive MUST be constructed before RunManager on macOS
    // so Qt owns the main thread before any worker threads are spawned
    G4UIExecutive* uiExec = interactive ? new G4UIExecutive(argc, argv) : nullptr;

    auto runManager = G4RunManagerFactory::CreateRunManager(G4RunManagerType::SerialOnly);
    runManager->SetUserInitialization(new DetectorConstruction());
    runManager->SetUserInitialization(new Shielding());  // neutron HP + (n,t) reactions
    runManager->SetUserInitialization(new ActionInitialization());
    runManager->Initialize();

    auto visManager = new G4VisExecutive();
    visManager->Initialize();

    auto UI = G4UImanager::GetUIpointer();

    if (interactive) {
        G4String macro = (argc >= 2) ? argv[1] : "vis.mac";
        UI->ApplyCommand("/control/execute " + macro);
        uiExec->SessionStart();
        delete uiExec;
    } else {
        UI->ApplyCommand("/control/execute " + G4String(argv[1]));
    }

    delete visManager;
    delete runManager;
    return 0;
}
