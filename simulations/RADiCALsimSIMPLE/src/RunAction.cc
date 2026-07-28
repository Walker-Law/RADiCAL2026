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
    // dT range: with the realistic beam spot (RADSIMPLE_BEAM_SPOT_MM) the
    // distribution is much wider than the old pencil-beam one (starved far
    // corners at thinned light -> ns-scale tails), so the window must be wide
    // or the tails silently overflow and bias the core fit. 2.5 ps bins.
    a->CreateH1("dT",    "t_{down}-t_{up}, 4-corner mean;#DeltaT (ns);events",
                4000, -5.0, 5.0);
    a->CreateH1("dTc",   "t_{down}-t_{up}, per corner;#DeltaT (ns);corners",
                4000, -5.0, 5.0);

    a->CreateNtuple("ev", "one row per event");
    a->CreateNtupleDColumn("Elyso");     // GeV, all 29 LYSO plates (truth dE/dx)
    a->CreateNtupleDColumn("Npe");       // detected photons, 4 corner fibres
    a->CreateNtupleDColumn("dT");        // ns, 4-corner mean (-1 = no timing)
    a->CreateNtupleDColumn("NpeCenter"); // central E-type light (0 unless RADSIMPLE_CENTER_ETYPE=1)
    a->CreateNtupleDColumn("tMCP");      // MCP particle-arrival time, ns (-1 = MCP off/missed)
    a->CreateNtupleDColumn("eTrig1");    // trigger counter 1 deposit, MeV
    a->CreateNtupleDColumn("eTrig2");    // trigger counter 2 deposit, MeV
    a->CreateNtupleDColumn("ePbGlass");  // Pb-glass tail-catcher deposit, GeV
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
