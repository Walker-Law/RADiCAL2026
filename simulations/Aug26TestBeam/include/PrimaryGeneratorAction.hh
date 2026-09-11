// PrimaryGeneratorAction — the T10 electron beam: Gaussian spot, Gaussian
// momentum spread around the macro's /gun/energy.
#ifndef PrimaryGeneratorAction_h
#define PrimaryGeneratorAction_h
#include "G4VUserPrimaryGeneratorAction.hh"
#include "globals.hh"
class G4ParticleGun;
class G4Event;

class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction {
public:
    PrimaryGeneratorAction();
    ~PrimaryGeneratorAction() override;
    void GeneratePrimaries(G4Event*) override;
private:
    G4ParticleGun* fGun;
    G4double fNominal = -1.;   // the macro's energy (the beam file's momentum)
    G4double fLastSet = -1.;   // the smeared value we last wrote into the gun
};
#endif
