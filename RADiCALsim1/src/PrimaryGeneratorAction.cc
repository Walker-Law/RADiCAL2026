#include "PrimaryGeneratorAction.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"

PrimaryGeneratorAction::PrimaryGeneratorAction() {
    fGun = new G4ParticleGun(1);
    auto* e = G4ParticleTable::GetParticleTable()->FindParticle("e-");
    fGun->SetParticleDefinition(e);
    fGun->SetParticleEnergy(120.0 * GeV);
    fGun->SetParticleMomentumDirection(G4ThreeVector(0, 0, 1));
}

PrimaryGeneratorAction::~PrimaryGeneratorAction() { delete fGun; }

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* ev) {
    // Gaussian beam profile σ = 2.9 mm (consistent with firstsim)
    G4double sigma = 2.9 * mm;
    fGun->SetParticlePosition(G4ThreeVector(
        G4RandGauss::shoot(0., sigma),
        G4RandGauss::shoot(0., sigma),
        -100.0 * mm));   // upstream of module front face
    fGun->GeneratePrimaryVertex(ev);
}
