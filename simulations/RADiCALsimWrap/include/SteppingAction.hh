// SteppingAction — runs at every simulation step. Two jobs:
//   1. add up energy deposited in the LYSO plates (for the energy observable),
//   2. detect optical photons that reach a SiPM (for the timing observable).
#ifndef SteppingAction_h
#define SteppingAction_h
#include "G4UserSteppingAction.hh"
class EventAction;
class G4Step;
class SteppingAction : public G4UserSteppingAction {
public:
    explicit SteppingAction(EventAction* ea) : fEvt(ea) {}
    void UserSteppingAction(const G4Step*) override;
private:
    EventAction* fEvt;
};
#endif
