// RunAction — books the histograms + ntuple and writes the output file.
// Histogram/ntuple ids used across the code:
//   H0 Elyso  : energy deposited in LYSO (GeV)         -> energy resolution
//   H1 Npe    : photons detected at the SiPMs / event  -> light yield
//   H2 dT     : t(down) - t(up), 4-corner mean (ns)    -> timing resolution
//   H3 dTc    : the same, per individual corner (ns)   -> diagnostic
//   Ntuple 0  : (Elyso, Npe, dT) one row per event     -> for scan fits
#ifndef RunAction_h
#define RunAction_h
#include "G4UserRunAction.hh"
class G4Run;
class RunAction : public G4UserRunAction {
public:
    RunAction();
    void BeginOfRunAction(const G4Run*) override;
    void EndOfRunAction(const G4Run*) override;
};
#endif
