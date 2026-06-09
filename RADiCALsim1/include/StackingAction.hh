#ifndef StackingAction_h
#define StackingAction_h

#include "G4UserStackingAction.hh"
#include "globals.hh"

// Caps the number of optical photons tracked per event. At 120 GeV a rare
// "monster" shower can dump enough energy into a corner quartz/WLS to spawn
// millions of optical photons, hanging that worker thread. Capping bounds the
// per-event cost while preserving timing: prompt (Cherenkov/early-scint) photons
// are created first and survive the cap; only the late scintillation tail — which
// does not affect the leading-edge front/back ΔT — is discarded.
class StackingAction : public G4UserStackingAction {
public:
    StackingAction() = default;
    ~StackingAction() override = default;

    G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track* track) override;
    void PrepareNewEvent() override;

private:
    G4int fNopt = 0;
    static const G4int kMaxOpt = 500000;   // optical photons / event budget
};

#endif
