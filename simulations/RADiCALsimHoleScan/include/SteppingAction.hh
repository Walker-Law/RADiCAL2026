#ifndef SteppingAction_h
#define SteppingAction_h

#include "G4UserSteppingAction.hh"

class EventAction;

class SteppingAction : public G4UserSteppingAction {
public:
    SteppingAction(EventAction* ea);
    ~SteppingAction() override;
    void UserSteppingAction(const G4Step*) override;
private:
    EventAction* fEventAction;
};

#endif
