// EventAction — per-event bookkeeping. SteppingAction calls the Add*/Record*
// methods during the event; EndOfEventAction() turns them into the timing /
// energy observables and fills the histograms + ntuple.
#ifndef EventAction_h
#define EventAction_h
#include "G4UserEventAction.hh"
#include "globals.hh"
#include <array>
class RunAction;

class EventAction : public G4UserEventAction {
public:
    explicit EventAction(RunAction*) {}
    void BeginOfEventAction(const G4Event*) override;
    void EndOfEventAction(const G4Event*) override;

    void AddLYSO(G4double edep) { fElyso += edep; }

    // Record one detected photon, upstream/downstream end, global time t (ns).
    // channel 0-3 = the corner TIMING fibres (kept in the first-photon arrays
    // and in the 4-corner dT average); channel 4 = the OPTIONAL central E-type
    // ENERGY capillary — counted separately (NpeCenter) and NEVER mixed into
    // the timing average.
    void RecordPhoton(G4int channel, bool isUpstream, G4double t) {
        if (channel < 0 || channel > 4) return;
        if (channel == 4) { ++fNpeCenter; return; }
        ++fNpe;
        auto& first = isUpstream ? fTup[channel] : fTdn[channel];
        if (t < first) first = t;
    }

    // ---- beam-line detectors (no electronics: pure truth quantities) ----
    void RecordMCP(G4double t) { if (t < fTmcp) fTmcp = t; }   // earliest hit = t0
    void AddTrig(G4int i, G4double e) { if (i==0||i==1) fEtrig[i] += e; }
    void AddPbGlass(G4double e) { fEpbGlass += e; }

private:
    static constexpr G4double kBig = 1e9;   // "nothing recorded" sentinel (ns)
    G4double fElyso     = 0.;
    G4double fNpe       = 0.;               // corner-fibre photons (8 SiPMs)
    G4double fNpeCenter = 0.;               // central E-type photons (2 SiPMs)
    G4double fTmcp      = kBig;             // MCP particle-arrival time (ns)
    std::array<G4double,2> fEtrig{};        // trigger-counter energy deposits
    G4double fEpbGlass  = 0.;               // Pb-glass (tail catcher) deposit
    std::array<G4double,4> fTup{}, fTdn{};
};
#endif
