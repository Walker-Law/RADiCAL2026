#include "PrimaryGeneratorAction.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4Event.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"
#include <cstdlib>

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
    // Beam spot: Gaussian in x,y with sigma = RADSIMPLE_BEAM_SPOT_MM (default
    // 2.9 mm, the measured H2 beam width used across this project; 0 = pencil).
    // This is REQUIRED physics, not decoration: the tiles have a real central
    // hole on the beam axis (papers' Fig. 2), and a zero-width beam at exactly
    // (0,0) travels straight down that channel without showering — 2303.05580
    // documents ("particles ... through the central hole") and cuts that
    // pathology. A finite spot makes it the rare accident it is in the data.
    static G4double sig = -1.;
    if (sig < 0.) {
        const char* s = std::getenv("RADSIMPLE_BEAM_SPOT_MM");
        sig = s ? std::atof(s) : 2.9;
        if (sig < 0.) sig = 0.;
    }
    const G4double x = (sig > 0.) ? G4RandGauss::shoot(0., sig*mm) : 0.;
    const G4double y = (sig > 0.) ? G4RandGauss::shoot(0., sig*mm) : 0.;
    fGun->SetParticlePosition(G4ThreeVector(x, y, -450.*mm));
    fGun->GeneratePrimaryVertex(evt);
}
