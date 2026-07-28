#include "PrimaryGeneratorAction.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4Event.hh"
#include "G4SystemOfUnits.hh"

// A single electron per event, fired straight down +z. Beam energy is set in
// the macro with  /gun/energy 50 GeV. The electron starts at z = -450 mm,
// UPSTREAM of the whole test-beam line (triggers at -400/-350 mm, MCP at
// -250 mm), so it traverses every beam-line element before hitting the stack
// (front face at -62.44 mm) — exactly like the real H2 beam.
PrimaryGeneratorAction::PrimaryGeneratorAction() {
    fGun = new G4ParticleGun(1);
    fGun->SetParticleDefinition(
        G4ParticleTable::GetParticleTable()->FindParticle("e-"));
    fGun->SetParticleMomentumDirection(G4ThreeVector(0., 0., 1.));
    fGun->SetParticleEnergy(50.*GeV);                 // default; override in macro
    fGun->SetParticlePosition(G4ThreeVector(0., 0., -450.*mm));
}

PrimaryGeneratorAction::~PrimaryGeneratorAction() { delete fGun; }

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* evt) {
    fGun->GeneratePrimaryVertex(evt);
}
