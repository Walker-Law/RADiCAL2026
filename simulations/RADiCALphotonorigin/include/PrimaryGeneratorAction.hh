#ifndef PrimaryGeneratorAction_h
#define PrimaryGeneratorAction_h

#include "G4VUserPrimaryGeneratorAction.hh"
#include "globals.hh"

class G4ParticleGun;

class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction {
public:
    PrimaryGeneratorAction();
    ~PrimaryGeneratorAction() override;
    void GeneratePrimaries(G4Event*) override;
private:
    G4ParticleGun* fGun;
    // If set, every event fires at a fixed transverse point (visual demo); else
    // each event samples a fresh random-uniform impact over the tile face (scan).
    bool     fFixed  = false;
    G4double fFixedX = 0.;
    G4double fFixedY = 0.;
};

#endif
