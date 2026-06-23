#include "BeamState.hh"

namespace {
    G4ThreadLocal G4double gBeamX = 0.;
    G4ThreadLocal G4double gBeamY = 0.;
}

void     BeamState::Set(G4double x, G4double y) { gBeamX = x; gBeamY = y; }
G4double BeamState::X() { return gBeamX; }
G4double BeamState::Y() { return gBeamY; }

G4int BeamState::Quadrant() {
    const bool right = (gBeamX >= 0.);
    const bool up    = (gBeamY >= 0.);
    if (right  && up)  return 0;   // TR (+,+)
    if (!right && up)  return 1;   // TL (-,+)
    if (right  && !up) return 2;   // BR (+,-)
    return 3;                      // BL (-,-)
}
