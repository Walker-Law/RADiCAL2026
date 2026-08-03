#include "EventAction.hh"
#include "G4AnalysisManager.hh"
#include "G4Event.hh"
#include "G4PrimaryVertex.hh"
#include "G4SystemOfUnits.hh"
#include <algorithm>

void EventAction::BeginOfEventAction(const G4Event*) {
    fElyso = 0.;
    fEw    = 0.;
    fNpe   = 0.;
    fNpeWLS = 0.;
    fNpeCenter = 0.;
    fTmcp  = kBig;
    fEtrig.fill(0.);
    fEpbGlass = 0.;
    for (auto& v : fT) v.clear();
    fLayerEacc.fill(0.);
    fCornerNpeAcc.fill(0.);
    fPhT.clear();
    fPhId.clear();
    fPhWls.clear();
}

// The electronics-free 5% CFD for one SiPM: the arrival time of the
// ceil(kCfdFrac*N)-th photon (see EventAction.hh header for why this is the
// light-only analog of the test-beam 5% CFD). nth_element is O(N) — no full
// sort needed even at true light (~10^5 photons/end).
static G4double t05(std::vector<G4double>& v) {
    if (v.empty()) return -999.;
    size_t k = (size_t)std::ceil(EventAction::kCfdFrac * v.size());
    if (k > 0) --k;                          // 1-based -> index
    std::nth_element(v.begin(), v.begin()+k, v.end());
    return v[k];
}

void EventAction::EndOfEventAction(const G4Event* evt) {
    auto a = G4AnalysisManager::Instance();

    // Timing: per corner with light on BOTH ends, dTcfd = t05(down)-t05(up);
    // average over the corners that fired.
    fT05Up.assign(4, -999.);
    fT05Dn.assign(4, -999.);
    G4double sum = 0.; int n = 0;
    for (int c = 0; c < 4; ++c) {
        fT05Up[c] = t05(fT[c]);
        fT05Dn[c] = t05(fT[c+4]);
        if (fT05Up[c] > -999. && fT05Dn[c] > -999.) {
            sum += fT05Dn[c] - fT05Up[c]; ++n;
        }
    }
    const G4double dTcfd = (n > 0) ? sum / n : -999.;

    a->FillH1(0, fElyso / GeV);
    a->FillH1(1, fNpe);
    if (n > 0) a->FillH1(2, dTcfd);

    // Beam-spot truth: where this event's primary actually started (mm).
    G4double x = 0., y = 0.;
    if (auto* v = evt->GetPrimaryVertex()) { x = v->GetX0()/mm; y = v->GetY0()/mm; }

    a->FillNtupleDColumn(0, fElyso / GeV);
    a->FillNtupleDColumn(1, fNpe);
    a->FillNtupleDColumn(2, dTcfd);                            // -999 = no timing
    a->FillNtupleDColumn(3, fNpeCenter);                       // central E-type light
    a->FillNtupleDColumn(4, (fTmcp < kBig) ? fTmcp : -1.);     // MCP t0 (ns)
    a->FillNtupleDColumn(5, fEtrig[0] / MeV);                  // trigger 1 dE
    a->FillNtupleDColumn(6, fEtrig[1] / MeV);                  // trigger 2 dE
    a->FillNtupleDColumn(7, fEpbGlass / GeV);                  // Pb-glass (leakage)
    a->FillNtupleDColumn(8, x);                                // primary x (mm)
    a->FillNtupleDColumn(9, y);                                // primary y (mm)
    a->FillNtupleDColumn(10, fEw / GeV);                       // W absorber dE
    a->FillNtupleDColumn(11, fNpeWLS);                         // WLS share of Npe

    // Vector columns (bound by reference in RunAction — fill, then row).
    fLayerE.assign(fLayerEacc.begin(), fLayerEacc.end());
    for (auto& e : fLayerE) e /= GeV;
    fCornerNpe.assign(fCornerNpeAcc.begin(), fCornerNpeAcc.end());
    // fT05Up/fT05Dn filled above; fPhT/fPhId/fPhWls accumulated during event.

    a->AddNtupleRow();
}
