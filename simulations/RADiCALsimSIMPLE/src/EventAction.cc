#include "EventAction.hh"
#include "G4AnalysisManager.hh"
#include "G4SystemOfUnits.hh"

void EventAction::BeginOfEventAction(const G4Event*) {
    fElyso = 0.;
    fNpe   = 0.;
    fTup.fill(kBig);
    fTdn.fill(kBig);
}

void EventAction::EndOfEventAction(const G4Event*) {
    auto a = G4AnalysisManager::Instance();

    // Timing observable: for every corner that saw a photon at BOTH ends,
    // dT = t(down) - t(up). Average over the corners that fired. This is the
    // raw first-photon time difference — no electronics of any kind.
    G4double sum = 0.; int n = 0;
    for (int k = 0; k < 4; ++k) {
        if (fTup[k] < kBig && fTdn[k] < kBig) {
            G4double dt = fTdn[k] - fTup[k];
            a->FillH1(3, dt);                 // per-corner (diagnostic)
            sum += dt; ++n;
        }
    }
    const G4double dTmean = (n > 0) ? sum / n : kBig;

    a->FillH1(0, fElyso / GeV);
    a->FillH1(1, fNpe);
    if (n > 0) a->FillH1(2, dTmean);

    a->FillNtupleDColumn(0, fElyso / GeV);
    a->FillNtupleDColumn(1, fNpe);
    a->FillNtupleDColumn(2, (n > 0) ? dTmean : -1.);
    a->AddNtupleRow();
}
