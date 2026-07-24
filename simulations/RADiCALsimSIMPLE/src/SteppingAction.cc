#include "SteppingAction.hh"
#include "EventAction.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4OpticalPhoton.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"
#include <cstdlib>

// SiPM photon detection efficiency (probability a photon reaching the sensor is
// counted). 0.36 = onsemi MicroFJ at DSB1's 495 nm green. Override with RADSIMPLE_PDE.
static G4double PDE() {
    static G4double v = (std::getenv("RADSIMPLE_PDE")
                         ? std::atof(std::getenv("RADSIMPLE_PDE")) : 0.36);
    return v;
}

void SteppingAction::UserSteppingAction(const G4Step* step) {
    auto track = step->GetTrack();

    // ---- optical photon: has it just arrived at a SiPM? ----
    if (track->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {
        auto post = step->GetPostStepPoint()->GetTouchableHandle()->GetVolume();
        if (post) {
            const G4String& n = post->GetLogicalVolume()->GetName();
            if (n == "PD_Up" || n == "PD_Down") {
                if (G4UniformRand() <= PDE()) {                 // detect with prob PDE
                    const G4int corner = post->GetCopyNo();     // 0..3
                    const G4double t = step->GetPostStepPoint()->GetGlobalTime()/ns;
                    fEvt->RecordPhoton(corner, n == "PD_Up", t);
                }
                track->SetTrackStatus(fStopAndKill);            // absorbed by the sensor
            }
        }
        return;                                                // photons deposit no ionisation
    }

    // ---- charged/neutral: sum energy deposited in LYSO ----
    auto pre = step->GetPreStepPoint()->GetTouchableHandle()->GetVolume();
    if (pre && pre->GetLogicalVolume()->GetName() == "LYSO") {
        const G4double e = step->GetTotalEnergyDeposit();
        if (e > 0.) fEvt->AddLYSO(e);
    }
}
