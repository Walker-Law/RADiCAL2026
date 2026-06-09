#ifndef EventAction_h
#define EventAction_h

#include "G4UserEventAction.hh"
#include "globals.hh"
#include "Randomize.hh"
#include <vector>
#include <array>

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
    // Quantum efficiency of the end photodetectors (bialkali/SiPM-like).
    static constexpr G4double kQE = 0.20;
    void RecordPhoton(G4int corner, bool isUpstream, G4double t) {
        if (corner < 0 || corner >= 4) return;
        if (G4UniformRand() > kQE) return;            // apply QE
        if (isUpstream) { fNphUp[corner]++;   if (t < fTphUp[corner])   fTphUp[corner]   = t;
                          if (fPhTUp[corner].size()   < kMaxStore) fPhTUp[corner].push_back(t); }
        else            { fNphDown[corner]++; if (t < fTphDown[corner]) fTphDown[corner] = t;
                          if (fPhTDown[corner].size() < kMaxStore) fPhTDown[corner].push_back(t); }
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
};

#endif
