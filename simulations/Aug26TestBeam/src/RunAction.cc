#include "RunAction.hh"
#include "EventAction.hh"
#include "G4AnalysisManager.hh"
#include "G4Run.hh"
#include <vector>

RunAction::RunAction(EventAction* eventAction) {
    auto a = G4AnalysisManager::Instance();
    a->SetDefaultFileType("root");
    a->SetFileName("radtb26_output");     // a macro overrides with /analysis/setFileName
    a->SetVerboseLevel(0);
    a->SetNtupleMerging(true);            // merge per-thread ntuples on MT runs

    // Ranges sized for 1-11 GeV at TRUE light (up to ~10^5 detected photons).
    a->CreateH1("Elyso",   "LYSO energy;E_{LYSO} (GeV);events",            250, 0., 12.5);
    a->CreateH1("Npe",     "photons detected (4 upstream corners);N_{pe};events", 250, 0., 250000.);
    a->CreateH1("dTpair",  "diagonal-pair 5%-quantile difference;#DeltaT_{pair} (ns);events",
                4000, -5.0, 5.0);
    a->CreateH1("tUpMean", "4-corner mean 5%-quantile time (absolute);t (ns);events",
                4000, 0., 100.);

    a->CreateNtuple("ev", "one row per event");
    a->CreateNtupleDColumn("Elyso");     //  0 GeV, all 29 LYSO plates (truth dE/dx)
    a->CreateNtupleDColumn("Npe");       //  1 detected photons, 4 upstream corners
    a->CreateNtupleDColumn("dTpair");    //  2 ns, realizable timing observable (-999 = <4 corners lit)
    a->CreateNtupleDColumn("tUpMean");   //  3 ns, absolute 4-corner mean (ideal-reference timing)
    a->CreateNtupleDColumn("nT05");      //  4 how many corners had light (0-4)
    a->CreateNtupleDColumn("tMCP");      //  5 MCP arrival time, ns (-1 = MCP off, the default)
    a->CreateNtupleDColumn("eTrig1");    //  6 trigger counter 1 deposit, MeV
    a->CreateNtupleDColumn("eTrig2");    //  7 trigger counter 2 deposit, MeV
    a->CreateNtupleDColumn("x");         //  8 primary x at the gun, mm (beam spot truth)
    a->CreateNtupleDColumn("y");         //  9 primary y, mm
    a->CreateNtupleDColumn("Ew");        // 10 GeV, all 28 W plates
    a->CreateNtupleDColumn("Efil");      // 11 MeV deposited in the 4 filaments (drives self-scintillation)
    a->CreateNtupleDColumn("NpeCher");   // 12 of Npe: Cherenkov-born
    a->CreateNtupleDColumn("NpeDirect"); // 13 of Npe: unshifted LYSO scintillation
    a->CreateNtupleDColumn("NpeWLS");    // 14 of Npe: wavelength-shifted in the filament
    a->CreateNtupleDColumn("NpeFil");    // 15 of Npe: the filament's own scintillation (LuAG:Ce)

    // Vector columns bound BY REFERENCE to the EventAction's members. The
    // master RunAction never fills a row, so it binds dummies for the schema.
    static std::vector<G4double> dummy;
    auto& lay = eventAction ? eventAction->fLayerE    : dummy;
    auto& npc = eventAction ? eventAction->fCornerNpe : dummy;
    auto& tu5 = eventAction ? eventAction->fT05Up     : dummy;
    auto& pht = eventAction ? eventAction->fPhT       : dummy;
    auto& phi = eventAction ? eventAction->fPhId      : dummy;
    auto& pho = eventAction ? eventAction->fPhOrigin  : dummy;
    a->CreateNtupleDColumn("Elayer",    lay);  // GeV per LYSO layer [29]
    a->CreateNtupleDColumn("NpeCorner", npc);  // photons per corner [4]
    a->CreateNtupleDColumn("t05Up",     tu5);  // 5% quantile time per corner [4] (-999 = none)
    // THE PERFECT WAVEFORM — every detected photon, always stored.
    a->CreateNtupleDColumn("phT",      pht);   // arrival time (ns)
    a->CreateNtupleDColumn("phId",     phi);   // corner 0..3
    a->CreateNtupleDColumn("phOrigin", pho);   // 0 Cherenkov, 1 direct LYSO, 2 shifted, 3 filament scintillation
    a->FinishNtuple();
}

void RunAction::BeginOfRunAction(const G4Run*) {
    G4AnalysisManager::Instance()->OpenFile();
}

void RunAction::EndOfRunAction(const G4Run*) {
    auto a = G4AnalysisManager::Instance();
    a->Write();
    a->CloseFile();
}
