// DetectorConstruction.cc — the geometry and materials of the SIMPLE RADiCAL model.
//
// THE PICTURE (beam travels +z):
//
//     e- --->   [ LYSO | W | LYSO | W | ... | LYSO ]        (57 plates, Tyvek between)
//                        |                     |
//                 4 fibres run through the corners of every plate:
//                        quartz --- DSB1(15mm at shower max) --- quartz
//                        |                                          |
//                   PD_Up (SiPM)                               PD_Down (SiPM)
//
// LIGHT CHAIN (this is the whole point):
//   1. the shower deposits energy in the LYSO plates,
//   2. LYSO scintillates -> 420 nm blue photons,
//   3. a blue photon that reaches a corner fibre enters the quartz and, at the
//      15 mm DSB1 section (placed at shower max), is ABSORBED and RE-EMITTED as
//      495 nm green (Geant4 process "OpWLS"),
//   4. the green photon is guided by total internal reflection along the quartz
//      to the SiPM (PD) at each end,
//   5. we record the arrival time of the FIRST photon at each end.  The timing
//      observable is dT = t(down) - t(up), averaged over the 4 corners.
//
// NO electronics: there is no waveform, threshold, gain, noise, or digitization.
// dT is the pure first-photon arrival-time difference.

#include "DetectorConstruction.hh"
#include "G4NistManager.hh"
#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4Box.hh"
#include "G4Tubs.hh"
#include "G4SubtractionSolid.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4OpticalSurface.hh"
#include "G4LogicalSkinSurface.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"
#include "G4SystemOfUnits.hh"
#include <cstdlib>

// -------------------------------------------------------------------------
// MATERIALS.  Optical property values are the RADiCAL project's verified
// numbers (Luxium LYSO datasheet, fused-silica Sellmeier, DSB1 measured
// constants from arXiv:2401.01747). Photon-energy grid ~350-800 nm.
// -------------------------------------------------------------------------
void DetectorConstruction::DefineMaterials() {
    auto nist = G4NistManager::Instance();
    nist->FindOrBuildMaterial("G4_W");               // tungsten absorber
    nist->FindOrBuildMaterial("G4_Si");              // SiPM sensor body
    nist->FindOrBuildMaterial("G4_AIR");
    auto quartz = nist->FindOrBuildMaterial("G4_SILICON_DIOXIDE");  // fibre light guide

    // LYSO:Ce, built from its elements (not in the NIST table).
    auto Lu = nist->FindOrBuildElement("Lu");
    auto Y  = nist->FindOrBuildElement("Y");
    auto Si = nist->FindOrBuildElement("Si");
    auto O  = nist->FindOrBuildElement("O");
    auto C  = nist->FindOrBuildElement("C");
    auto H  = nist->FindOrBuildElement("H");
    auto lyso = new G4Material("LYSO", 7.1*g/cm3, 4);
    lyso->AddElement(Lu, 0.7145); lyso->AddElement(Y, 0.0403);
    lyso->AddElement(Si, 0.0637); lyso->AddElement(O, 0.1815);

    // DSB1 WLS shifter, modelled PDMS-like (a soft silicone-based plastic).
    auto dsb1 = new G4Material("DSB1", 1.05*g/cm3, 4);
    dsb1->AddElement(Si, 0.379); dsb1->AddElement(O, 0.216);
    dsb1->AddElement(C, 0.324);  dsb1->AddElement(H, 0.081);

    // Tyvek reflective wrap = polyethylene (CH2)n at nonwoven density.
    auto tyvek = new G4Material("Tyvek", 0.38*g/cm3, 2);
    tyvek->AddElement(C, 1); tyvek->AddElement(H, 2);

    const std::vector<G4double> phE =
        {1.55*eV, 2.07*eV, 2.48*eV, 2.76*eV, 3.10*eV, 3.54*eV};   // 800..350 nm

    // --- Fused quartz: transparent light guide (n ~ 1.46). ---
    auto qMPT = new G4MaterialPropertiesTable();
    qMPT->AddProperty("RINDEX",    phE, {1.455,1.457,1.460,1.462,1.466,1.472});
    qMPT->AddProperty("ABSLENGTH", phE, {10.*m,10.*m,10.*m,10.*m,8.*m,5.*m});
    quartz->SetMaterialPropertiesTable(qMPT);

    // --- Air: give it n=1 so optical boundaries work (photons can cross gaps). ---
    auto aMPT = new G4MaterialPropertiesTable();
    aMPT->AddProperty("RINDEX", phE, {1.0,1.0,1.0,1.0,1.0,1.0});
    G4Material::GetMaterial("G4_AIR")->SetMaterialPropertiesTable(aMPT);

    // --- LYSO: SCINTILLATOR. 33200 ph/MeV (datasheet), 36 ns, emits ~420 nm,
    //     n=1.81. The yield is scaled DOWN by RADSIMPLE_LYSO_SCALE for speed
    //     (full yield = ~5e8 photons per 120 GeV event, untrackable). The
    //     photon-counting part of the timing scales as sqrt(scale), so you
    //     extrapolate a thinned run to true light by that factor. ---
    G4double lysoScale = 1e-2;
    if (const char* s = std::getenv("RADSIMPLE_LYSO_SCALE")) {
        double v = std::atof(s); if (v > 0.) lysoScale = v;
    }
    auto yMPT = new G4MaterialPropertiesTable();
    yMPT->AddProperty("RINDEX",    phE, std::vector<G4double>(6, 1.81));
    yMPT->AddProperty("ABSLENGTH", phE, std::vector<G4double>(6, 40.*cm));
    yMPT->AddProperty("SCINTILLATIONCOMPONENT1", phE, {0.00,0.02,0.25,0.70,0.80,0.00}); // 420 nm peak
    yMPT->AddConstProperty("SCINTILLATIONYIELD",        33200./MeV * lysoScale);
    yMPT->AddConstProperty("RESOLUTIONSCALE",           1.0);
    yMPT->AddConstProperty("SCINTILLATIONTIMECONSTANT1", 36.*ns);
    yMPT->AddConstProperty("SCINTILLATIONYIELD1",        1.0);
    lyso->SetMaterialPropertiesTable(yMPT);
    G4cout << "[SIMPLE] LYSO yield scale " << lysoScale << " -> "
           << 33200.*lysoScale << " ph/MeV (RADSIMPLE_LYSO_SCALE)" << G4endl;

    // --- DSB1: PURE WLS SHIFTER (no self-scintillation, on purpose — keeps the
    //     detected light 100% LYSO->WLS, the clean chain). Absorbs blue (short
    //     WLSABSLENGTH at 400-449 nm, covering LYSO's 420 nm), re-emits green
    //     (WLSCOMPONENT peaked 495 nm), fast 3.5 ns. n=1.50. ---
    auto lMPT = new G4MaterialPropertiesTable();
    lMPT->AddProperty("RINDEX",       phE, std::vector<G4double>(6, 1.50));
    lMPT->AddProperty("ABSLENGTH",    phE, std::vector<G4double>(6, 1.*m));
    lMPT->AddProperty("WLSABSLENGTH", phE, {5.*m,5.*m,5.*m,2.*mm,2.*mm,5.*mm});     // eats blue
    lMPT->AddProperty("WLSCOMPONENT", phE, {0.08,0.45,1.00,0.20,0.02,0.00});        // emits green 495 nm
    lMPT->AddConstProperty("WLSTIMECONSTANT", 3.5*ns);
    dsb1->SetMaterialPropertiesTable(lMPT);

    // --- Tyvek: needs n so a boundary exists; reflectivity is set by the surface below. ---
    auto tMPT = new G4MaterialPropertiesTable();
    tMPT->AddProperty("RINDEX", phE, std::vector<G4double>(6, 1.50));
    tyvek->SetMaterialPropertiesTable(tMPT);
}

// -------------------------------------------------------------------------
// GEOMETRY.
// -------------------------------------------------------------------------
G4VPhysicalVolume* DetectorConstruction::Construct() {
    DefineMaterials();
    auto air    = G4Material::GetMaterial("G4_AIR");
    auto lyso   = G4Material::GetMaterial("LYSO");
    auto tung   = G4Material::GetMaterial("G4_W");
    auto tyvek  = G4Material::GetMaterial("Tyvek");
    auto quartz = G4Material::GetMaterial("G4_SILICON_DIOXIDE");
    auto dsb1   = G4Material::GetMaterial("DSB1");
    auto silic  = G4Material::GetMaterial("G4_Si");

    const G4double hXY = tileXY/2.*mm;

    // World.
    auto worldS = new G4Box("World", 40.*mm, 40.*mm, 110.*mm);
    auto worldLV = new G4LogicalVolume(worldS, air, "World");
    worldLV->SetVisAttributes(G4VisAttributes::GetInvisible());
    auto worldPV = new G4PVPlacement(nullptr, {}, worldLV, "World", nullptr, false, 0);

    // The 4 corner fibre (x,y) positions.
    const G4double c = cornerOff*mm;
    const G4ThreeVector corner[4] =
        { {+c,+c,0}, {+c,-c,0}, {-c,+c,0}, {-c,-c,0} };

    // Helper: drill the 4 corner holes through a plate solid (over-long in z).
    auto drill = [&](G4VSolid* s, const G4String& nm) -> G4VSolid* {
        auto bore = new G4Tubs(nm+"_bore", 0, holeR*mm, 200.*mm, 0, 360*deg);
        G4VSolid* out = s;
        for (int k = 0; k < 4; ++k)
            out = new G4SubtractionSolid(nm+"_d"+std::to_string(k), out, bore,
                                         nullptr, corner[k]);
        return out;
    };

    // Reflective Tyvek surface (98% diffuse) — applied to the Tyvek foils. Where
    // a foil is drilled (the 4 holes) there is no Tyvek, so scintillation light
    // passes into the fibres there; everywhere else it reflects back into LYSO.
    auto tyvekSurf = new G4OpticalSurface("Tyvek");
    tyvekSurf->SetType(dielectric_metal); tyvekSurf->SetFinish(ground);
    tyvekSurf->SetModel(unified);
    auto tsMPT = new G4MaterialPropertiesTable();
    tsMPT->AddProperty("REFLECTIVITY",
        {1.55*eV,3.54*eV}, {0.98,0.98});
    tyvekSurf->SetMaterialPropertiesTable(tsMPT);

    // Vis colours.
    auto lysoVis = new G4VisAttributes(G4Colour(0.3,0.5,1.0,0.3)); // blue
    auto wVis    = new G4VisAttributes(G4Colour(1.0,0.3,0.3,0.3)); // red
    wVis->SetForceSolid(true);
    auto dsbVis  = new G4VisAttributes(G4Colour(1.0,0.6,0.0));     // orange
    dsbVis->SetForceSolid(true);
    auto qVis    = new G4VisAttributes(G4Colour(0.7,0.9,1.0,0.4));
    auto pdVis   = new G4VisAttributes(G4Colour(1.0,1.0,0.0));     // yellow
    pdVis->SetForceSolid(true);

    // ---- Build the stack: LYSO | Tyvek | W | Tyvek | LYSO | ... ----
    // 57 plates (even index = LYSO, odd = W), 56 Tyvek foils between them.
    G4LogicalVolume* lysoLV = nullptr;              // keep one LYSO LV for the surface pair
    G4double z = -stackZ/2.*mm;
    const int nPlates = nLYSO + nW;                 // 57
    for (int i = 0; i < nPlates; ++i) {
        const bool isLyso = (i % 2 == 0);
        const G4double th = (isLyso ? lysoThick : wThick) * mm;
        z += th/2;
        auto solid = drill(new G4Box("plate", hXY, hXY, th/2), "plate");
        auto lv = new G4LogicalVolume(solid, isLyso ? lyso : tung,
                                      isLyso ? "LYSO" : "W");
        lv->SetVisAttributes(isLyso ? lysoVis : wVis);
        if (isLyso) lysoLV = lv;
        new G4PVPlacement(nullptr, G4ThreeVector(0,0,z), lv,
                          isLyso ? "LYSO" : "W", worldLV, false, i);
        z += th/2;
        if (i < nPlates - 1) {                      // Tyvek foil after this plate
            z += tyvekThick*mm/2;
            auto ts = drill(new G4Box("tyv", hXY, hXY, tyvekThick*mm/2), "tyv");
            auto tlv = new G4LogicalVolume(ts, tyvek, "Tyvek");
            tlv->SetVisAttributes(G4VisAttributes::GetInvisible());
            new G4LogicalSkinSurface("TyvekSurf", tlv, tyvekSurf);  // reflective
            new G4PVPlacement(nullptr, G4ThreeVector(0,0,z), tlv, "Tyvek",
                              worldLV, false, i);
            z += tyvekThick*mm/2;
        }
    }

    // ---- Build the 4 corner fibres: quartz | DSB1(15mm @ shower max) | quartz ----
    // z of the DSB1 centre = front + showerMaxDepth, measured from stack front.
    const G4double front = -stackZ/2.*mm;
    const G4double dsbC  = front + showerMaxDepth*mm;              // DSB1 centre z
    const G4double dsbHi = dsbC + wlsLen*mm/2;                    // downstream edge
    const G4double dsbLo = dsbC - wlsLen*mm/2;                    // upstream edge
    const G4double back  = +stackZ/2.*mm;

    const G4double upLen = dsbLo - front;                         // upstream quartz length
    const G4double dnLen = back - dsbHi;                          // downstream quartz length
    auto qUpS  = new G4Tubs("qUp",  0, fibreR*mm, upLen/2, 0, 360*deg);
    auto qDnS  = new G4Tubs("qDn",  0, fibreR*mm, dnLen/2, 0, 360*deg);
    auto dsbS  = new G4Tubs("dsb",  0, fibreR*mm, wlsLen*mm/2, 0, 360*deg);
    auto qUpLV = new G4LogicalVolume(qUpS, quartz, "QuartzUp");   qUpLV->SetVisAttributes(qVis);
    auto qDnLV = new G4LogicalVolume(qDnS, quartz, "QuartzDn");   qDnLV->SetVisAttributes(qVis);
    auto dsbLV = new G4LogicalVolume(dsbS, dsb1,   "DSB1");       dsbLV->SetVisAttributes(dsbVis);

    // Photodetectors (SiPMs): thin silicon discs at each stack end.
    const G4double pdHz = 0.05*mm;
    auto pdS   = new G4Tubs("pd", 0, fibreR*mm, pdHz, 0, 360*deg);
    auto pdUpLV = new G4LogicalVolume(pdS, silic, "PD_Up");   pdUpLV->SetVisAttributes(pdVis);
    auto pdDnLV = new G4LogicalVolume(pdS, silic, "PD_Down"); pdDnLV->SetVisAttributes(pdVis);

    for (int k = 0; k < 4; ++k) {
        G4ThreeVector p = corner[k];
        new G4PVPlacement(nullptr, p + G4ThreeVector(0,0,(front+dsbLo)/2), qUpLV, "QuartzUp",  worldLV, false, k);
        new G4PVPlacement(nullptr, p + G4ThreeVector(0,0,dsbC),            dsbLV, "DSB1",      worldLV, false, k);
        new G4PVPlacement(nullptr, p + G4ThreeVector(0,0,(dsbHi+back)/2),  qDnLV, "QuartzDn",  worldLV, false, k);
        new G4PVPlacement(nullptr, p + G4ThreeVector(0,0,front-pdHz),      pdUpLV,"PD_Up",     worldLV, false, k);
        new G4PVPlacement(nullptr, p + G4ThreeVector(0,0,back +pdHz),      pdDnLV,"PD_Down",   worldLV, false, k);
    }

    G4cout << "[SIMPLE] stack " << stackZ << " mm, DSB1 centre z = " << dsbC/mm
           << " mm; 4 corner fibres, PDs at both ends." << G4endl;
    return worldPV;
}
