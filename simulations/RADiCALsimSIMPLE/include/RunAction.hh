// RunAction — books the histograms + ntuple and writes the output file.
// Histogram/ntuple ids used across the code:
//   H0 Elyso  : energy deposited in LYSO (GeV)         -> energy resolution
//   H1 Npe    : photons detected at the SiPMs / event  -> light yield
//   H2 dT     : t(down) - t(up), 4-corner mean (ns)    -> timing resolution
//   H3 dTc    : the same, per individual corner (ns)   -> diagnostic
//   Ntuple 0  : one row per event — see the column list in RunAction.cc.
//               Scalar columns 0-7 are the ORIGINAL schema; 8-10 and the
//               vector columns are the 2026-07-28 "store enough to never
//               re-run for a new event-level graph" extension.
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
