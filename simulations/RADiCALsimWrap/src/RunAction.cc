#include "RunAction.hh"
#include "EventAction.hh"
#include "G4AnalysisManager.hh"
#include "G4Run.hh"
#include <vector>

RunAction::RunAction(EventAction* eventAction) {
    auto a = G4AnalysisManager::Instance();
    a->SetDefaultFileType("root");
    a->SetFileName("radsimple_output");   // default; a macro can override with
                                           // /analysis/setFileName before /run/beamOn
                                           // (Geant4's built-in UI command) — used
                                           // by the sweep macros to write one file
                                           // per energy.
    a->SetVerboseLevel(0);
    a->SetNtupleMerging(true);          // merge per-thread ntuples on MT runs

    a->CreateH1("Elyso", "LYSO energy;E_{LYSO} (GeV);events",      250, 0., 25.);
    a->CreateH1("Npe",   "photons detected;N_{pe};events",         200, 0., 40000.);
    // dTcfd — the electronics-free 5% CFD (see EventAction.hh). Wide window:
    // with the realistic beam spot the distribution is much wider than a
    // pencil-beam one, and a too-narrow window silently overflows and biases
    // the core fit. 2.5 ps bins.
    a->CreateH1("dTcfd", "5%-CFD t_{down}-t_{up}, 4-corner mean;#DeltaT (ns);events",
                4000, -5.0, 5.0);

    a->CreateNtuple("ev", "one row per event");
    a->CreateNtupleDColumn("Elyso");     // GeV, all 29 LYSO plates (truth dE/dx)
    a->CreateNtupleDColumn("Npe");       // detected photons, 4 corner fibres
    a->CreateNtupleDColumn("dTcfd");     // ns, 5% CFD, 4-corner mean (-999 = none) <- THE timing observable
    a->CreateNtupleDColumn("NpeCenter"); // central E-type light (0 unless RADSIMPLE_CENTER_ETYPE=1)
    a->CreateNtupleDColumn("tMCP");      // MCP particle-arrival time, ns (-1 = MCP off/missed)
    a->CreateNtupleDColumn("eTrig1");    // trigger counter 1 deposit, MeV
    a->CreateNtupleDColumn("eTrig2");    // trigger counter 2 deposit, MeV
    a->CreateNtupleDColumn("ePbGlass");  // Pb-glass tail-catcher deposit, GeV
    a->CreateNtupleDColumn("x");         // primary x at the gun, mm (beam spot truth)
    a->CreateNtupleDColumn("y");         // primary y, mm
    a->CreateNtupleDColumn("Ew");        // GeV, all 28 W plates (absorber dE/dx)
    a->CreateNtupleDColumn("NpeWLS");    // of Npe, how many were OpWLS-created

    // Vector columns — bound BY REFERENCE to the EventAction's members, read
    // automatically at AddNtupleRow(). The master RunAction never fills a row,
    // so it binds function-local statics purely to declare the same schema.
    static std::vector<G4double> dummy;
    auto& lay = eventAction ? eventAction->fLayerE    : dummy;
    auto& npc = eventAction ? eventAction->fCornerNpe : dummy;
    auto& tu5 = eventAction ? eventAction->fT05Up     : dummy;
    auto& td5 = eventAction ? eventAction->fT05Dn     : dummy;
    auto& pht = eventAction ? eventAction->fPhT       : dummy;
    auto& phi = eventAction ? eventAction->fPhId      : dummy;
    auto& phw = eventAction ? eventAction->fPhWls     : dummy;
    a->CreateNtupleDColumn("Elayer",    lay);  // GeV per LYSO layer [29] -> longitudinal profile
    a->CreateNtupleDColumn("NpeCorner", npc);  // photons per corner [4]  -> asymmetry / position
    a->CreateNtupleDColumn("t05Up",     tu5);  // 5% CFD time per corner, up end [4] (-999 = none)
    a->CreateNtupleDColumn("t05Dn",     td5);  // same, down end [4]
    // THE PERFECT WAVEFORM — every detected photon, always stored. This is
    // the complete record of the light just before electronics; any estimator
    // (first-photon, other CFD fractions, SPTR smearing, pulse shapes) is
    // derivable from these three columns offline, so estimator studies are
    // analysis work, not reruns. Channel-end ids: 0-3 corner up, 4-7 corner
    // down, 8/9 center up/down (EventAction::ChanEnd).
    a->CreateNtupleDColumn("phT",   pht);      // arrival time (ns)
    a->CreateNtupleDColumn("phId",  phi);      // channel-end id
    a->CreateNtupleDColumn("phWls", phw);      // 1 = WLS-created, 0 = prompt
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
