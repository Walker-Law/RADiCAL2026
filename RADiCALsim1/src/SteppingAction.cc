#include "SteppingAction.hh"
#include "EventAction.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4LogicalVolume.hh"
#include "G4OpticalPhoton.hh"
#include "G4SystemOfUnits.hh"

SteppingAction::SteppingAction(EventAction* ea) : fEventAction(ea) {}
SteppingAction::~SteppingAction() {}

void SteppingAction::UserSteppingAction(const G4Step* step) {
    G4Track* track = step->GetTrack();

    // ── Optical photons: detect at the end photodetectors, then kill ────────
    if (track->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {
        auto postVol = step->GetPostStepPoint()->GetTouchableHandle()->GetVolume();
        if (postVol) {
            const G4String& pn = postVol->GetLogicalVolume()->GetName();
            if (pn == "PD_Front" || pn == "PD_Back") {
                fEventAction->RecordPhoton(postVol->GetCopyNumber(),
                                           pn == "PD_Front",
                                           step->GetPostStepPoint()->GetGlobalTime());
                track->SetTrackStatus(fStopAndKill);
            }
        }
        return;   // optical photons deposit no sampling energy
    }

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
    // ── CERN test-beam line detectors ──────────────────────────────────────
    else if (name == "Trig1") {
        fEventAction->RecordTrig(0, edep, step->GetPreStepPoint()->GetGlobalTime());
    }
    else if (name == "Trig2") {
        fEventAction->RecordTrig(1, edep, step->GetPreStepPoint()->GetGlobalTime());
    }
    else if (name == "MCP_Radiator") {
        fEventAction->RecordMCP(edep, step->GetPreStepPoint()->GetGlobalTime());
    }
    else if (name == "PbGlass") {
        fEventAction->AddPbGlassEdep(edep);
    }
}
