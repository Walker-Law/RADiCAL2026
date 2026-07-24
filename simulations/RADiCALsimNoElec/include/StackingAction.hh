#ifndef StackingAction_h
#define StackingAction_h

#include "G4UserStackingAction.hh"
#include "globals.hh"

// Caps the number of optical photons tracked per event. At 120 GeV a rare
// "monster" shower can dump enough energy into a corner quartz/WLS to spawn
// millions of optical photons, hanging that worker thread. Capping bounds the
// per-event cost while preserving timing: prompt (Cherenkov/early-scint) photons
// are created first and survive the cap; only the late scintillation tail — which
// does not affect the leading-edge downstream/upstream ΔT — is discarded.
//
// SIZING (July 2026, post LYSO-optical-activation): with LYSO scintillating at
// 33200 ph/MeV * RADICAL_LYSO_SCINT_SCALE (default 1e-3), full-stack raw photon
// production alone reaches ~460k/event at 120 GeV — dangerously close to a
// 500k budget. Hitting the cap truncates events non-uniformly across energy,
// which showed up as a physically impossible UPTURN in sigma_t at high E (more
// energy -> worse timing) because high-E events were silently light-starved.
// Default raised accordingly; override with RADICAL_MAX_OPT_PHOTONS if you
// change RADICAL_LYSO_SCINT_SCALE and need to rescale the budget with it.
class StackingAction : public G4UserStackingAction {
public:
    StackingAction() = default;
    ~StackingAction() override = default;

    G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track* track) override;
    void PrepareNewEvent() override;

    // WLS photons EMITTED in the fibers this event (OpWLS creations, counted
    // BEFORE any budget kill). With H1[30] PhotonsWLS (detected) this measures
    // the sim's end-to-end capture x transport x PDE fraction — checkable
    // against the physical TIR trapping limit (~5-6% per fiber end for n~1.5).
    static G4int  WlsEmitted()      { return fgWlsEmitted; }
    static void   ResetWlsEmitted() { fgWlsEmitted = 0; }

private:
    G4int fNopt = 0;
    static G4int MaxOpt();
    static G4ThreadLocal G4int fgWlsEmitted;
};

#endif
