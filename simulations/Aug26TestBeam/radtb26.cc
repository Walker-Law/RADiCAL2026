// radtb26.cc — main program for the August 2026 CERN T10 test-beam simulation.
//
// A copy of RADiCALsimSIMPLE's minimal model with three changes: sensors at
// the UPSTREAM fibre ends only, a run-time choice of filament material
// (RADSIMPLE_CAPILLARY = LUAG | DSB1), and a 1-11 GeV beam with momentum
// spread. Read the source in this order:
//   DetectorConstruction  — geometry + materials (the physics inputs)
//   PrimaryGeneratorAction— the electron beam
//   SteppingAction        — detect photons at the upstream sensors, tally edep
//   EventAction           — per-event bookkeeping, the timing observables
//   RunAction             — histograms and the output file
//
// Usage (from build/):
//   RADSIMPLE_CAPILLARY=LUAG ./radtb26            geometry viewer
//   RADSIMPLE_CAPILLARY=LUAG ./radtb26 run.mac    batch run
// Normal use is through ../run_tb26.sh, which sets everything.

#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"
#include "FTFP_BERT.hh"
#include "G4EmStandardPhysics_option4.hh"
#include "G4OpticalPhysics.hh"
#include "G4OpticalParameters.hh"
#include "G4Threading.hh"
#include <cstdlib>
#include <string>

#include "DetectorConstruction.hh"
#include "ActionInitialization.hh"

int main(int argc, char** argv) {
    auto runManager = G4RunManagerFactory::CreateRunManager();

    // Thread count: ALL cores by default; RADSIMPLE_THREADS=N overrides. Keep
    // /run/numberOfThreads OUT of macros — it would silently win over this.
    {
        int nThreads = G4Threading::G4GetNumberOfCores();
        if (const char* t = std::getenv("RADSIMPLE_THREADS")) {
            int v = std::atoi(t);
            if (v > 0) nThreads = v;
        }
        runManager->SetNumberOfThreads(nThreads);
        G4cout << "[TB26] threads: " << nThreads << " of "
               << G4Threading::G4GetNumberOfCores() << " cores  (RADSIMPLE_THREADS)" << G4endl;
    }

    runManager->SetUserInitialization(new DetectorConstruction());

    // Standard EM shower physics plus optical photons (Scintillation, OpWLS,
    // OpBoundary, OpAbsorption). RADSIMPLE_OPTICAL=0 turns the light off for
    // fast energy-only checks.
    auto physics = new FTFP_BERT();
    physics->ReplacePhysics(new G4EmStandardPhysics_option4());
    bool useOptical = true;
    if (const char* o = std::getenv("RADSIMPLE_OPTICAL")) useOptical = (std::string(o) != "0");
    if (useOptical) physics->RegisterPhysics(new G4OpticalPhysics());
    runManager->SetUserInitialization(physics);

    if (useOptical) {
        auto* op = G4OpticalParameters::Instance();
        op->SetScintTrackSecondariesFirst(true);
        op->SetCerenkovTrackSecondariesFirst(true);
    }

    runManager->SetUserInitialization(new ActionInitialization());
    G4cout << "[TB26] optical photons: " << (useOptical ? "ON" : "OFF") << G4endl;

    auto visManager = new G4VisExecutive();
    visManager->Initialize();
    auto UI = G4UImanager::GetUIpointer();

    if (argc >= 2) {
        UI->ApplyCommand("/control/execute " + G4String(argv[1]));
    } else {
        auto ui = new G4UIExecutive(argc, argv);
        UI->ApplyCommand("/control/execute vis.mac");
        ui->SessionStart();
        delete ui;
    }

    delete visManager;
    delete runManager;
    return 0;
}
