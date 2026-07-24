#include "PrimaryGeneratorAction.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4Event.hh"
#include "G4SystemOfUnits.hh"

// A single electron per event, fired straight down +z into the front face of
// the stack. Beam energy is set in the macro with  /gun/energy 50 GeV.
// The stack front is at z = -stackZ/2 = -62.44 mm (see DetectorConstruction),
// so we start the electron just upstream of it.
PrimaryGeneratorAction::PrimaryGeneratorAction() {
    fGun = new G4ParticleGun(1);
    fGun->SetParticleDefinition(
        G4ParticleTable::GetParticleTable()->FindParticle("e-"));
    fGun->SetParticleMomentumDirection(G4ThreeVector(0., 0., 1.));
    fGun->SetParticleEnergy(50.*GeV);                 // default; override in macro
    fGun->SetParticlePosition(G4ThreeVector(0., 0., -70.*mm));
}

PrimaryGeneratorAction::~PrimaryGeneratorAction() { delete fGun; }

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* evt) {
    fGun->GeneratePrimaryVertex(evt);
}
