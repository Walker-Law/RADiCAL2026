#include "RunAction.hh"
#include "G4Run.hh"
#include "G4AccumulableManager.hh"
#include "G4AnalysisManager.hh"
#include "G4SystemOfUnits.hh"
#include <iomanip>

RunAction::RunAction() {
    G4AccumulableManager* am = G4AccumulableManager::Instance();
    am->RegisterAccumulable(fTritonsBe);
    am->RegisterAccumulable(fTritonsGraph);
    am->RegisterAccumulable(fTritonsBlanket);
    am->RegisterAccumulable(fEnterBe);
    am->RegisterAccumulable(fEnterGraph);
    am->RegisterAccumulable(fEnterBlanket);
    am->RegisterAccumulable(fNeutronInteractions);
    am->RegisterAccumulable(fNeutronInteractionsBlanket);

    auto* ana = G4AnalysisManager::Instance();
    ana->SetDefaultFileType("root");
    ana->SetFileName("H3generation");
    ana->SetVerboseLevel(0);

    // Spectra histograms (indices 0–4)
    ana->CreateH1("nSpec_Be",      "Neutron KE entering Be (MeV)",       150, 0., 15.);
    ana->CreateH1("nSpec_Graph",   "Neutron KE entering Graphite (MeV)", 150, 0., 15.);
    ana->CreateH1("nSpec_Blanket", "Neutron KE entering Blanket (MeV)",  150, 0., 15.);
    ana->CreateH1("triton_KE",     "Triton KE at production (MeV)",      100, 0.,  5.);
    ana->CreateH1("triton_R",      "Triton production radius (cm)",        70, 100., 170.);

    // Scalar summary histograms (indices 5–8) — 1 bin each, filled at end of run.
    // Bin content = the scalar value (filled once with that value as weight).
    ana->CreateH1("nTritons_total",        "Total H3 produced",                          1, 0., 1.);
    ana->CreateH1("nNeutronInt_blanket",   "Neutron interactions in blanket",             1, 0., 1.);
    ana->CreateH1("TBR_per_source",        "H3 / source neutrons",                       1, 0., 1.);
    ana->CreateH1("TBR_per_blanket_int",   "H3 / neutron interactions in blanket",       1, 0., 1.);
}

void RunAction::BeginOfRunAction(const G4Run*) {
    G4AccumulableManager::Instance()->Reset();
    G4AnalysisManager::Instance()->OpenFile();
}

void RunAction::AddTriton(const G4String& vol, G4double ke, G4double r) {
    if      (vol == "Be_Shell")       fTritonsBe      += 1;
    else if (vol == "Graphite_Shell") fTritonsGraph   += 1;
    else if (vol == "Blanket")        fTritonsBlanket += 1;

    auto* ana = G4AnalysisManager::Instance();
    ana->FillH1(3, ke / MeV);
    ana->FillH1(4, r  / cm);
}

void RunAction::AddParticleEntering(const G4String& vol, G4double ke) {
    auto* ana = G4AnalysisManager::Instance();
    if (vol == "Be_Shell") {
        fEnterBe += 1;
        ana->FillH1(0, ke / MeV);
    } else if (vol == "Graphite_Shell") {
        fEnterGraph += 1;
        ana->FillH1(1, ke / MeV);
    } else if (vol == "Blanket") {
        fEnterBlanket += 1;
        ana->FillH1(2, ke / MeV);
    }
}

void RunAction::AddNeutronInteraction(const G4String& vol) {
    fNeutronInteractions += 1;
    if (vol == "Blanket") fNeutronInteractionsBlanket += 1;
}

void RunAction::EndOfRunAction(const G4Run* run) {
    G4AccumulableManager::Instance()->Merge();

    G4int nSrc        = run->GetNumberOfEvent();
    G4int tBe         = fTritonsBe.GetValue();
    G4int tGraph      = fTritonsGraph.GetValue();
    G4int tBlank      = fTritonsBlanket.GetValue();
    G4int tTotal      = tBe + tGraph + tBlank;
    G4int nInt        = fNeutronInteractions.GetValue();
    G4int nIntBlanket = fNeutronInteractionsBlanket.GetValue();
    G4int eBe         = fEnterBe.GetValue();
    G4int eGraph      = fEnterGraph.GetValue();
    G4int eBlank      = fEnterBlanket.GetValue();

    auto ratio = [](G4int num, G4int den) -> G4double {
        return den > 0 ? (G4double)num / den : 0.0;
    };

    // Fill scalar summary histograms (1 bin each, weight = the value).
    // FillH1(id, x=0.5, weight) → bin content = weight after one fill.
    auto* ana = G4AnalysisManager::Instance();
    ana->FillH1(5, 0.5, (G4double)tTotal);
    ana->FillH1(6, 0.5, (G4double)nIntBlanket);
    ana->FillH1(7, 0.5, ratio(tTotal, nSrc));
    ana->FillH1(8, 0.5, ratio(tTotal, nIntBlanket));

    ana->Write();
    ana->CloseFile();

    G4cout << "\n======================================================\n";
    G4cout << "  TRITIUM (H-3) PRODUCTION SUMMARY\n";
    G4cout << "======================================================\n";
    G4cout << std::left << std::setw(34) << "  Source neutrons:"               << nSrc        << "\n";
    G4cout << std::left << std::setw(34) << "  Total tritons:"                 << tTotal      << "\n";
    G4cout << std::left << std::setw(34) << "  Neutron interactions (all):"    << nInt        << "\n";
    G4cout << std::left << std::setw(34) << "  Neutron interactions (blanket):"<< nIntBlanket << "\n";
    G4cout << std::left << std::setw(34) << "  TBR (tritons/source):"
           << std::fixed << std::setprecision(5) << ratio(tTotal, nSrc) << "\n";
    G4cout << std::left << std::setw(34) << "  TBR (tritons/n-int all):"
           << std::fixed << std::setprecision(5) << ratio(tTotal, nInt) << "\n";
    G4cout << std::left << std::setw(34) << "  TBR (tritons/n-int blanket):"
           << std::fixed << std::setprecision(5) << ratio(tTotal, nIntBlanket) << "\n";
    G4cout << "------------------------------------------------------\n";
    G4cout << "  Layer               Tritons  Entering   H3/Entering\n";
    G4cout << "------------------------------------------------------\n";
    G4cout << std::left << std::setw(20) << "  Be multiplier"
           << std::setw(9) << tBe   << std::setw(11) << eBe
           << std::fixed << std::setprecision(5) << ratio(tBe, eBe) << "\n";
    G4cout << std::left << std::setw(20) << "  Graphite moderator"
           << std::setw(9) << tGraph << std::setw(11) << eGraph
           << std::fixed << std::setprecision(5) << ratio(tGraph, eGraph) << "\n";
    G4cout << std::left << std::setw(20) << "  LiSiO blanket"
           << std::setw(9) << tBlank << std::setw(11) << eBlank
           << std::fixed << std::setprecision(5) << ratio(tBlank, eBlank) << "\n";
    G4cout << "======================================================\n";
    G4cout << "  ROOT output: H3generation.root\n";
    G4cout << "======================================================\n\n";
}
