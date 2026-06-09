#include "EventAction.hh"
#include "G4Event.hh"
#include "G4AnalysisManager.hh"
#include "G4SystemOfUnits.hh"
#include <numeric>
#include <cmath>

// Sentinel for "no hit yet" times (ns); any real hit time is far below this.
static const G4double kBigTime = 1.0e9;

EventAction::EventAction() {
    fEdepLYSO.fill(0.);
    fEdepW.fill(0.);
    fEdepCenter = 0.;
    fEdepWLS.fill(0.);
    fEdepTrig.fill(0.);
    fTimeTrig.fill(kBigTime);
    fEdepMCP = 0.;
    fTimeMCP = kBigTime;
    fEdepPbGlass = 0.;
    fTphFront.fill(kBigTime); fTphBack.fill(kBigTime);
    fNphFront.fill(0);        fNphBack.fill(0);
}
EventAction::~EventAction() {}

void EventAction::BeginOfEventAction(const G4Event*) {
    fEdepLYSO.fill(0.);
    fEdepW.fill(0.);
    fEdepCenter = 0.;
    fEdepWLS.fill(0.);
    fCornerHits.clear();
    fLYSOHits.clear();
    fEdepTrig.fill(0.);
    fTimeTrig.fill(kBigTime);
    fEdepMCP = 0.;
    fTimeMCP = kBigTime;
    fEdepPbGlass = 0.;
    fTphFront.fill(kBigTime); fTphBack.fill(kBigTime);
    fNphFront.fill(0);        fNphBack.fill(0);
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
    // 5. X-Y LATERAL SHOWER PROFILE — integrated + 6 depth slices
    // =========================================================================
    // Mapping: layer index → slice H2 id
    //   Slice 0 → H2[7]  layers  0– 4  (z ≈ −57 to −41 mm)
    //   Slice 1 → H2[8]  layers  5– 9  (z ≈ −41 to −21 mm)
    //   Slice 2 → H2[9]  layers 10–14  (z ≈ −21 to  −1 mm)  ← shower max
    //   Slice 3 → H2[10] layers 15–19  (z ≈  −1 to +19 mm)
    //   Slice 4 → H2[11] layers 20–24  (z ≈ +19 to +39 mm)
    //   Slice 5 → H2[12] layers 25–28  (z ≈ +39 to +57 mm)
    auto depthSliceH2 = [](G4int layer) -> G4int {
        if (layer <=  4) return 7;
        if (layer <=  9) return 8;
        if (layer <= 14) return 9;
        if (layer <= 19) return 10;
        if (layer <= 24) return 11;
        return 12;
    };

    for (const auto& hit : fLYSOHits) {
        // H2[2]: integrated X-Y map (all layers)
        am->FillH2(2, hit.x / mm, hit.y / mm, hit.edep / MeV);
        // H2[7-12]: depth-sliced X-Y map
        G4int sliceId = depthSliceH2((G4int)hit.layer);
        am->FillH2(sliceId, hit.x / mm, hit.y / mm, hit.edep / MeV);
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

        // Geometric z-reconstruction diagnostics (kept from the deposition model)
        am->FillH1(12, zResid / mm);                    // Z residual
        am->FillH2(0,  deltaT / ns, zHit / mm);         // DeltaT vs true z (geometric)
        am->FillH2(1,  zReco  / mm, zHit / mm);         // z_reco vs z_true
    }

    // =========================================================================
    // 6b. OPTICAL-PHOTON TIMING (the real measurement)
    //   Per corner, ΔT = t_back − t_front of the FIRST detected photon at each
    //   end PD (leading-edge). σ_t now comes from genuine photon statistics +
    //   propagation, so it follows a/√E ⊕ b. H1[6] = ΔT, H2[5] = ΔT vs E,
    //   H1[21] = detected photons/event.
    // =========================================================================
    G4int nPhotTot = 0;
    for (G4int c = 0; c < 4; c++) {
        nPhotTot += fNphFront[c] + fNphBack[c];
        if (fTphFront[c] < kBigTime && fTphBack[c] < kBigTime) {
            G4double dT = fTphBack[c] - fTphFront[c];   // back − front (positive)
            am->FillH1(6, dT / ns);                     // optical ΔT
            if (totalLYSO > 0.) am->FillH2(5, totalLYSO / GeV, dT / ns);
        }
    }
    if (nPhotTot > 0) am->FillH1(21, nPhotTot);         // photons detected / event

    // =========================================================================
    // 7. CERN TEST-BEAM LINE OBSERVABLES
    //    Trigger counters, MCP timing reference (t0), Pb-glass tail catcher.
    // =========================================================================
    if (fEdepTrig[0] > 0.) am->FillH1(14, fEdepTrig[0] / MeV);  // trigger 1 dE
    if (fEdepTrig[1] > 0.) am->FillH1(15, fEdepTrig[1] / MeV);  // trigger 2 dE
    if (fEdepMCP    > 0.) am->FillH1(16, fEdepMCP     / MeV);  // MCP radiator dE
    if (fEdepPbGlass > 0.) am->FillH1(17, fEdepPbGlass / GeV);  // Pb-glass energy

    // Beam time-of-flight: trigger 1 -> MCP (both seen by the primary)
    if (fTimeTrig[0] < kBigTime && fTimeMCP < kBigTime)
        am->FillH1(19, (fTimeMCP - fTimeTrig[0]) / ns);

    // Energy-weighted mean WLS arrival time across all 4 corners
    G4double wlsESum = 0., wlsTSum = 0.;
    for (const auto& hit : fCornerHits) { wlsESum += hit.edep; wlsTSum += hit.edep * hit.t; }
    G4double wlsMeanT = (wlsESum > 0.) ? wlsTSum / wlsESum : kBigTime;

    // RADiCAL timing relative to the MCP reference (t0): the key resolution plot
    if (wlsESum > 0. && fTimeMCP < kBigTime) {
        am->FillH1(18, (wlsMeanT - fTimeMCP) / ns);             // H1[18]
        am->FillH2(14, fTimeMCP / ns, wlsMeanT / ns);           // H2[14]
    }

    // Tail-catcher correlation: RADiCAL sampled energy vs Pb-glass leakage energy
    if (totalLYSO > 0.)
        am->FillH2(13, totalLYSO / GeV, fEdepPbGlass / GeV);    // H2[13]

    // Tail-catcher-corrected energy estimator (H1[20]):
    //   E_comb = E_LYSO + f_s * E_PbGlass,  f_s = LYSO sampling fraction.
    // The −0.94 LYSO/PbGlass anti-correlation means adding the (sampling-scaled)
    // forward leakage back cancels the leakage fluctuation, tightening sigma/E.
    static const G4double kSamplingFrac = 0.18;
    G4double eComb = totalLYSO + kSamplingFrac * fEdepPbGlass;

    // Beam-acceptance cut: reject halo events that missed the ±7 mm module and
    // showered straight into the Pb-glass (these form a spurious sharp peak).
    // A real test beam removes these via trigger/tracking. Keep events where the
    // module-reconstructed energy exceeds the tail-catcher energy (>50% in module);
    // this preserves genuine forward-leakage events while cutting clean misses.
    G4double eModuleReco = totalLYSO / kSamplingFrac;
    bool inAcceptance = (eModuleReco > fEdepPbGlass);
    if (eComb > 0. && inAcceptance) am->FillH1(20, eComb / GeV);   // H1[20]
}
