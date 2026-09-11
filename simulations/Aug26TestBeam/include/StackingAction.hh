// StackingAction — coherent light thinning (ALL light × RADSIMPLE_LYSO_SCALE).
//
// LYSO scintillation is thinned at the SOURCE (yield × RADSIMPLE_LYSO_SCALE in
// DetectorConstruction). Cherenkov has no yield knob, so it is thinned HERE:
// each newly created Cherenkov photon is kept with that same probability.
// WLS re-emission is NOT thinned again — it inherits its parent's thinning.
//
// Net effect: every light source is reduced by the same factor, so the
// scint : Cherenkov MIX stays physical at any thinning. Without this,
// Cherenkov ran at full rate while scint was 100x thinned — the inverted-ratio
// trap (prompt Cherenkov wins the first-photon race) learned the hard way in
// RADiCALsimDSB.
#ifndef StackingAction_h
#define StackingAction_h
#include "G4UserStackingAction.hh"

class StackingAction : public G4UserStackingAction {
public:
    G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track*) override;
};
#endif
