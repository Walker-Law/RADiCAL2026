#include "EventAction.hh"
#include "G4AnalysisManager.hh"
#include "G4Event.hh"
#include "G4PrimaryVertex.hh"
#include "G4SystemOfUnits.hh"

void EventAction::BeginOfEventAction(const G4Event*) {
    fElyso = 0.;
    fEw    = 0.;
    fNpe   = 0.;
    fNpeWLS = 0.;
    fNpeCenter = 0.;
    fTmcp  = kBig;
    fEtrig.fill(0.);
    fEpbGlass = 0.;
    fTup.fill(kBig);
    fTdn.fill(kBig);
    fTupW.fill(kBig);
    fTdnW.fill(kBig);
    fLayerEacc.fill(0.);
    fCornerNpeAcc.fill(0.);
    fPhT.clear();
    fPhId.clear();
    fPhWls.clear();
}

void EventAction::EndOfEventAction(const G4Event* evt) {
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

    // Beam-spot truth: where this event's primary actually started (mm).
    G4double x = 0., y = 0.;
    if (auto* v = evt->GetPrimaryVertex()) { x = v->GetX0()/mm; y = v->GetY0()/mm; }

    a->FillNtupleDColumn(0, fElyso / GeV);
    a->FillNtupleDColumn(1, fNpe);
    // no-timing sentinel is -999, NOT -1: with the realistic beam spot, real
    // dT values of a few ns (either sign) occur, so -1 is a legal physics value
    a->FillNtupleDColumn(2, (n > 0) ? dTmean : -999.);
    // Optional-component observables (all -1/0 when the component is off/idle):
    a->FillNtupleDColumn(3, fNpeCenter);                       // central E-type light
    a->FillNtupleDColumn(4, (fTmcp < kBig) ? fTmcp : -1.);     // MCP t0 (ns)
    a->FillNtupleDColumn(5, fEtrig[0] / MeV);                  // trigger 1 dE
    a->FillNtupleDColumn(6, fEtrig[1] / MeV);                  // trigger 2 dE
    a->FillNtupleDColumn(7, fEpbGlass / GeV);                  // Pb-glass (leakage)
    a->FillNtupleDColumn(8, x);                                // primary x (mm)
    a->FillNtupleDColumn(9, y);                                // primary y (mm)
    a->FillNtupleDColumn(10, fEw / GeV);                       // W absorber dE

    // Vector columns (bound by reference in RunAction — fill, then row).
    fLayerE.assign(fLayerEacc.begin(), fLayerEacc.end());
    for (auto& e : fLayerE) e /= GeV;
    fCornerNpe.assign(fCornerNpeAcc.begin(), fCornerNpeAcc.end());
    fCornerTup.resize(4); fCornerTdn.resize(4);
    for (int k = 0; k < 4; ++k) {
        fCornerTup[k] = (fTup[k] < kBig) ? fTup[k] : -999.;
        fCornerTdn[k] = (fTdn[k] < kBig) ? fTdn[k] : -999.;
    }
    // fPhT/fPhId already accumulated (empty unless RADSIMPLE_STORE_PHOTON_TIMES=1)

    a->AddNtupleRow();
}
