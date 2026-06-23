#ifndef BeamState_h
#define BeamState_h

#include "globals.hh"

// Per-event beam transverse impact, shared between the primary generator (the
// writer) and the event action / scoring (the reader). Thread-local: each worker
// keeps its own copy, so it is MT-safe with one process per energy/run.
//
// Quadrant convention (beam's-eye view, +x right, +y up). It matches the corner
// capillary copy numbers in DetectorConstruction and the colour key everywhere:
//   TR (+,+) = 0 (red)   TL (-,+) = 1 (yellow)
//   BR (+,-) = 2 (green) BL (-,-) = 3 (blue)
namespace BeamState {
    void     Set(G4double x, G4double y);
    G4double X();
    G4double Y();
    G4int    Quadrant();   // 0..3 per the convention above
}

#endif
