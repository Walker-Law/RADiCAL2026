#include "SteppingAction.hh"
#include "RunAction.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VProcess.hh"
#include "G4SystemOfUnits.hh"

SteppingAction::SteppingAction(RunAction* runAction) : fRunAction(runAction) {}

void SteppingAction::UserSteppingAction(const G4Step* step) {
    auto* preVol  = step->GetPreStepPoint()->GetPhysicalVolume();
    auto* postVol = step->GetPostStepPoint()->GetPhysicalVolume();
    if (!preVol || !postVol) return;

    G4String preName  = preVol->GetLogicalVolume()->GetName();
    G4String postName = postVol->GetLogicalVolume()->GetName();

    const G4Track* track = step->GetTrack();
    G4String pname = track->GetParticleDefinition()->GetParticleName();

    // Neutron entering a new layer — record KE at entry point
    if (pname == "neutron" && preName != postName) {
        G4double ke = step->GetPostStepPoint()->GetKineticEnergy();
        fRunAction->AddParticleEntering(postName, ke);
    }

    // Count neutron hadronic interactions (any process that isn't pure transport)
    if (pname == "neutron") {
        const G4VProcess* proc = step->GetPostStepPoint()->GetProcessDefinedStep();
        if (proc) {
            G4String procName = proc->GetProcessName();
            if (procName != "Transportation" &&
                procName != "CoulombScat"    &&
                procName != "nKiller") {
                fRunAction->AddNeutronInteraction(preName);
            }
        }
    }

    // Triton secondaries produced in this step
    for (const G4Track* sec : *step->GetSecondaryInCurrentStep()) {
        if (sec->GetParticleDefinition()->GetParticleName() == "triton") {
            G4double ke = sec->GetKineticEnergy();
            G4double r  = sec->GetPosition().mag();
            fRunAction->AddTriton(preName, ke, r);
        }
    }
}
