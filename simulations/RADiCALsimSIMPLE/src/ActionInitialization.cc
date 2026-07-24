#include "ActionInitialization.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "EventAction.hh"
#include "SteppingAction.hh"

void ActionInitialization::Build() const {
    // One RunAction owns the histograms/output. EventAction accumulates per event.
    // SteppingAction feeds EventAction. PrimaryGeneratorAction fires the beam.
    SetUserAction(new PrimaryGeneratorAction());
    auto runAction = new RunAction();
    SetUserAction(runAction);
    auto eventAction = new EventAction(runAction);
    SetUserAction(eventAction);
    SetUserAction(new SteppingAction(eventAction));
}
