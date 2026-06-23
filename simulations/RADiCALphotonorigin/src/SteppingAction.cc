#include "SteppingAction.hh"
#include "EventAction.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4LogicalVolume.hh"
#include "G4OpticalPhoton.hh"
#include "G4SystemOfUnits.hh"
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

// Which corner capillary a photon was born in, from its creation volume name
// ("Cap_Corner_<part>_<0..3>"). Returns 0..3 for a corner, or 4 for "elsewhere"
// (centre-cap Cherenkov, etc.). Drives the SiPM-vs-origin cross-talk matrix.
static G4int originCorner(const G4Track* track) {
    const G4LogicalVolume* birthVol = track->GetLogicalVolumeAtVertex();
    if (!birthVol) return 4;
    const G4String& name = birthVol->GetName();
    if (name.rfind("Cap_Corner_", 0) != 0 || name.empty()) return 4;
    const char last = name.back();
    return (last >= '0' && last <= '3') ? (last - '0') : 4;
}

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
                fEventAction->RecordPhoton(postVol->GetCopyNo(),
                                           pn == "PD_Upstream",
                                           step->GetPostStepPoint()->GetGlobalTime(),
                                           originCorner(track));
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

    // Plain "LYSO" tiles plus the 4 shower-max "LYSO_SMQ_<q>" quadrant volumes
    // (both carry the LYSO layer index as copy number) score into the same layer.
    if (name == "LYSO" || name.rfind("LYSO_SMQ_", 0) == 0) {
        G4int    copy = touchable->GetCopyNumber();   // = LYSO layer index
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
    else if (name.find("Cap_Corner_WLS") != G4String::npos) {  // now "Cap_Corner_WLS_0..3"
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
