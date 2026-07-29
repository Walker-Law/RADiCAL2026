#include "RunAction.hh"
#include "EventAction.hh"
#include "G4AnalysisManager.hh"
#include "G4Run.hh"
#include <cstdlib>
#include <vector>

RunAction::RunAction(EventAction* eventAction) {
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
    // dTwls — the SAME estimator restricted to WLS-created photons. Fit THIS
    // one: the all-light dT above is bimodal at thinned light (prompt
    // Cherenkov vs delayed WLS supplying the first photon at random), which
    // makes a Gaussian core fit meaningless. See EventAction::RecordPhoton.
    a->CreateH1("dTwls", "t_{down}-t_{up}, WLS photons only;#DeltaT (ns);events",
                4000, -5.0, 5.0);

    // Full photon-time dump: only if RADSIMPLE_STORE_PHOTON_TIMES=1 (grows the
    // files ~100x — see README "How much data is stored"). The flag must be
    // read HERE (not just in EventAction) because the column has to exist in
    // the ntuple schema on every thread AND on the master for merging.
    const char* sp = std::getenv("RADSIMPLE_STORE_PHOTON_TIMES");
    const bool storePhotons = (sp && std::atof(sp) != 0.);
    if (eventAction) eventAction->SetStorePhotons(storePhotons);

    a->CreateNtuple("ev", "one row per event");
    a->CreateNtupleDColumn("Elyso");     // GeV, all 29 LYSO plates (truth dE/dx)
    a->CreateNtupleDColumn("Npe");       // detected photons, 4 corner fibres
    a->CreateNtupleDColumn("dT");        // ns, 4-corner mean (-999 = no timing)
    a->CreateNtupleDColumn("NpeCenter"); // central E-type light (0 unless RADSIMPLE_CENTER_ETYPE=1)
    a->CreateNtupleDColumn("tMCP");      // MCP particle-arrival time, ns (-1 = MCP off/missed)
    a->CreateNtupleDColumn("eTrig1");    // trigger counter 1 deposit, MeV
    a->CreateNtupleDColumn("eTrig2");    // trigger counter 2 deposit, MeV
    a->CreateNtupleDColumn("ePbGlass");  // Pb-glass tail-catcher deposit, GeV
    a->CreateNtupleDColumn("x");         // primary x at the gun, mm (beam spot truth)
    a->CreateNtupleDColumn("y");         // primary y, mm
    a->CreateNtupleDColumn("Ew");        // GeV, all 28 W plates (absorber dE/dx)

    // Vector columns — bound BY REFERENCE to the EventAction's members, read
    // automatically at AddNtupleRow(). The master RunAction never fills a row,
    // so it binds function-local statics purely to declare the same schema.
    static std::vector<G4double> dummy;
    auto& lay = eventAction ? eventAction->fLayerE    : dummy;
    auto& npc = eventAction ? eventAction->fCornerNpe : dummy;
    auto& tup = eventAction ? eventAction->fCornerTup : dummy;
    auto& tdn = eventAction ? eventAction->fCornerTdn : dummy;
    a->CreateNtupleDColumn("Elayer",    lay);  // GeV per LYSO layer [29] -> longitudinal profile
    a->CreateNtupleDColumn("NpeCorner", npc);  // photons per corner [4]  -> asymmetry / starvation
    a->CreateNtupleDColumn("tUp",       tup);  // first-photon time per corner, up end [4] (-999 = none)
    a->CreateNtupleDColumn("tDn",       tdn);  // same, down end [4]
    if (storePhotons) {
        auto& pht = eventAction ? eventAction->fPhT  : dummy;
        auto& phi = eventAction ? eventAction->fPhId : dummy;
        a->CreateNtupleDColumn("phT",  pht);   // EVERY detected photon's time (ns)
        a->CreateNtupleDColumn("phId", phi);   // its channel: corner + 4*(down end)
    }
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
