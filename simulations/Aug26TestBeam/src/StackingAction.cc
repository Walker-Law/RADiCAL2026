#include "StackingAction.hh"
#include "G4Track.hh"
#include "G4OpticalPhoton.hh"
#include "G4VProcess.hh"
#include "Randomize.hh"
#include <cstdlib>

G4ClassificationOfNewTrack
StackingAction::ClassifyNewTrack(const G4Track* track) {
    if (track->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {
        const G4VProcess* cp = track->GetCreatorProcess();
        if (cp && cp->GetProcessName() == "Cerenkov") {
            // same knob as the LYSO yield scale -> coherent thinning
            // (RADSIMPLE_LIGHT_SCALE, old name RADSIMPLE_LYSO_SCALE accepted)
            static G4ThreadLocal G4double scale = -1.;
            if (scale < 0.) {
                const char* s = std::getenv("RADSIMPLE_LIGHT_SCALE");
                if (!s) s = std::getenv("RADSIMPLE_LYSO_SCALE");
                scale = (s && std::atof(s) > 0.) ? std::atof(s) : 1e-2;
            }
            if (G4UniformRand() > scale) return fKill;   // binomial thinning
        }
    }
    return fUrgent;
}
