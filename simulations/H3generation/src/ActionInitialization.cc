#include "ActionInitialization.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "SteppingAction.hh"

void ActionInitialization::Build() const {
    auto* runAction = new RunAction();
    SetUserAction(new PrimaryGeneratorAction());
    SetUserAction(runAction);
    SetUserAction(new SteppingAction(runAction));
}
