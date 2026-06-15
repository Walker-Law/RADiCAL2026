#include "SteppingAction.hh"
#include "RunAction.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4VPhysicalVolume.hh"
#include "G4StepPoint.hh"
#include "G4VProcess.hh"

SteppingAction::SteppingAction(RunAction* runAction) : fRunAction(runAction) {}

void SteppingAction::UserSteppingAction(const G4Step* step) {
    auto* preVol  = step->GetPreStepPoint()->GetPhysicalVolume();
    auto* postVol = step->GetPostStepPoint()->GetPhysicalVolume();
    if (!preVol || !postVol) return;

    G4String preName  = preVol->GetLogicalVolume()->GetName();
    G4String postName = postVol->GetLogicalVolume()->GetName();

    // Count particles entering each layer (boundary crossing into that layer)
    if (preName != postName) {
        fRunAction->AddParticleEntering(postName);
    }

    // Count tritons (H-3 nuclei) created as secondaries in this step
    const std::vector<const G4Track*>* secondaries = step->GetSecondaryInCurrentStep();
    for (const G4Track* sec : *secondaries) {
        if (sec->GetParticleDefinition()->GetParticleName() == "triton") {
            fRunAction->AddTriton(preName);  // produced in pre-step volume
        }
    }
}
