#include "PrimaryGeneratorAction.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh" // Required for Gaussian random distribution sampling

PrimaryGeneratorAction::PrimaryGeneratorAction() {
    fParticleGun = new G4ParticleGun(1);

    auto particleTable = G4ParticleTable::GetParticleTable();
    G4ParticleDefinition* particle = particleTable->FindParticle("e-");
    fParticleGun->SetParticleDefinition(particle);
    fParticleGun->SetParticleEnergy(120.0 * GeV);
    fParticleGun->SetParticleMomentumDirection(G4ThreeVector(0.0, 0.0, 1.0));
}

PrimaryGeneratorAction::~PrimaryGeneratorAction() {
    delete fParticleGun;
}

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* anEvent) {
    // Spatial Gaussian beam profile setup (1 Std Dev = 2.9 mm)
    G4double sigma = 2.9 * mm;
    G4double x = G4RandGauss::shoot(0.0, sigma);
    G4double y = G4RandGauss::shoot(0.0, sigma);
    
    // Position the gun at the sampled coordinates, 32.5 mm in front of the module face
    fParticleGun->SetParticlePosition(G4ThreeVector(x, y, -100.0 * mm));

    fParticleGun->GeneratePrimaryVertex(anEvent);
}
