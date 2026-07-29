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
    //
    // isWLS = the photon was created by OpWLS, i.e. it is LYSO light absorbed
    // and re-emitted by the DSB1 shifter.
    //
    // WHY THE SPLIT EXISTS (measured 2026-07-29, 10 GeV, LIGHT_SCALE=1e-2):
    // the detected light is 99% WLS / 1% PROMPT (Cherenkov born in the
    // quartz/DSB1 as shower particles cross it). Tiny as it is, the prompt
    // population arrives ~10 ns EARLIER on average (prompt <t> = 2.0 ns vs
    // WLS <t> = 12.3 ns, because WLS light inherits LYSO's 36 ns emission
    // before DSB1's 3.5 ns re-emission). So a FIRST-photon estimator is set by
    // the 1% whenever a SiPM happens to catch one.
    //
    // At 1e-2 thinning that is ~0.9 prompt photons per SiPM per event — a
    // Poisson coin flip. Corners that catch one read ~3.7 ns early; corners
    // that do not read late. The 4-corner mean of a two-valued variable then
    // produces 5 discrete spikes 3.7/4 = 0.93 ns apart, which is exactly the
    // comb that made the Gaussian sigma_t fits meaningless.
    //
    // This is a THINNING ARTIFACT, not detector physics: at full light every
    // SiPM would catch prompt photons and the estimator would be unimodal
    // again. Tracking the WLS population separately restores a unimodal,
    // fittable observable at any thinning — and it is also the better proxy
    // for the real device, whose CFD fires on the leading edge of the WLS
    // bulk, not on a 1% prompt precursor. (Same conclusion RADiCALsimDSB
    // reached with its process-tagged "scint-only" estimator.)
    void RecordPhoton(G4int channel, bool isUpstream, G4double t, bool isWLS) {
        if (channel < 0 || channel > 4) return;
        if (channel == 4) { ++fNpeCenter; return; }
        ++fNpe;
        ++fCornerNpeAcc[channel];
        auto& first = isUpstream ? fTup[channel] : fTdn[channel];
        if (t < first) first = t;
        if (isWLS) {
            ++fNpeWLS;
            auto& fw = isUpstream ? fTupW[channel] : fTdnW[channel];
            if (t < fw) fw = t;
        }
        if (fStorePhotons) {
            fPhT.push_back(t);
            fPhId.push_back(channel + (isUpstream ? 0 : 4));  // 0-3 up, 4-7 down
            fPhWls.push_back(isWLS ? 1. : 0.);
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
    std::vector<G4double> fCornerTupW;  // first WLS photon, up end (ns; -999 = none)
    std::vector<G4double> fCornerTdnW;  // first WLS photon, down end
    std::vector<G4double> fPhT;         // ALL photon times (only if fStorePhotons)
    std::vector<G4double> fPhId;        // matching channel ids: corner + 4*isDown
    std::vector<G4double> fPhWls;       // 1 = that photon was WLS, 0 = prompt

    static constexpr G4int kNLayers = 29;

private:
    static constexpr G4double kBig = 1e9;   // "nothing recorded" sentinel (ns)
    G4double fElyso     = 0.;
    G4double fEw        = 0.;               // tungsten (absorber) deposit
    G4double fNpe       = 0.;               // corner-fibre photons (8 SiPMs)
    G4double fNpeWLS    = 0.;               // of those, the OpWLS-created ones
    G4double fNpeCenter = 0.;               // central E-type photons (2 SiPMs)
    G4double fTmcp      = kBig;             // MCP particle-arrival time (ns)
    std::array<G4double,2> fEtrig{};        // trigger-counter energy deposits
    G4double fEpbGlass  = 0.;               // Pb-glass (tail catcher) deposit
    std::array<G4double,4> fTup{},  fTdn{};    // first photon, ANY population
    std::array<G4double,4> fTupW{}, fTdnW{};   // first WLS photon only
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
