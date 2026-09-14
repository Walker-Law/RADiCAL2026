#include "EventAction.hh"
#include "G4AnalysisManager.hh"
#include "G4Event.hh"
#include "G4PrimaryVertex.hh"
#include "G4SystemOfUnits.hh"
#include <algorithm>
#include <cmath>

void EventAction::BeginOfEventAction(const G4Event*) {
    fElyso = 0.; fEw = 0.; fEfil = 0.; fNpe = 0.;
    fNorigin.fill(0.);
    fTmcp = kBig; fEtrig.fill(0.);
    for (auto& v : fT) v.clear();
    fLayerEacc.fill(0.); fCornerNpeAcc.fill(0.);
    fPhT.clear(); fPhId.clear(); fPhOrigin.clear();
}

// The electronics-free 5% trigger: arrival time of the ceil(0.05 N)-th photon.
// std::nth_element is O(N) — matters at true light (10^4-10^5 photons/corner).
static G4double t05(std::vector<G4double>& v) {
    if (v.empty()) return -999.;
    size_t k = (size_t)std::ceil(EventAction::kCfdFrac * v.size());
    if (k > 0) --k;                          // 1-based -> index
    std::nth_element(v.begin(), v.begin()+k, v.end());
    return v[k];
}

void EventAction::EndOfEventAction(const G4Event* evt) {
    auto a = G4AnalysisManager::Instance();

    fT05Up.assign(4, -999.);
    int n = 0; G4double sum = 0.;
    for (int c = 0; c < 4; ++c) {
        fT05Up[c] = t05(fT[c]);
        if (fT05Up[c] > -999.) { sum += fT05Up[c]; ++n; }
    }
    const G4double tUpMean = (n > 0) ? sum / n : -999.;
    // Diagonal-difference estimator, EXACTLY the beam test's DiagDiff.C:
    //   (mean of one diagonal pair) - (mean of the other diagonal pair),
    // pairs 0-3 and 1-2 (corner convention in EventAction.hh). Each pair mean
    // is insensitive to a beam-position shift in EITHER x or y to first order
    // (opposite corners move oppositely), so the difference cancels position
    // drift as well as the event start time. (The previous definition,
    // averaging the two within-pair differences, cancelled only one axis.)
    const bool all4 = (n == 4);
    const G4double dTpair = all4
        ? 0.5 * (fT05Up[0] + fT05Up[3]) - 0.5 * (fT05Up[1] + fT05Up[2])
        : -999.;

    a->FillH1(0, fElyso / GeV);
    a->FillH1(1, fNpe);
    if (all4)  a->FillH1(2, dTpair);
    if (n > 0) a->FillH1(3, tUpMean);

    G4double x = 0., y = 0.;
    if (auto* v = evt->GetPrimaryVertex()) { x = v->GetX0()/mm; y = v->GetY0()/mm; }

    a->FillNtupleDColumn(0,  fElyso / GeV);
    a->FillNtupleDColumn(1,  fNpe);
    a->FillNtupleDColumn(2,  dTpair);
    a->FillNtupleDColumn(3,  tUpMean);
    a->FillNtupleDColumn(4,  (G4double)n);
    a->FillNtupleDColumn(5,  (fTmcp < kBig) ? fTmcp : -1.);
    a->FillNtupleDColumn(6,  fEtrig[0] / MeV);
    a->FillNtupleDColumn(7,  fEtrig[1] / MeV);
    a->FillNtupleDColumn(8,  x);
    a->FillNtupleDColumn(9,  y);
    a->FillNtupleDColumn(10, fEw / GeV);
    a->FillNtupleDColumn(11, fEfil / MeV);
    a->FillNtupleDColumn(12, fNorigin[0]);   // Cherenkov
    a->FillNtupleDColumn(13, fNorigin[1]);   // direct LYSO scintillation
    a->FillNtupleDColumn(14, fNorigin[2]);   // wavelength-shifted
    a->FillNtupleDColumn(15, fNorigin[3]);   // filament self-scintillation

    fLayerE.assign(fLayerEacc.begin(), fLayerEacc.end());
    for (auto& e : fLayerE) e /= GeV;
    fCornerNpe.assign(fCornerNpeAcc.begin(), fCornerNpeAcc.end());

    a->AddNtupleRow();
}
