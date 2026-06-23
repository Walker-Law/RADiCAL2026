#include "PrimaryGeneratorAction.hh"
#include "BeamState.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"
#include <cstdlib>

namespace {
// Half-width of the square the beam impact is scanned over. The active tile face
// is 14x14 mm (+/-7 mm), so each event's random-uniform (x,y) here sweeps the
// whole transverse face across a run -> fills the position->SiPM maps.
constexpr G4double kBeamHalf = 7.0 * mm;

// Representative impact for a fixed-quadrant visual demo: right over that
// quadrant's corner capillary (+/-3.5 mm). Indexed TR,TL,BR,BL = 0..3.
const G4double kQuadX[4] = { +3.5 * mm, -3.5 * mm, +3.5 * mm, -3.5 * mm };
const G4double kQuadY[4] = { +3.5 * mm, +3.5 * mm, -3.5 * mm, -3.5 * mm };

// Read an env var as a length in mm; reports whether it was present.
G4double envMM(const char* name, bool& present) {
    const char* v = std::getenv(name);
    present = (v != nullptr);
    return present ? std::atof(v) * mm : 0.;
}
}  // namespace

PrimaryGeneratorAction::PrimaryGeneratorAction() {
    fGun = new G4ParticleGun(1);
    auto* e = G4ParticleTable::GetParticleTable()->FindParticle("e-");
    fGun->SetParticleDefinition(e);

    // Beam kinetic energy: env var RADICAL_BEAM_ENERGY_GEV overrides the 120 GeV
    // default. Read in the constructor (runs per worker thread -> MT-safe).
    G4double beamE = 120.0 * GeV;
    if (const char* env = std::getenv("RADICAL_BEAM_ENERGY_GEV")) {
        G4double v = std::atof(env);
        if (v > 0.) beamE = v * GeV;
    }
    fGun->SetParticleEnergy(beamE);
    fGun->SetParticleMomentumDirection(G4ThreeVector(0, 0, 1));

    // Optional FIXED beam impact (for the visual demo); otherwise random-uniform.
    //   RADICAL_BEAM_QUADRANT=0..3  -> over that quadrant's corner capillary
    //   RADICAL_BEAM_X_MM / _Y_MM   -> an explicit point (mm), overrides quadrant
    if (const char* q = std::getenv("RADICAL_BEAM_QUADRANT")) {
        int i = std::atoi(q);
        if (i >= 0 && i < 4) { fFixed = true; fFixedX = kQuadX[i]; fFixedY = kQuadY[i]; }
    }
    bool hasX = false, hasY = false;
    G4double x = envMM("RADICAL_BEAM_X_MM", hasX);
    G4double y = envMM("RADICAL_BEAM_Y_MM", hasY);
    if (hasX || hasY) { fFixed = true; fFixedX = x; fFixedY = y; }
}

PrimaryGeneratorAction::~PrimaryGeneratorAction() { delete fGun; }

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* ev) {
    G4double x, y;
    if (fFixed) {
        x = fFixedX;
        y = fFixedY;
    } else {
        // Pencil beam at a fresh random-uniform impact -> the per-event position
        // is exactly known and tags the event for the position->SiPM map.
        x = (2. * G4UniformRand() - 1.) * kBeamHalf;
        y = (2. * G4UniformRand() - 1.) * kBeamHalf;
    }
    BeamState::Set(x, y);   // publish for the event action / scoring

    fGun->SetParticlePosition(G4ThreeVector(x, y, -500.0 * mm));   // upstream of Trig1
    fGun->GeneratePrimaryVertex(ev);
}
