#include "PrimaryGeneratorAction.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4Event.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"
#include <cstdlib>
#include <cmath>

// One electron per event, fired straight down +z from z = -450 mm, upstream of
// the trigger counters (-400/-350 mm) and the module (front face at -62.44 mm).
// The nominal energy comes from the macro (/gun/energy), one value per beam
// file in the run manifest: 1, 3, 5, 7, 9, 11 GeV.
//
// Electrons at every energy: the 1/3/5 GeV beam files were POSITIVE (positrons)
// and the 7/9/11 GeV files negative, but shower physics is charge-symmetric,
// so one particle type is used throughout (confirmed with the beam-test lead).
PrimaryGeneratorAction::PrimaryGeneratorAction() {
    fGun = new G4ParticleGun(1);
    fGun->SetParticleDefinition(
        G4ParticleTable::GetParticleTable()->FindParticle("e-"));
    fGun->SetParticleMomentumDirection(G4ThreeVector(0., 0., 1.));
    fGun->SetParticleEnergy(5.*GeV);                  // default; the macro overrides
    fGun->SetParticlePosition(G4ThreeVector(0., 0., -450.*mm));
}

PrimaryGeneratorAction::~PrimaryGeneratorAction() { delete fGun; }

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* evt) {
    // Beam spot: Gaussian in x,y, sigma = RADSIMPLE_BEAM_SPOT_MM. No T10 spot
    // measurement exists for this run (the beam files only say "focus optimized
    // XBPF +3.0 m"), so the H2 value 2.9 mm is kept as a stated placeholder.
    // A finite spot is required physics: the tiles have a real central hole on
    // the beam axis, and a pencil beam at (0,0) would travel down it unshowered.
    static G4double sig = -1.;
    if (sig < 0.) {
        const char* s = std::getenv("RADSIMPLE_BEAM_SPOT_MM");
        sig = s ? std::atof(s) : 2.9;
        if (sig < 0.) sig = 0.;
    }
    // Momentum spread: Gaussian, relative sigma = RADSIMPLE_BEAM_DP. Default
    // 0.01: the T10 characterization paper (arXiv:2507.02567) reports the
    // acceptance collimator "maintained at +-1%" for its data taking; the
    // collimator setting for the RADiCAL runs was not recorded, so +-1% is the
    // literature value, stated as such.
    static G4double dp = -1.;
    if (dp < 0.) {
        const char* s = std::getenv("RADSIMPLE_BEAM_DP");
        dp = s ? std::atof(s) : 0.01;
        if (dp < 0.) dp = 0.;
    }
    // Detect a macro change of /gun/energy: if the gun holds something other
    // than the value WE last set, the macro moved it — that is the new nominal.
    const G4double now = fGun->GetParticleEnergy();
    if (fNominal < 0. || std::fabs(now - fLastSet) > 1e-9) fNominal = now;
    const G4double e = (dp > 0.) ? fNominal * (1. + G4RandGauss::shoot(0., dp)) : fNominal;
    fGun->SetParticleEnergy(e);
    fLastSet = e;

    const G4double x = (sig > 0.) ? G4RandGauss::shoot(0., sig*mm) : 0.;
    const G4double y = (sig > 0.) ? G4RandGauss::shoot(0., sig*mm) : 0.;
    fGun->SetParticlePosition(G4ThreeVector(x, y, -450.*mm));
    fGun->GeneratePrimaryVertex(evt);
}
