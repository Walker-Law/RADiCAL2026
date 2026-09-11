// RunAction — books the histograms + ntuple and writes the output file.
// Histogram ids:
//   H0 Elyso    : energy deposited in LYSO (GeV)               -> energy (truth)
//   H1 Npe      : photons detected, 4 upstream corners / event -> light yield
//   H2 dTpair   : diagonal-pair 5%-quantile difference (ns)    -> timing, realizable
//   H3 tUpMean  : absolute 4-corner mean 5%-quantile time (ns) -> timing vs a perfect reference
//   Ntuple "ev" : one row per event — column list in RunAction.cc. Includes the
//                 PERFECT WAVEFORM (phT/phId/phOrigin: every detected photon).
#ifndef RunAction_h
#define RunAction_h
#include "G4UserRunAction.hh"
class G4Run;
class EventAction;
class RunAction : public G4UserRunAction {
public:
    // eventAction: the same-thread EventAction whose vectors back the ntuple's
    // vector columns. The MASTER RunAction (merging only) passes nullptr.
    explicit RunAction(EventAction* eventAction = nullptr);
    void BeginOfRunAction(const G4Run*) override;
    void EndOfRunAction(const G4Run*) override;
};
#endif
