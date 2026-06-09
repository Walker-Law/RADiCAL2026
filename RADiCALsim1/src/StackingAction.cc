#include "StackingAction.hh"
#include "G4Track.hh"
#include "G4OpticalPhoton.hh"

void StackingAction::PrepareNewEvent() { fNopt = 0; }

G4ClassificationOfNewTrack
StackingAction::ClassifyNewTrack(const G4Track* track) {
    if (track->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {
        if (++fNopt > kMaxOpt) return fKill;   // drop photons beyond the budget
    }
    return fUrgent;
}
