// radsimple.cc — main program for the SIMPLE RADiCAL timing simulation.
//
// This is a deliberately minimal, readable model of the RADiCAL module:
//   * a LYSO/W sampling stack that develops the electromagnetic shower,
//   * 4 corner WLS timing fibres read out at both ends,
//   * NO electronics — timing is the raw first-photon arrival-time difference.
//
// The whole simulation is ~6 short source files. Read them in this order:
//   DetectorConstruction  — geometry + materials (the physics inputs)
//   PrimaryGeneratorAction— the electron beam
//   SteppingAction        — what happens at each step (detect photons, tally edep)
//   EventAction           — per-event bookkeeping + fills the histograms
//   RunAction             — defines the histograms and the output file
//
// Usage (from build/):
//   ./radsimple                 open the OpenGL geometry viewer
//   ./radsimple run.mac         batch run -> radsimple_output.root
//   RADSIMPLE_OPTICAL=0 ...      turn optical-photon tracking off (fast, energy only)

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

    // Thread count: ALL cores by default. Override with RADSIMPLE_THREADS=N.
    // Set here (not in the macro) so it is tunable per machine without editing
    // -- and thus re-cmake-ing -- a .mac file. NOTE: if a macro contains
    // /run/numberOfThreads it runs later and WINS, so keep macros free of it.
    // Lower it if a machine's ntuple merge misbehaves at high thread counts
    // (symptom: ~200-byte per-thread files and an empty merged output); at very
    // high counts with few events per thread, physics-table startup also starts
    // to dominate, so more threads stop helping.
    {
        int nThreads = G4Threading::G4GetNumberOfCores();
        if (const char* t = std::getenv("RADSIMPLE_THREADS")) {
            int v = std::atoi(t);
            if (v > 0) nThreads = v;
        }
        runManager->SetNumberOfThreads(nThreads);
        G4cout << "[WRAP] threads: " << nThreads << " of "
               << G4Threading::G4GetNumberOfCores() << " cores"
               << "  (RADSIMPLE_THREADS)" << G4endl;
    }

    runManager->SetUserInitialization(new DetectorConstruction());

    // Physics: standard EM shower (FTFP_BERT + option4 EM) plus optical photons.
    // G4OpticalPhysics brings the four processes the WLS chain needs:
    //   Scintillation (LYSO makes 420 nm light), OpWLS (DSB1 absorbs it and
    //   re-emits 495 nm), OpBoundary (reflection/refraction at surfaces),
    //   OpAbsorption (bulk absorption). Optical tracking is ~100x slower, so it
    //   can be switched off with RADSIMPLE_OPTICAL=0 for energy-only studies.
    auto physics = new FTFP_BERT();
    physics->ReplacePhysics(new G4EmStandardPhysics_option4());
    bool useOptical = true;
    if (const char* o = std::getenv("RADSIMPLE_OPTICAL")) useOptical = (std::string(o) != "0");
    if (useOptical) physics->RegisterPhysics(new G4OpticalPhysics());
    runManager->SetUserInitialization(physics);

    if (useOptical) {
        auto* op = G4OpticalParameters::Instance();
        op->SetScintTrackSecondariesFirst(true);   // track the shower first, then its light
        op->SetCerenkovTrackSecondariesFirst(true);
    }

    runManager->SetUserInitialization(new ActionInitialization());
    G4cout << "[WRAP] optical photons: " << (useOptical ? "ON" : "OFF") << G4endl;

    auto visManager = new G4VisExecutive();
    visManager->Initialize();
    auto UI = G4UImanager::GetUIpointer();

    if (argc >= 2) {                      // a macro was given -> batch mode
        UI->ApplyCommand("/control/execute " + G4String(argv[1]));
    } else {                              // no argument -> interactive viewer
        auto ui = new G4UIExecutive(argc, argv);
        UI->ApplyCommand("/control/execute vis.mac");
        ui->SessionStart();
        delete ui;
    }

    delete visManager;
    delete runManager;
    return 0;
}
