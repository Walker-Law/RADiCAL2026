#include "SteppingAction.hh"
#include "EventAction.hh"
#include "G4Step.hh"
#include "G4LogicalVolume.hh"
#include "G4SystemOfUnits.hh"

SteppingAction::SteppingAction(EventAction* ea) : fEventAction(ea) {}
SteppingAction::~SteppingAction() {}

void SteppingAction::UserSteppingAction(const G4Step* step) {
    G4double edep = step->GetTotalEnergyDeposit();
    if (edep <= 0.) return;

    auto touchable = step->GetPreStepPoint()->GetTouchableHandle();
    auto logVol    = touchable->GetVolume()->GetLogicalVolume();
    const G4String& name = logVol->GetName();

    if (name == "LYSO") {
        G4int    copy = touchable->GetCopyNumber();
        G4double x    = step->GetPreStepPoint()->GetPosition().x();
        G4double y    = step->GetPreStepPoint()->GetPosition().y();
        fEventAction->AddEdepLYSO(copy, edep, x, y);
    }
    else if (name == "W_Absorber") {
        G4int copy = touchable->GetCopyNumber();
        fEventAction->AddEdepW(copy, edep);
    }
    else if (name == "Cap_Center_EJ309") {
        fEventAction->AddCenterCapEdep(edep);
    }
    else if (name == "Cap_Corner_WLS") {
        G4int    corner = touchable->GetCopyNumber();
        G4double z      = step->GetPreStepPoint()->GetPosition().z();
        G4double t      = step->GetPreStepPoint()->GetGlobalTime();
        fEventAction->RecordCornerWLS(corner, edep, z, t);
    }
}
