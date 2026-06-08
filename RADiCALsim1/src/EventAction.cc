#include "EventAction.hh"
#include "G4Event.hh"
#include "G4AnalysisManager.hh"
#include "G4SystemOfUnits.hh"
#include <numeric>
#include <cmath>

EventAction::EventAction() {
    fEdepLYSO.fill(0.);
    fEdepW.fill(0.);
    fEdepCenter = 0.;
    fEdepWLS.fill(0.);
}
EventAction::~EventAction() {}

void EventAction::BeginOfEventAction(const G4Event*) {
    fEdepLYSO.fill(0.);
    fEdepW.fill(0.);
    fEdepCenter = 0.;
    fEdepWLS.fill(0.);
    fCornerHits.clear();
    fLYSOHits.clear();
}

void EventAction::EndOfEventAction(const G4Event*) {
    auto am = G4AnalysisManager::Instance();

    // --- Shower profile: energy per LYSO layer ---
    G4double totalLYSO = 0.;
    for (G4int i = 0; i < 29; i++) {
        if (fEdepLYSO[i] > 0.)
            am->FillH1(0, i + 0.5, fEdepLYSO[i] / MeV);
        totalLYSO += fEdepLYSO[i];
    }

    // --- Total energy in W (invisible/absorber) ---
    G4double totalW = 0.;
    for (G4int i = 0; i < 28; i++) totalW += fEdepW[i];

    // --- Fill summary histograms ---
    if (totalLYSO > 0.) am->FillH1(1, totalLYSO / GeV);
    if (totalW    > 0.) am->FillH1(2, totalW    / GeV);

    G4double totalActive = totalLYSO + totalW;
    if (totalActive > 0.) am->FillH1(3, totalLYSO / totalActive);

    // --- Central EJ309 energy capillary ---
    if (fEdepCenter > 0.) am->FillH1(4, fEdepCenter / MeV);

    // --- Corner WLS (DSB1) energies per capillary ---
    for (G4int c = 0; c < 4; c++)
        if (fEdepWLS[c] > 0.) am->FillH1(5, fEdepWLS[c] / MeV);

    // --- Timing reconstruction from corner WLS hits ---
    // Longitudinal position z is reconstructed from differential arrival time
    // deltaT = (distBack - distFront) / v_light_in_quartz
    // where distFront = z - z_front_face, distBack = z_back_face - z
    // v_quartz = c / n,  n = 1.46 (fused silica)
    const G4double stackHalfZ = 57.03 * mm;
    const G4double c_light    = 299.792458 * mm / ns;
    const G4double v_quartz   = c_light / 1.46;

    // Use energy-weighted centroid per corner for better timing estimate
    std::array<G4double, 4> cornerEdepSum = {0, 0, 0, 0};
    std::array<G4double, 4> cornerZSum    = {0, 0, 0, 0};
    std::array<G4double, 4> cornerTSum    = {0, 0, 0, 0};

    for (const auto& hit : fCornerHits) {
        int c = (int)hit.corner;
        cornerEdepSum[c] += hit.edep;
        cornerZSum[c]    += hit.edep * hit.z;
        cornerTSum[c]    += hit.edep * hit.t;
    }

    for (G4int c = 0; c < 4; c++) {
        if (cornerEdepSum[c] <= 0.) continue;
        G4double zHit   = cornerZSum[c] / cornerEdepSum[c];  // energy-weighted z
        G4double tHit   = cornerTSum[c] / cornerEdepSum[c];  // energy-weighted t

        G4double distFront = zHit - (-stackHalfZ);
        G4double distBack  = stackHalfZ - zHit;

        G4double tArrFront = tHit + distFront / v_quartz;
        G4double tArrBack  = tHit + distBack  / v_quartz;

        G4double deltaT    = tArrBack - tArrFront;  // negative if z < 0
        G4double zReco     = -deltaT * v_quartz / 2.0;  // reconstructed z from deltaT

        am->FillH1(6, deltaT / ns);
        am->FillH2(0, deltaT / ns, zHit / mm);
        am->FillH2(1, zReco  / mm, zHit / mm);
    }
}
