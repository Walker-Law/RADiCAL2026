#include "ActionInitialization.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "EventAction.hh"
#include "SteppingAction.hh"
#include "StackingAction.hh"

// On a multithreaded run the master thread merges the per-thread histograms
// into radsimple_output.root — it needs its own RunAction to do that.
void ActionInitialization::BuildForMaster() const {
    SetUserAction(new RunAction());
}

void ActionInitialization::Build() const {
    // One RunAction owns the histograms/output. EventAction accumulates per event.
    // SteppingAction feeds EventAction. PrimaryGeneratorAction fires the beam.
    // ORDER MATTERS: EventAction must exist BEFORE RunAction, because RunAction
    // binds the ntuple's vector columns to the EventAction's members by
    // reference at creation time.
    SetUserAction(new PrimaryGeneratorAction());
    auto eventAction = new EventAction(nullptr);
    SetUserAction(eventAction);
    SetUserAction(new RunAction(eventAction));
    SetUserAction(new SteppingAction(eventAction));
    SetUserAction(new StackingAction());   // coherent Cherenkov thinning
}
