#include "SteppingAction.hh"
#include "EventAction.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4OpticalPhoton.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"
#include <cstdlib>

// SiPM photon detection efficiency (probability a photon reaching the sensor is
// counted). 0.36 = onsemi MicroFJ at DSB1's 495 nm green. Override with RADSIMPLE_PDE.
static G4double PDE() {
    static G4double v = (std::getenv("RADSIMPLE_PDE")
                         ? std::atof(std::getenv("RADSIMPLE_PDE")) : 0.36);
    return v;
}

// Per-photon step cap (2026-07-27, ported from RADiCALsimDSB/StackingAction.hh:
// "a rare 'monster' shower can dump enough energy ... to spawn ... optical
// photons, hanging that worker thread"). Without this, a photon trapped in a
// long total-internal-reflection bounce chain (Tyvek 98% reflective, quartz/
// DSB1 absorption lengths of ~1-10 m) can run for a very long time before
// finally absorbing or hitting a SiPM -- exactly what stalled a worker thread
// on a rare 120 GeV event. A photon past the cap is killed outright: it never
// reached a SiPM within a generous bounce budget, so it would not have set the
// first-photon timing anyway. RADSIMPLE_OPT_MAXSTEP=0 disables the cap.
static G4int OptMaxStep() {
    static G4int v = [] {
        if (const char* s = std::getenv("RADSIMPLE_PHOTON_STEP_CAP")) return std::atoi(s);
        if (const char* s = std::getenv("RADSIMPLE_OPT_MAXSTEP"))     return std::atoi(s); // old name
        return 20000;
    }();
    return v;
}

void SteppingAction::UserSteppingAction(const G4Step* step) {
    auto track = step->GetTrack();

    // ---- optical photon: has it just arrived at a SiPM? ----
    if (track->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {
        const G4int cap = OptMaxStep();
        if (cap > 0 && track->GetCurrentStepNumber() >= cap) {
            track->SetTrackStatus(fStopAndKill);            // bounce budget exhausted
            return;
        }
        auto post = step->GetPostStepPoint()->GetTouchableHandle()->GetVolume();
        if (post) {
            const G4String& n = post->GetLogicalVolume()->GetName();
            if (n == "PD_Up" || n == "PD_Down") {
                // Accept ONLY light that arrives THROUGH the fibre the SiPM is
                // optically coupled to (previous volume = quartz stub or DSB1).
                // A real SiPM is glued to its capillary end face — it does not
                // sit in open air collecting strays. Without this cut, photons
                // escaping the LYSO end faces or flying down the (real) central
                // air hole reach a PD DIRECTLY through air — and air light is
                // FASTER than guided light (300 vs ~205 mm/ns), so those strays
                // land first and wreck the first-photon timing (measured: dT
                // scatter blew up from ~0.1 ns to ~+-1.7 ns when the beam-line
                // geometry made the stray paths easy to hit).
                auto preV = step->GetPreStepPoint()->GetTouchableHandle()->GetVolume();
                const G4String pn = preV ? preV->GetLogicalVolume()->GetName() : "";
                const bool viaFibre = (pn == "QuartzUp" || pn == "QuartzDn" || pn == "DSB1");
                if (viaFibre && G4UniformRand() <= PDE()) {     // detect with prob PDE
                    const G4int channel = post->GetCopyNo();    // 0..3 corners, 4 = center
                    const G4double t = step->GetPostStepPoint()->GetGlobalTime()/ns;
                    fEvt->RecordPhoton(channel, n == "PD_Up", t);
                }
                track->SetTrackStatus(fStopAndKill);            // absorbed either way
            }
        }
        return;                                                // photons deposit no ionisation
    }

    // ---- charged/neutral: route energy deposits / times by volume name ----
    auto pre = step->GetPreStepPoint()->GetTouchableHandle()->GetVolume();
    if (!pre) return;
    const G4String& vn = pre->GetLogicalVolume()->GetName();
    const G4double  e  = step->GetTotalEnergyDeposit();

    if (vn == "LYSO") {
        // Plate copy numbers count ALL plates (LYSO even, W odd), so the LYSO
        // layer index 0..28 is copyNo/2 — gives the longitudinal profile.
        if (e > 0.) fEvt->AddLYSO(e, pre->GetCopyNo() / 2);
    }
    else if (vn == "W")          { if (e > 0.) fEvt->AddW(e); }
    else if (vn == "Trig1")      { if (e > 0.) fEvt->AddTrig(0, e); }
    else if (vn == "Trig2")      { if (e > 0.) fEvt->AddTrig(1, e); }
    else if (vn == "PbGlass")    { if (e > 0.) fEvt->AddPbGlass(e); }
    else if (vn == "MCPRadiator") {
        // MCP timing reference: earliest CHARGED-particle arrival = event t0.
        // (Pure truth time — the MCP's own resolution is electronics, not here.)
        if (track->GetDefinition()->GetPDGCharge() != 0.)
            fEvt->RecordMCP(step->GetPreStepPoint()->GetGlobalTime()/ns);
    }
}
