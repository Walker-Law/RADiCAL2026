#pragma once
#include "G4UserSteppingAction.hh"

class RunAction;

class SteppingAction : public G4UserSteppingAction {
public:
    SteppingAction(RunAction* runAction);
    void UserSteppingAction(const G4Step*) override;
private:
    RunAction* fRunAction;
};
