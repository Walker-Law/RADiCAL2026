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

    // =========================================================================
    // 1. LONGITUDINAL SHOWER PROFILE + SHOWER SHAPE OBSERVABLES
    // =========================================================================
    G4double totalLYSO = 0.;
    G4int    showerMaxLayer = 0;
    G4double maxLayerEdep   = 0.;
    G4double cogNumer = 0., cogDenom = 0.;
    G4double rmsNumer = 0.;

    for (G4int i = 0; i < 29; i++) {
        G4double e = fEdepLYSO[i];
        if (e > 0.) {
            am->FillH1(0, i + 0.5, e / MeV);   // shower profile (weighted fill)
            if (e > maxLayerEdep) { maxLayerEdep = e; showerMaxLayer = i; }
            cogNumer += (i + 0.5) * e;
            cogDenom += e;
        }
        totalLYSO += e;
    }

    G4double cog = (cogDenom > 0.) ? cogNumer / cogDenom : 0.;  // in layer units

    // RMS of longitudinal distribution
    if (cogDenom > 0.) {
        for (G4int i = 0; i < 29; i++) {
            G4double dl = (i + 0.5) - cog;
            rmsNumer += fEdepLYSO[i] * dl * dl;
        }
    }
    G4double showerRMS = (cogDenom > 0.) ? std::sqrt(rmsNumer / cogDenom) : 0.;

    // =========================================================================
    // 2. ABSORBER + SUMMARY
    // =========================================================================
    G4double totalW = 0.;
    for (G4int i = 0; i < 28; i++) totalW += fEdepW[i];

    if (totalLYSO > 0.) am->FillH1(1, totalLYSO / GeV);
    if (totalW    > 0.) am->FillH1(2, totalW    / GeV);
    G4double totalActive = totalLYSO + totalW;
    if (totalActive > 0.) am->FillH1(3, totalLYSO / totalActive);

    // =========================================================================
    // 3. CAPILLARY SIGNALS
    // =========================================================================
    if (fEdepCenter > 0.) {
        am->FillH1(4, fEdepCenter / MeV);
        // H1[10]: center cap / total LYSO ratio
        if (totalLYSO > 0.)
            am->FillH1(10, (fEdepCenter / MeV) / (totalLYSO / MeV));
        // H2[3]: EJ309 vs total LYSO linearity
        am->FillH2(3, totalLYSO / GeV, fEdepCenter / MeV);
    }

    G4double totalCornerWLS = 0.;
    for (G4int c = 0; c < 4; c++) {
        if (fEdepWLS[c] > 0.) {
            am->FillH1(5, fEdepWLS[c] / MeV);          // all corners combined
            am->FillH1(11, c + 0.5, fEdepWLS[c] / MeV); // per-corner bar
            totalCornerWLS += fEdepWLS[c];
        }
    }
    if (totalCornerWLS > 0.) {
        am->FillH1(13, totalCornerWLS / MeV);
        // H2[4]: total corner WLS vs total LYSO
        if (totalLYSO > 0.)
            am->FillH2(4, totalLYSO / GeV, totalCornerWLS / MeV);
    }

    // =========================================================================
    // 4. SHOWER SHAPE HISTOGRAMS (per-event)
    // =========================================================================
    if (totalLYSO > 0.) {
        am->FillH1(7, showerMaxLayer + 0.5);    // shower max layer
        am->FillH1(8, cog);                      // longitudinal COG (layer units)
        am->FillH1(9, showerRMS);                // shower longitudinal RMS
        // H2[6]: shower max vs total LYSO
        am->FillH2(6, totalLYSO / GeV, showerMaxLayer + 0.5);
    }

    // =========================================================================
    // 5. X-Y LATERAL SHOWER PROFILE
    // =========================================================================
    for (const auto& hit : fLYSOHits) {
        // H2[2]: X-Y map weighted by edep (shows transverse shower extent)
        am->FillH2(2, hit.x / mm, hit.y / mm, hit.edep / MeV);
    }

    // =========================================================================
    // 6. TIMING RECONSTRUCTION
    // =========================================================================
    const G4double stackHalfZ = 57.03 * mm;
    const G4double c_light    = 299.792458 * mm / ns;
    const G4double v_quartz   = c_light / 1.46;

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
        G4double zHit = cornerZSum[c] / cornerEdepSum[c];
        G4double tHit = cornerTSum[c] / cornerEdepSum[c];

        G4double distFront = zHit - (-stackHalfZ);
        G4double distBack  = stackHalfZ - zHit;
        G4double tArrFront = tHit + distFront / v_quartz;
        G4double tArrBack  = tHit + distBack  / v_quartz;
        G4double deltaT    = tArrBack - tArrFront;
        G4double zReco     = -deltaT * v_quartz / 2.0;
        G4double zResid    = zReco - zHit;

        am->FillH1(6,  deltaT / ns);                    // DeltaT
        am->FillH1(12, zResid / mm);                    // Z residual
        am->FillH2(0,  deltaT / ns, zHit / mm);         // DeltaT vs true z
        am->FillH2(1,  zReco  / mm, zHit / mm);         // z_reco vs z_true
        // H2[5]: DeltaT vs total LYSO (timing resolution vs energy)
        if (totalLYSO > 0.)
            am->FillH2(5, totalLYSO / GeV, deltaT / ns);
    }
}
