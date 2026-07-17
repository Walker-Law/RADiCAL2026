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
    // Photon detection efficiency of the end SiPMs. 0.36 = onsemi MicroFJ-30035
    // datasheet PDE at DSB1's 495 nm emission (Fig 1: 50% peak at 420 nm, ~36%
    // at 495 nm at +6 V overvoltage; ~27% at +2.5 V OV — RADiCAL runs high OV
    // for timing). cat = creation-process category: 0 Cherenkov, 1 fiber
    // self-scint, 2 OpWLS (LYSO light re-emitted by DSB1 — realistic signal
    // path). "Scint" population = cat 1+2 (non-Cherenkov); "WLS" = cat 2.
    static constexpr G4double kQE = 0.36;
    void RecordPhoton(G4int corner, bool isUpstream, G4double t0, G4int cat) {
        if (corner < 0 || corner >= 4) return;
        if (G4UniformRand() > kQE) return;            // apply QE
        // Single-Photon Time Resolution (SPTR): a real SiPM adds ~Gaussian jitter
        // to EACH detected photon's arrival time (avalanche transit-time spread,
        // ~80-150 ps FWHM for HDR2-class devices). Absent, our timing was
        // optimistic; adding it makes the leading edge realistically smeared and
        // contributes ~SPTR/sqrt(N_pe) to sigma_t. RADICAL_SPTR_PS sets the RMS
        // (default 60 ps ~ 140 ps FWHM); 0 disables.
        static G4ThreadLocal G4double sptr = -1.;
        if (sptr < 0.) { const char* s = std::getenv("RADICAL_SPTR_PS");
                         sptr = s ? std::atof(s) : 60.; }
        G4double t = (sptr > 0.) ? t0 + G4RandGauss::shoot(0., sptr * 1.e-3) : t0;  // ps->ns
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
        else            { fNphWlsDown++;   // Fig8: single DOWNSTREAM SiPM readout
                          if (t < fTphDownW[corner]) fTphDownW[corner] = t; }
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
    G4int fNphWlsDown;   // Fig8: downstream-only WLS photons (single-SiPM LY)
};

#endif
