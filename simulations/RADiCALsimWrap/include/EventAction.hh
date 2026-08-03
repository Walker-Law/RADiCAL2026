// EventAction — per-event bookkeeping. SteppingAction calls the Add*/Record*
// methods during the event; EndOfEventAction() turns them into observables and
// fills the histograms + ntuple.
//
// DESIGN (2026-08-02): this simulation is a LIGHT RECORDER. It stores the
// complete photon arrival record at every SiPM face — the "perfect waveform",
// the last thing that exists before electronics — and computes ONE in-sim
// trigger from it: an electronics-free 5% CFD (below). Anything else you might
// want (first-photon, Nth-photon, other thresholds, SPTR smearing, pulse
// shapes) is derivable OFFLINE from the stored photons, so estimator choices
// are analysis decisions, not simulation reruns.
//
// THE TIMING TRIGGER: "5% CFD", electronics-free version. With no amplifier
// there is no pulse shape to take 5%-of-peak on; the faithful light-only
// analog is a QUANTILE: t05 = the arrival time of the ceil(0.05*N)-th photon
// at that SiPM (5% of the event's total collected light on that end). Then
// dTcfd = t05(down) - t05(up), averaged over the 4 corners, sigma_t =
// sigma(dTcfd)/2 as usual.
//
// WHY NOT FIRST-PHOTON (removed 2026-08-02, was `dTwls`/`dT`): a first-photon
// estimator is a MINIMUM — an order statistic. Two problems, both measured:
// (1) at thinned light the all-light minimum is set by a ~1% prompt-Cherenkov
// population arriving ~10 ns early, whose per-SiPM presence is a coin flip ->
// a multi-modal 5-spike comb (see README "The light chain"); (2) even the
// WLS-only version scales as f^-1.4 (not 1/sqrt(N)) — an extreme-value
// statistic no real device reproduces. The 5% quantile rides on ~N/20 photons,
// scales mean-like, is single-moded at any light level, and is the direct
// light-level analog of the test-beam analysis's 5% CFD convention.
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

    // Record one detected photon: channel 0-3 = corner fibres, 4 = the
    // optional central E-type; isUpstream picks the end; t in ns; isWLS = the
    // photon was created by OpWLS (LYSO light re-emitted by DSB1) as opposed
    // to prompt Cherenkov born in the fibre itself. EVERY detected photon is
    // stored — this is the perfect waveform.
    void RecordPhoton(G4int channel, bool isUpstream, G4double t, bool isWLS) {
        if (channel < 0 || channel > 4) return;
        const int end = ChanEnd(channel, isUpstream);
        fT[end].push_back(t);
        fPhT.push_back(t);
        fPhId.push_back(end);
        fPhWls.push_back(isWLS ? 1. : 0.);
        if (channel == 4) { ++fNpeCenter; return; }
        ++fNpe;
        ++fCornerNpeAcc[channel];
        if (isWLS) ++fNpeWLS;
    }

    // Channel-end index used everywhere (ntuple phId included):
    //   0-3 corner up, 4-7 corner down, 8 center up, 9 center down.
    static int ChanEnd(G4int channel, bool isUpstream) {
        return (channel == 4) ? (isUpstream ? 8 : 9)
                              : channel + (isUpstream ? 0 : 4);
    }

    // ---- beam-line detectors (no electronics: pure truth quantities) ----
    void RecordMCP(G4double t) { if (t < fTmcp) fTmcp = t; }   // earliest hit = t0
    void AddTrig(G4int i, G4double e) { if (i==0||i==1) fEtrig[i] += e; }
    void AddPbGlass(G4double e) { fEpbGlass += e; }

    // ---- ntuple vector columns -------------------------------------------
    // RunAction binds these by REFERENCE at ntuple creation; EndOfEventAction
    // fills them just before AddNtupleRow. Public because the binding needs
    // the actual objects.
    std::vector<G4double> fLayerE;      // GeV per LYSO layer, size 29
    std::vector<G4double> fCornerNpe;   // detected photons per corner, size 4
    std::vector<G4double> fT05Up;       // 5% CFD time per corner, up end (ns; -999 = no light)
    std::vector<G4double> fT05Dn;       // same, down end
    std::vector<G4double> fPhT;         // EVERY detected photon's time (ns) — the waveform
    std::vector<G4double> fPhId;        // its channel-end (see ChanEnd)
    std::vector<G4double> fPhWls;       // 1 = WLS-created, 0 = prompt

    static constexpr G4int    kNLayers = 29;
    static constexpr G4double kCfdFrac = 0.05;   // the test-beam convention

private:
    static constexpr G4double kBig = 1e9;   // "nothing recorded" sentinel (ns)
    std::array<std::vector<G4double>,10> fT;   // per channel-end arrival times
    G4double fElyso     = 0.;
    G4double fEw        = 0.;               // tungsten (absorber) deposit
    G4double fNpe       = 0.;               // corner-fibre photons (8 SiPMs)
    G4double fNpeWLS    = 0.;               // of those, the OpWLS-created ones
    G4double fNpeCenter = 0.;               // central E-type photons (2 SiPMs)
    G4double fTmcp      = kBig;             // MCP particle-arrival time (ns)
    std::array<G4double,2> fEtrig{};        // trigger-counter energy deposits
    G4double fEpbGlass  = 0.;               // Pb-glass (tail catcher) deposit
    std::array<G4double,kNLayers> fLayerEacc{};
    std::array<G4double,4> fCornerNpeAcc{};
};
#endif
