#ifndef EventAction_h
#define EventAction_h

#include "G4UserEventAction.hh"
#include "globals.hh"
#include "Randomize.hh"
#include <vector>
#include <array>
#include <cstdlib>   // std::getenv / std::atof for the SPTR knob

class EventAction : public G4UserEventAction {
public:
    EventAction();
    ~EventAction() override;

    void BeginOfEventAction(const G4Event*) override;
    void EndOfEventAction(const G4Event*) override;

    void AddEdepLYSO(G4int layer, G4double edep, G4double x, G4double y) {
        if (layer >= 0 && layer < 29) {
            fEdepLYSO[layer] += edep;
            fLYSOHits.push_back({x, y, (G4double)layer, edep});
        }
    }
    void AddEdepW(G4int layer, G4double edep) {
        if (layer >= 0 && layer < 28) fEdepW[layer] += edep;
    }
    void AddCenterCapEdep(G4double edep) { fEdepCenter += edep; }
    void RecordCornerWLS(G4int corner, G4double edep, G4double z, G4double t) {
        if (corner >= 0 && corner < 4) {
            fEdepWLS[corner] += edep;
            if (edep > 0.) {
                fCornerHits.push_back({edep, z, t, (double)corner});
            }
        }
    }

    // ── CERN test-beam line detectors ──────────────────────────────────────
    void RecordTrig(G4int i, G4double edep, G4double t) {
        if (i >= 0 && i < 2) {
            fEdepTrig[i] += edep;
            if (t < fTimeTrig[i]) fTimeTrig[i] = t;   // earliest hit = arrival time
        }
    }
    void RecordMCP(G4double edep, G4double t) {
        fEdepMCP += edep;
        if (t < fTimeMCP) fTimeMCP = t;               // earliest hit = t0 reference
    }
    void AddPbGlassEdep(G4double edep) { fEdepPbGlass += edep; }

    // ── Optical-photon timing readout ───────────────────────────────────────
    // Photon detection efficiency of the end SiPMs: onsemi MicroFJ-30035
    // PDE(λ) at +6 V overvoltage (RADiCAL runs high OV for timing), digitized
    // from the datasheet PDE-vs-wavelength figure and anchored at the two
    // values previously used here: 50% peak at 420 nm, 36% at DSB1's 495 nm
    // emission. Wavelength-dependent (2026-07-22, replacing flat kQE=0.36): a
    // real SiPM detects whatever light arrives per its PDE(λ) — no flat number
    // is right for both the 495 nm WLS light and the blue-weighted (1/λ²)
    // Cherenkov. Linear interpolation, clamped outside 300–800 nm.
    // RADICAL_SIPM_QE=<v> overrides with a flat value (0.36 reproduces all
    // runs before 2026-07-22).
    // cat = creation-process category: 0 Cherenkov, 1 fiber self-scint,
    // 2 OpWLS (LYSO light re-emitted by DSB1 — realistic signal path).
    // "Scint" population = cat 1+2 (non-Cherenkov); "WLS" = cat 2.
    static G4double sipmPDE(G4double phEeV) {
        static G4ThreadLocal G4double flat = -2.;
        if (flat < -1.) { const char* q = std::getenv("RADICAL_SIPM_QE");
                          flat = q ? std::atof(q) : -1.; }
        if (flat >= 0.) return flat;
        static const G4double lam[] = {300, 320, 350, 380, 400, 420, 450, 470,
                                       495, 520, 550, 600, 650, 700, 750, 800};
        static const G4double pde[] = {0.14, 0.22, 0.33, 0.44, 0.48, 0.50, 0.46, 0.42,
                                       0.36, 0.32, 0.27, 0.20, 0.15, 0.11, 0.08, 0.05};
        constexpr G4int n = 16;
        const G4double l = 1239.84193 / phEeV;        // eV -> nm
        if (l <= lam[0])     return pde[0];
        if (l >= lam[n - 1]) return pde[n - 1];
        G4int i = 1; while (lam[i] < l) ++i;
        const G4double f = (l - lam[i - 1]) / (lam[i] - lam[i - 1]);
        return pde[i - 1] + f * (pde[i] - pde[i - 1]);
    }
    void RecordPhoton(G4int corner, bool isUpstream, G4double t0, G4int cat,
                      G4double phEeV = 2.505 /* 495 nm if caller omits */) {
        if (corner < 0 || corner >= 4) return;
        if (G4UniformRand() > sipmPDE(phEeV)) return; // apply PDE(lambda)
        // ── NoElec variant: SiPM single-photon time resolution (SPTR) REMOVED ──
        // The DSB sim adds ~Gaussian per-photon jitter here (avalanche transit-
        // time spread). This electronics-free variant uses an IDEAL, jitter-free
        // sensor: the recorded time is the pure photon arrival time. (RADICAL_SPTR_PS
        // is intentionally ignored here.)
        const G4double t = t0;
        if (isUpstream) { fNphUp[corner]++;   if (t < fTphUp[corner])   fTphUp[corner]   = t;
                          if (fPhTUp[corner].size()   < kMaxStore) fPhTUp[corner].push_back(t); }
        else            { fNphDown[corner]++; if (t < fTphDown[corner]) fTphDown[corner] = t;
                          if (fPhTDown[corner].size() < kMaxStore) fPhTDown[corner].push_back(t); }
        if (cat == 0) { fNphCher++; return; }
        fNphScint++;
        if (isUpstream) { if (t < fTphUpS[corner])   fTphUpS[corner]   = t;
                          if (fPhTUpS[corner].size()   < kMaxStore) fPhTUpS[corner].push_back(t); }
        else            { if (t < fTphDownS[corner]) fTphDownS[corner] = t;
                          if (fPhTDownS[corner].size() < kMaxStore) fPhTDownS[corner].push_back(t); }
        if (cat != 2) return;
        fNphWls++;
        if (isUpstream) { if (t < fTphUpW[corner])   fTphUpW[corner]   = t; }
        else            { if (t < fTphDownW[corner]) fTphDownW[corner] = t; }
    }

private:
    std::array<G4double, 29> fEdepLYSO;
    std::array<G4double, 28> fEdepW;
    G4double                 fEdepCenter;
    std::array<G4double, 4>  fEdepWLS;

    struct CornerHit { G4double edep, z, t, corner; };
    std::vector<CornerHit> fCornerHits;

    struct LYSOHit { G4double x, y, layer, edep; };
    std::vector<LYSOHit> fLYSOHits;

    // Beam-line detectors
    std::array<G4double, 2> fEdepTrig;
    std::array<G4double, 2> fTimeTrig;
    G4double                fEdepMCP;
    G4double                fTimeMCP;
    G4double                fEdepPbGlass;

    // Optical photon readout: earliest detected photon time + count, per corner
    // (upstream = −z end PD, downstream = +z end PD)
    std::array<G4double, 4> fTphUp, fTphDown;
    std::array<G4int, 4>    fNphUp, fNphDown;

    // Full arrival-time lists for waveform emulation (DRS4-style CFD timing,
    // mirroring the CERN test-beam analysis). Capped to bound memory.
    static constexpr size_t kMaxStore = 60000;
    std::array<std::vector<G4double>, 4> fPhTUp, fPhTDown;

    // Scintillation-origin-only parallels (Cherenkov excluded) + process counts
    std::array<G4double, 4> fTphUpS, fTphDownS;
    std::array<std::vector<G4double>, 4> fPhTUpS, fPhTDownS;
    G4int fNphScint, fNphCher;

    // OpWLS-only (LYSO->DSB1 re-emission, the realistic chain): first times + count
    std::array<G4double, 4> fTphUpW, fTphDownW;
    G4int fNphWls;
};

#endif
