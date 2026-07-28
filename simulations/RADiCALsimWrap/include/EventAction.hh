// EventAction — per-event bookkeeping. SteppingAction calls the Add*/Record*
// methods during the event; EndOfEventAction() turns them into the timing /
// energy observables and fills the histograms + ntuple.
#ifndef EventAction_h
#define EventAction_h
#include "G4UserEventAction.hh"
#include "globals.hh"
#include <array>
#include <vector>
class RunAction;

class EventAction : public G4UserEventAction {
public:
    explicit EventAction(RunAction*) {}
    void BeginOfEventAction(const G4Event*) override;
    void EndOfEventAction(const G4Event*) override;

    // edep in a LYSO plate; layer = 0..28 (plate copy number / 2).
    void AddLYSO(G4double edep, G4int layer) {
        fElyso += edep;
        if (layer >= 0 && layer < kNLayers) fLayerEacc[layer] += edep;
    }
    void AddW(G4double edep) { fEw += edep; }

    // Record one detected photon, upstream/downstream end, global time t (ns).
    // channel 0-3 = the corner TIMING fibres (kept in the first-photon arrays
    // and in the 4-corner dT average); channel 4 = the OPTIONAL central E-type
    // ENERGY capillary — counted separately (NpeCenter) and NEVER mixed into
    // the timing average.
    void RecordPhoton(G4int channel, bool isUpstream, G4double t) {
        if (channel < 0 || channel > 4) return;
        if (channel == 4) { ++fNpeCenter; return; }
        ++fNpe;
        ++fCornerNpeAcc[channel];
        auto& first = isUpstream ? fTup[channel] : fTdn[channel];
        if (t < first) first = t;
        if (fStorePhotons) {
            fPhT.push_back(t);
            fPhId.push_back(channel + (isUpstream ? 0 : 4));  // 0-3 up, 4-7 down
        }
    }

    // ---- beam-line detectors (no electronics: pure truth quantities) ----
    void RecordMCP(G4double t) { if (t < fTmcp) fTmcp = t; }   // earliest hit = t0
    void AddTrig(G4int i, G4double e) { if (i==0||i==1) fEtrig[i] += e; }
    void AddPbGlass(G4double e) { fEpbGlass += e; }

    // ---- ntuple vector columns -------------------------------------------
    // RunAction binds these by REFERENCE at ntuple creation (G4AnalysisManager
    // vector columns); EndOfEventAction fills them just before AddNtupleRow.
    // Public because the binding needs the actual objects, not copies.
    std::vector<G4double> fLayerE;      // GeV per LYSO layer, size 29
    std::vector<G4double> fCornerNpe;   // detected photons per corner, size 4
    std::vector<G4double> fCornerTup;   // first-photon time, up end (ns; -999 = none)
    std::vector<G4double> fCornerTdn;   // first-photon time, down end
    std::vector<G4double> fPhT;         // ALL photon times (only if fStorePhotons)
    std::vector<G4double> fPhId;        // matching channel ids: corner + 4*isDown

    static constexpr G4int kNLayers = 29;

private:
    static constexpr G4double kBig = 1e9;   // "nothing recorded" sentinel (ns)
    G4double fElyso     = 0.;
    G4double fEw        = 0.;               // tungsten (absorber) deposit
    G4double fNpe       = 0.;               // corner-fibre photons (8 SiPMs)
    G4double fNpeCenter = 0.;               // central E-type photons (2 SiPMs)
    G4double fTmcp      = kBig;             // MCP particle-arrival time (ns)
    std::array<G4double,2> fEtrig{};        // trigger-counter energy deposits
    G4double fEpbGlass  = 0.;               // Pb-glass (tail catcher) deposit
    std::array<G4double,4> fTup{}, fTdn{};
    std::array<G4double,kNLayers> fLayerEacc{};
    std::array<G4double,4> fCornerNpeAcc{};
    // Full per-photon dump (RADSIMPLE_STORE_PHOTON_TIMES=1). OFF by default:
    // it is the one payload that meaningfully grows the files (~100x), and it
    // is only needed to re-derive timing with a DIFFERENT estimator offline.
    bool fStorePhotons = false;
    friend class RunAction;   // reads fStorePhotons at ntuple creation
public:
    void SetStorePhotons(bool v) { fStorePhotons = v; }
};
#endif
