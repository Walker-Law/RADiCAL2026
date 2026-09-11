// RunAction — books the histograms + ntuple and writes the output file.
// Histogram/ntuple ids used across the code:
//   H0 Elyso  : energy deposited in LYSO (GeV)         -> energy resolution
//   H1 Npe    : photons detected at the SiPMs / event  -> light yield
//   H2 dTcfd  : 5% CFD t(down)-t(up), 4-corner mean    -> timing resolution
//   Ntuple 0  : one row per event — see the column list in RunAction.cc.
//               Includes the PERFECT WAVEFORM (phT/phId/phWls: every detected
//               photon, always stored) — the complete pre-electronics light
//               record, from which any estimator is derivable offline.
#ifndef RunAction_h
#define RunAction_h
#include "G4UserRunAction.hh"
class G4Run;
class EventAction;
class RunAction : public G4UserRunAction {
public:
    // eventAction: the same-thread EventAction whose vectors back the ntuple's
    // vector columns. The MASTER RunAction (merging only, never fills) passes
    // nullptr and binds harmless dummies instead.
    explicit RunAction(EventAction* eventAction = nullptr);
    void BeginOfRunAction(const G4Run*) override;
    void EndOfRunAction(const G4Run*) override;
};
#endif
