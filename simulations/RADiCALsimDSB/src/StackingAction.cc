#include "StackingAction.hh"
#include "G4Track.hh"
#include "G4OpticalPhoton.hh"
#include "G4VProcess.hh"
#include "G4LogicalVolume.hh"
#include <cstdlib>

// Cherenkov photons born in LYSO are killed at stacking. In the real crystal
// scintillation (33200 ph/MeV) outnumbers Cherenkov ~1000:1, but the sim scales
// the scint yield down (RADICAL_LYSO_SCINT_SCALE) while Cherenkov generates at
// the full physical rate — keeping it would invert the real scint:Cherenkov
// ratio AND spend most of the event tracking photons that carry ~0.1% of the
// real signal. Set RADICAL_KEEP_LYSO_CHER=1 to keep them.
static bool keepLysoCher() {
    static bool v = (std::getenv("RADICAL_KEEP_LYSO_CHER") != nullptr);
    return v;
}

void StackingAction::PrepareNewEvent() { fNopt = 0; }

G4ClassificationOfNewTrack
StackingAction::ClassifyNewTrack(const G4Track* track) {
    if (track->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {
        if (++fNopt > kMaxOpt) return fKill;   // drop photons beyond the budget
        if (!keepLysoCher()) {
            const G4VProcess* cp = track->GetCreatorProcess();
            if (cp && cp->GetProcessName() == "Cerenkov") {
                const G4LogicalVolume* lv = track->GetLogicalVolumeAtVertex();
                if (lv && lv->GetName() == "LYSO") return fKill;
            }
        }
    }
    return fUrgent;
}
