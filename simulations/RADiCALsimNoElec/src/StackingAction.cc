#include "StackingAction.hh"
#include "G4Track.hh"
#include "G4OpticalPhoton.hh"
#include "G4VProcess.hh"
#include "G4LogicalVolume.hh"
#include "Randomize.hh"
#include <cstdlib>

// Cherenkov photons born in LYSO are killed at stacking. In the real crystal
// scintillation (33200 ph/MeV) outnumbers Cherenkov ~1000:1, but the sim scales
// the scint yield down (RADICAL_LYSO_SCINT_SCALE) while Cherenkov generates at
// the full physical rate — keeping it would invert the real scint:Cherenkov
// ratio AND spend most of the event tracking photons that carry ~0.1% of the
// real signal. Set RADICAL_KEEP_LYSO_CHER=1 to keep them.
static bool keepLysoCher() {
    static bool v = (std::getenv("RADICAL_KEEP_LYSO_CHER") != nullptr);
    return v;
}

// Quartz/fiber Cherenkov thinning: keep each non-LYSO Cherenkov photon with
// probability RADICAL_QUARTZ_CHER_KEEP in (0,1] (default 1.0 = keep all).
// Binomial thinning at stacking is statistically identical to reducing the
// Cherenkov yield, so this restores the REAL scint:Cherenkov ratio (~1000:1)
// without the intractable cost of scaling the WLS yield UP: the sim's scaled
// LYSO yield (RADICAL_LYSO_SCINT_SCALE=1e-3) over-weights Cherenkov ~1000x,
// letting prompt Cherenkov set the 5% CFD leading edge. Detected ratio at
// default scales is ~8.6:1 (150 GeV), so KEEP=0.01 gives the realistic ~860:1.
static G4double quartzCherKeep() {
    static G4double v = (std::getenv("RADICAL_QUARTZ_CHER_KEEP")
                          ? std::atof(std::getenv("RADICAL_QUARTZ_CHER_KEEP")) : 1.0);
    return v;
}

G4int StackingAction::MaxOpt() {
    static G4int v = (std::getenv("RADICAL_MAX_OPT_PHOTONS")
                       ? std::atoi(std::getenv("RADICAL_MAX_OPT_PHOTONS")) : 4000000);
    return v;
}

G4ThreadLocal G4int StackingAction::fgWlsEmitted = 0;

void StackingAction::PrepareNewEvent() { fNopt = 0; }

G4ClassificationOfNewTrack
StackingAction::ClassifyNewTrack(const G4Track* track) {
    if (track->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {
        // Kill decisions FIRST so killed photons don't consume the budget
        // (previously LYSO Cherenkov ate ~15% of the 4M budget at 150 GeV).
        const G4VProcess* cp = track->GetCreatorProcess();
        // capture-fraction denominator: every OpWLS re-emission, pre-kill
        if (cp && cp->GetProcessName() == "OpWLS") ++fgWlsEmitted;
        if (cp && cp->GetProcessName() == "Cerenkov") {
            const G4LogicalVolume* lv = track->GetLogicalVolumeAtVertex();
            if (!keepLysoCher() && lv && lv->GetName() == "LYSO") return fKill;
            // quartz/fiber Cherenkov: binomial thinning to the requested keep fraction
            const G4double keep = quartzCherKeep();
            if (keep < 1.0 && G4UniformRand() > keep) return fKill;
        }
        // Per-event transport budget: drop photons beyond it. WARNING: if this
        // trips, detected N_pe SATURATES and timing estimators are biased —
        // discovered July 2026 when a LYSO-yield sweep silently clipped here
        // (generated photons must stay under RADICAL_MAX_OPT_PHOTONS).
        if (++fNopt > MaxOpt()) return fKill;
    }
    return fUrgent;
}
