#include "SteppingAction.hh"
#include "EventAction.hh"
#include "G4Step.hh"
#include "G4LogicalVolume.hh"
#include "G4SystemOfUnits.hh"

SteppingAction::SteppingAction(EventAction* eventAction) : fEventAction(eventAction) {}
SteppingAction::~SteppingAction() {}

void SteppingAction::UserSteppingAction(const G4Step* step) {
    auto volume = step->GetPreStepPoint()->GetTouchableHandle()->GetVolume()->GetLogicalVolume();
    G4String volName = volume->GetName();
    G4double edep = step->GetTotalEnergyDeposit();

    // 1. Main Scintillating Plates
    if (volName == "LYSO") {
        G4int copyNo = step->GetPreStepPoint()->GetTouchableHandle()->GetCopyNumber();
        if (edep > 0.) fEventAction->AddEdep(copyNo, edep);
    }
    
    // 2. CENTRAL Capillary (ENERGY): Integrates entire volume since it is all WLS
    else if (volName == "Cap_Central_WLS") {
        if (edep > 0.) {
            fEventAction->AddCentralCapEnergy(edep);
        }
    }
    
    // 3. CORNER Capillaries (TIMING): Only triggers in the tiny 10mm WLS window
    else if (volName == "Cap_Corner_WLS") {
        if (edep > 0.) {
            G4double zPos    = step->GetPreStepPoint()->GetPosition().z();
            G4double tGlobal = step->GetPreStepPoint()->GetGlobalTime();

            G4double zFront = -67.5 * mm;
            G4double zBack  =  67.5 * mm;

            G4double distToFront = zPos - zFront;
            G4double distToBack  = zBack - zPos;

            G4double c_light = 299.792458 * mm / ns;
            G4double v_quartz = c_light / 1.46; // n = 1.46

            G4double tArrivalFront = tGlobal + (distToFront / v_quartz);
            G4double tArrivalBack  = tGlobal + (distToBack / v_quartz);

            G4double deltaT = tArrivalBack - tArrivalFront;

            fEventAction->RecordCornerHit(deltaT, zPos);
        }
    }
}
