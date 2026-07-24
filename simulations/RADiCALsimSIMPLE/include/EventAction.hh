// EventAction — per-event bookkeeping. SteppingAction calls AddLYSO() and
// RecordPhoton() during the event; EndOfEventAction() turns them into the
// timing / energy observables and fills the histograms.
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

    // Record one detected photon at corner (0-3), upstream/downstream end,
    // with global arrival time t (ns). We keep only the EARLIEST time per end
    // (that is the "first-photon" timing) and count the total.
    void RecordPhoton(G4int corner, bool isUpstream, G4double t) {
        if (corner < 0 || corner > 3) return;
        ++fNpe;
        auto& first = isUpstream ? fTup[corner] : fTdn[corner];
        if (t < first) first = t;
    }

private:
    static constexpr G4double kBig = 1e9;   // "no photon yet" sentinel (ns)
    G4double fElyso = 0.;
    G4double fNpe   = 0.;
    std::array<G4double,4> fTup{}, fTdn{};
};
#endif
