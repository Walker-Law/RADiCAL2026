#include "SteppingAction.hh"
#include "EventAction.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4OpticalPhoton.hh"
#include "G4VProcess.hh"
#include "G4LogicalVolume.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"
#include <cstdlib>

// Photon detection efficiency of the silicon photomultiplier (probability a
// photon reaching the sensor is counted). Flat 0.36 = onsemi MicroFJ at DSB1's
// 495 nm. A flat value treats DSB1 (495 nm) and LuAG:Ce (530 nm) light the
// same, which the real sensor does not quite do — a known, stated approximation.
// Override with RADSIMPLE_PDE.
static G4double PDE() {
    static G4double v = (std::getenv("RADSIMPLE_PDE")
                         ? std::atof(std::getenv("RADSIMPLE_PDE")) : 0.36);
    return v;
}

// Per-photon step cap: a photon trapped in a long total-internal-reflection
// bounce chain is killed after this many steps (anti-hang safety valve; costs
// no measurable light). RADSIMPLE_PHOTON_STEP_CAP=0 disables it.
static G4int OptMaxStep() {
    static G4int v = [] {
        if (const char* s = std::getenv("RADSIMPLE_PHOTON_STEP_CAP")) return std::atoi(s);
        if (const char* s = std::getenv("RADSIMPLE_OPT_MAXSTEP"))     return std::atoi(s);
        return 20000;
    }();
    return v;
}

// Which light population made this photon (EventAction.hh origin codes).
static G4int PhotonOrigin(const G4Track* track) {
    const G4VProcess* cp = track->GetCreatorProcess();
    const G4String pn = cp ? cp->GetProcessName() : "";
    if (pn == "Cerenkov") return 0;
    if (pn == "OpWLS")    return 2;
    if (pn == "Scintillation") {
        const G4LogicalVolume* lv = track->GetLogicalVolumeAtVertex();
        if (lv && lv->GetName() == "FIL") return 3;   // the filament's own light
        return 1;                                     // LYSO light, unshifted
    }
    return 1;
}

void SteppingAction::UserSteppingAction(const G4Step* step) {
    auto track = step->GetTrack();

    // ---- optical photon: has it just arrived at the upstream sensor? ----
    if (track->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {
        const G4int cap = OptMaxStep();
        if (cap > 0 && track->GetCurrentStepNumber() >= cap) {
            track->SetTrackStatus(fStopAndKill);
            return;
        }
        auto post = step->GetPostStepPoint()->GetTouchableHandle()->GetVolume();
        if (post && post->GetLogicalVolume()->GetName() == "PD_Up") {
            // Accept ONLY light that arrives THROUGH the fibre the sensor is
            // glued to (previous volume = the upstream quartz stub, or the
            // filament itself). Stray light through air is faster than guided
            // light and would corrupt the leading edge.
            auto preV = step->GetPreStepPoint()->GetTouchableHandle()->GetVolume();
            const G4String pn = preV ? preV->GetLogicalVolume()->GetName() : "";
            const bool viaFibre = (pn == "QuartzUp" || pn == "FIL");
            if (viaFibre && G4UniformRand() <= PDE()) {
                const G4int corner = post->GetCopyNo();               // 0..3
                const G4double t = step->GetPostStepPoint()->GetGlobalTime()/ns;
                fEvt->RecordPhoton(corner, t, PhotonOrigin(track));
            }
            track->SetTrackStatus(fStopAndKill);                     // absorbed either way
        }
        return;                                                      // photons deposit no ionisation
    }

    // ---- charged/neutral: route energy deposits / times by volume name ----
    auto pre = step->GetPreStepPoint()->GetTouchableHandle()->GetVolume();
    if (!pre) return;
    const G4String& vn = pre->GetLogicalVolume()->GetName();
    const G4double  e  = step->GetTotalEnergyDeposit();

    if (vn == "LYSO") {
        // Plate copy numbers count ALL plates (LYSO even, W odd): layer = copy/2.
        if (e > 0.) fEvt->AddLYSO(e, pre->GetCopyNo() / 2);
    }
    else if (vn == "W")      { if (e > 0.) fEvt->AddW(e); }
    else if (vn == "FIL")    { if (e > 0.) fEvt->AddFil(e); }
    else if (vn == "Trig1")  { if (e > 0.) fEvt->AddTrig(0, e); }
    else if (vn == "Trig2")  { if (e > 0.) fEvt->AddTrig(1, e); }
    else if (vn == "MCPRadiator") {
        if (track->GetDefinition()->GetPDGCharge() != 0.)
            fEvt->RecordMCP(step->GetPreStepPoint()->GetGlobalTime()/ns);
    }
}
