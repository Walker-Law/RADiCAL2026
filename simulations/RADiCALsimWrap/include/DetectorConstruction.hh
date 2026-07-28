// DetectorConstruction — all geometry and materials live here.
#ifndef DetectorConstruction_h
#define DetectorConstruction_h
#include "G4VUserDetectorConstruction.hh"
#include "globals.hh"

class DetectorConstruction : public G4VUserDetectorConstruction {
public:
    G4VPhysicalVolume* Construct() override;

    // Geometry constants — public so the analysis / other code can read them.
    // (All lengths in mm; see the .cc for where each number comes from.)
    static constexpr G4double tileXY      = 14.0;     // tile is 14 x 14 mm
    static constexpr G4double lysoThick   = 1.5;      // LYSO plate thickness
    static constexpr G4double wThick      = 2.5;      // tungsten plate thickness
    static constexpr G4double tyvekThick  = 0.2032;   // Tyvek foil (0.008 inch)
    static constexpr G4int    nLYSO       = 29;
    static constexpr G4int    nW          = 28;
    // stack length = 29*1.5 + 28*2.5 + 56*0.2032 = 124.88 mm
    static constexpr G4double stackZ = nLYSO*lysoThick + nW*wThick
                                       + (nLYSO+nW-1)*tyvekThick;
    static constexpr G4double showerMaxDepth = 40.4;  // from front face (paper Fig 7)
    static constexpr G4double wlsLen      = 15.0;     // DSB1 length at shower max
    static constexpr G4double cornerOff   = 3.5;      // fibre centre, in from edge
    static constexpr G4double fibreR      = 0.575;    // quartz/DSB1 outer radius
    static constexpr G4double holeR       = 0.65;     // drilled hole radius in tiles

private:
    void DefineMaterials();
};
#endif
