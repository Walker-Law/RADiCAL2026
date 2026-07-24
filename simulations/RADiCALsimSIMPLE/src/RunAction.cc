#include "RunAction.hh"
#include "G4AnalysisManager.hh"
#include "G4Run.hh"

RunAction::RunAction() {
    auto a = G4AnalysisManager::Instance();
    a->SetDefaultFileType("root");
    a->SetFileName("radsimple_output");   // default; a macro can override with
                                           // /analysis/setFileName before /run/beamOn
                                           // (Geant4's built-in UI command) — used
                                           // by run.mac to write one file per energy.
    a->SetVerboseLevel(0);
    a->SetNtupleMerging(true);          // merge per-thread ntuples on MT runs

    a->CreateH1("Elyso", "LYSO energy;E_{LYSO} (GeV);events",      250, 0., 25.);
    a->CreateH1("Npe",   "photons detected;N_{pe};events",         200, 0., 40000.);
    a->CreateH1("dT",    "t_{down}-t_{up}, 4-corner mean;#DeltaT (ns);events",
                3000, -0.5, 1.0);       // 0.5 ps bins
    a->CreateH1("dTc",   "t_{down}-t_{up}, per corner;#DeltaT (ns);corners",
                3000, -0.5, 1.0);

    a->CreateNtuple("ev", "one row per event");
    a->CreateNtupleDColumn("Elyso");    // GeV
    a->CreateNtupleDColumn("Npe");      // detected photons
    a->CreateNtupleDColumn("dT");       // ns, 4-corner mean
    a->FinishNtuple();
}

void RunAction::BeginOfRunAction(const G4Run*) {
    G4AnalysisManager::Instance()->OpenFile();   // no name -> uses whatever
                                                  // SetFileName()/setFileName gave it
}

void RunAction::EndOfRunAction(const G4Run*) {
    auto a = G4AnalysisManager::Instance();
    a->Write();
    a->CloseFile();
}
