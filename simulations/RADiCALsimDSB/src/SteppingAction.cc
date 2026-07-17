#include "SteppingAction.hh"
#include "EventAction.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4LogicalVolume.hh"
#include "G4OpticalPhoton.hh"
#include "G4SystemOfUnits.hh"
#include "G4VProcess.hh"
#include <cstdlib>

// Optical-photon transport caps (read once from the environment). A Cherenkov
// photon in the quartz light guide can ricochet thousands of times (98% Tyvek
// reflectivity, ~1 m absorption) before it dies — each bounce a tracking step,
// which makes optical events crawl. Killing photons past a max step count
// (and/or a max global time) bounds the per-photon work. For a TIMING study
// this is physically safe: the 5% CFD leading edge is set by the first few ns of
// light; photons still bouncing at tens of ns can't move it. Both default OFF.
static G4int    optMaxStep() { static G4int v   = (std::getenv("RADICAL_OPT_MAXSTEP") ? std::atoi(std::getenv("RADICAL_OPT_MAXSTEP")) : 0);   return v; }
static G4double optTMax()    { static G4double v = (std::getenv("RADICAL_OPT_TMAX")    ? std::atof(std::getenv("RADICAL_OPT_TMAX"))    : 0.); return v; }

SteppingAction::SteppingAction(EventAction* ea) : fEventAction(ea) {}
SteppingAction::~SteppingAction() {}

void SteppingAction::UserSteppingAction(const G4Step* step) {
    G4Track* track = step->GetTrack();

    // ── Optical photons: detect at the end photodetectors, then kill ────────
    if (track->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {
        auto postVol = step->GetPostStepPoint()->GetTouchableHandle()->GetVolume();
        if (postVol) {
            const G4String& pn = postVol->GetLogicalVolume()->GetName();
            if (pn == "PD_Upstream" || pn == "PD_Downstream") {
                // Tag by creation process:
                //   0 = Cherenkov (solid-rod contaminant; real hollow capillary
                //       has almost none)
                //   1 = Scintillation (DSB1 fiber self-scint from direct dE/dx)
                //   2 = OpWLS (LYSO 420 nm light absorbed + re-emitted at 495 nm
                //       by DSB1 — THE realistic RADiCAL signal path; also
                //       includes quartz Cherenkov that got WLS-shifted, which
                //       the real fiber shifts too)
                const G4VProcess* cp = track->GetCreatorProcess();
                G4int cat = 1;
                if (cp) {
                    const G4String& pname = cp->GetProcessName();
                    if      (pname == "Cerenkov") cat = 0;
                    else if (pname == "OpWLS")    cat = 2;
                }
                fEventAction->RecordPhoton(postVol->GetCopyNo(),
                                           pn == "PD_Upstream",
                                           step->GetPostStepPoint()->GetGlobalTime(),
                                           cat);
                track->SetTrackStatus(fStopAndKill);
                return;
            }
        }
        // Transport caps: kill undetected photons that have bounced/lived too long.
        const G4int    mx = optMaxStep();
        const G4double tm = optTMax();
        if (mx > 0 && track->GetCurrentStepNumber() >= mx)
            track->SetTrackStatus(fStopAndKill);
        else if (tm > 0. && step->GetPostStepPoint()->GetGlobalTime() > tm * ns)
            track->SetTrackStatus(fStopAndKill);
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
