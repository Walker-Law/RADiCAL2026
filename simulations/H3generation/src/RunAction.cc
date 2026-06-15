#include "RunAction.hh"
#include "G4Run.hh"
#include "G4AccumulableManager.hh"
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
    am->RegisterAccumulable(fSourceNeutrons);
}

void RunAction::BeginOfRunAction(const G4Run*) {
    G4AccumulableManager::Instance()->Reset();
}

void RunAction::AddTriton(const G4String& vol) {
    if      (vol == "Be_Shell")      fTritonsBe      += 1;
    else if (vol == "Graphite_Shell") fTritonsGraph   += 1;
    else if (vol == "Blanket")        fTritonsBlanket += 1;
}

void RunAction::AddParticleEntering(const G4String& vol) {
    if      (vol == "Be_Shell")       fEnterBe      += 1;
    else if (vol == "Graphite_Shell") fEnterGraph   += 1;
    else if (vol == "Blanket")        fEnterBlanket += 1;
}

void RunAction::EndOfRunAction(const G4Run* run) {
    G4AccumulableManager::Instance()->Merge();

    G4int nSrc   = run->GetNumberOfEvent();
    G4int tBe    = fTritonsBe.GetValue();
    G4int tGraph = fTritonsGraph.GetValue();
    G4int tBlank = fTritonsBlanket.GetValue();
    G4int tTotal = tBe + tGraph + tBlank;

    G4int eBe    = fEnterBe.GetValue();
    G4int eGraph = fEnterGraph.GetValue();
    G4int eBlank = fEnterBlanket.GetValue();

    auto ratio = [](G4int num, G4int den) -> G4double {
        return den > 0 ? (G4double)num / den : 0.0;
    };

    G4cout << "\n======================================================\n";
    G4cout << "  TRITIUM (H-3) PRODUCTION SUMMARY\n";
    G4cout << "======================================================\n";
    G4cout << std::left << std::setw(28) << "  Source neutrons:"   << nSrc   << "\n";
    G4cout << std::left << std::setw(28) << "  Total tritons:"     << tTotal << "\n";
    G4cout << std::left << std::setw(28) << "  Overall TBR:"
           << std::fixed << std::setprecision(4) << ratio(tTotal, nSrc) << "\n";
    G4cout << "------------------------------------------------------\n";
    G4cout << "  Layer               Tritons  Entering   H3/Entering\n";
    G4cout << "------------------------------------------------------\n";
    G4cout << std::left << std::setw(20) << "  Be multiplier"
           << std::setw(9) << tBe << std::setw(11) << eBe
           << std::fixed << std::setprecision(4) << ratio(tBe, eBe) << "\n";
    G4cout << std::left << std::setw(20) << "  Graphite moderator"
           << std::setw(9) << tGraph << std::setw(11) << eGraph
           << std::fixed << std::setprecision(4) << ratio(tGraph, eGraph) << "\n";
    G4cout << std::left << std::setw(20) << "  LiSiO blanket"
           << std::setw(9) << tBlank << std::setw(11) << eBlank
           << std::fixed << std::setprecision(4) << ratio(tBlank, eBlank) << "\n";
    G4cout << "======================================================\n\n";
}
