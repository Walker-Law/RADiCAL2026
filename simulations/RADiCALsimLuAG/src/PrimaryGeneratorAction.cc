#include "PrimaryGeneratorAction.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"
#include <cstdlib>

PrimaryGeneratorAction::PrimaryGeneratorAction() {
    fGun = new G4ParticleGun(1);
    auto* e = G4ParticleTable::GetParticleTable()->FindParticle("e-");
    fGun->SetParticleDefinition(e);

    // Beam kinetic energy: env var RADICAL_BEAM_ENERGY_GEV overrides the 120 GeV
    // default. Read in the constructor (runs per worker thread → MT-safe energy
    // scan: one process per energy, all workers see the same value).
    G4double beamE = 120.0 * GeV;
    if (const char* env = std::getenv("RADICAL_BEAM_ENERGY_GEV")) {
        G4double v = std::atof(env);
        if (v > 0.) beamE = v * GeV;
    }
    fGun->SetParticleEnergy(beamE);
    fGun->SetParticleMomentumDirection(G4ThreeVector(0, 0, 1));
}

PrimaryGeneratorAction::~PrimaryGeneratorAction() { delete fGun; }

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* ev) {
    // Gaussian beam profile σ = 2.9 mm (consistent with firstsim)
    G4double sigma = 2.9 * mm;
    fGun->SetParticlePosition(G4ThreeVector(
        G4RandGauss::shoot(0., sigma),
        G4RandGauss::shoot(0., sigma),
        -500.0 * mm));   // upstream of the first trigger counter (z = -400 mm)
    fGun->GeneratePrimaryVertex(ev);
}
