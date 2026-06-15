#pragma once
#include "G4UserRunAction.hh"
#include "G4Accumulable.hh"

class RunAction : public G4UserRunAction {
public:
    RunAction();
    void BeginOfRunAction(const G4Run*) override;
    void EndOfRunAction(const G4Run*) override;

    void AddTriton(const G4String& volume);
    void AddParticleEntering(const G4String& volume);

private:
    // tritons produced per layer
    G4Accumulable<G4int> fTritonsBe{0};
    G4Accumulable<G4int> fTritonsGraph{0};
    G4Accumulable<G4int> fTritonsBlanket{0};

    // particles (any) entering each layer
    G4Accumulable<G4int> fEnterBe{0};
    G4Accumulable<G4int> fEnterGraph{0};
    G4Accumulable<G4int> fEnterBlanket{0};

    // source neutrons = events
    G4Accumulable<G4int> fSourceNeutrons{0};
};
