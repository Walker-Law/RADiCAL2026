#include "SteppingAction.hh"
#include "RunAction.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4VPhysicalVolume.hh"
#include "G4SystemOfUnits.hh"

SteppingAction::SteppingAction(RunAction* runAction) : fRunAction(runAction) {}

void SteppingAction::UserSteppingAction(const G4Step* step) {
    auto* preVol  = step->GetPreStepPoint()->GetPhysicalVolume();
    auto* postVol = step->GetPostStepPoint()->GetPhysicalVolume();
    if (!preVol || !postVol) return;

    G4String preName  = preVol->GetLogicalVolume()->GetName();
    G4String postName = postVol->GetLogicalVolume()->GetName();

    // Neutron crossing into a layer — record KE at entry
    if (preName != postName &&
        step->GetTrack()->GetParticleDefinition()->GetParticleName() == "neutron") {
        G4double ke = step->GetPostStepPoint()->GetKineticEnergy();
        fRunAction->AddParticleEntering(postName, ke);
    }

    // Triton secondaries produced in this step
    const std::vector<const G4Track*>* secondaries = step->GetSecondaryInCurrentStep();
    for (const G4Track* sec : *secondaries) {
        if (sec->GetParticleDefinition()->GetParticleName() == "triton") {
            G4double ke  = sec->GetKineticEnergy();
            G4double r   = sec->GetPosition().mag();
            fRunAction->AddTriton(preName, ke, r);
        }
    }
}
