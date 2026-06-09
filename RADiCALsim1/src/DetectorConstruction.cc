// RADiCAL module geometry
// Ref: Wetzel et al., arXiv:2303.05580 — beam test at Fermilab June 2022
//
// Stack: 29 LYSO tiles (1.5 mm) + 28 W tiles (2.5 mm), LYSO-W-LYSO-... pattern
// Each tile separated by 0.01 mm Tyvek sheet (56 sheets total)
// Capillaries: energy (EJ309 liquid) in center hole, timing (LuAG:Ce WLS) in 4 corners
// Housing: milled Delrin shell

#include "DetectorConstruction.hh"

#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4Box.hh"
#include "G4Tubs.hh"
#include "G4SubtractionSolid.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4OpticalSurface.hh"
#include "G4LogicalSkinSurface.hh"
#include <vector>

DetectorConstruction::DetectorConstruction() {}
DetectorConstruction::~DetectorConstruction() {}

G4VPhysicalVolume* DetectorConstruction::Construct() {

    // =========================================================================
    // 1. MATERIALS
    // =========================================================================
    auto nist = G4NistManager::Instance();

    G4Material* air     = nist->FindOrBuildMaterial("G4_AIR");
    G4Material* wMat    = nist->FindOrBuildMaterial("G4_W");
    G4Material* quartz  = nist->FindOrBuildMaterial("G4_SILICON_DIOXIDE");

    G4Element* Lu = nist->FindOrBuildElement("Lu");
    G4Element* Y  = nist->FindOrBuildElement("Y");
    G4Element* Si = nist->FindOrBuildElement("Si");
    G4Element* O  = nist->FindOrBuildElement("O");
    G4Element* C  = nist->FindOrBuildElement("C");
    G4Element* H  = nist->FindOrBuildElement("H");
    G4Element* Al = nist->FindOrBuildElement("Al");
    G4Element* Ce = nist->FindOrBuildElement("Ce");

    // LYSO:Ce — Lu1.8Y0.2SiO5:Ce, density 7.1 g/cm3
    // mass fractions from firstsim (Lu 71%, Y 4%, Si 6%, O 19%)
    G4Material* lyso = new G4Material("LYSO", 7.1*g/cm3, 4);
    lyso->AddElement(Lu, 71.0*perCent);
    lyso->AddElement(Y,   4.0*perCent);
    lyso->AddElement(Si,  6.0*perCent);
    lyso->AddElement(O,  19.0*perCent);

    // Tyvek — spunbonded HDPE, density 0.41 g/cm3
    G4Material* tyvek = new G4Material("Tyvek", 0.41*g/cm3, 1);
    tyvek->AddMaterial(nist->FindOrBuildMaterial("G4_POLYETHYLENE"), 100*perCent);

    // Delrin (POM, polyoxymethylene) — density 1.42 g/cm3, [CH2O]n
    G4Material* delrin = new G4Material("Delrin", 1.42*g/cm3, 3);
    delrin->AddElement(C, 40.0*perCent);
    delrin->AddElement(H,  6.7*perCent);
    delrin->AddElement(O, 53.3*perCent);

    // EJ309 organic liquid scintillator — density 0.959 g/cm3, H/C ~ 1.25
    // Approximated as C9H10 (phenyl ring + side chain)
    G4Material* ej309 = new G4Material("EJ309", 0.959*g/cm3, 2);
    ej309->AddElement(C, 91.2*perCent);
    ej309->AddElement(H,  8.8*perCent);

    // LuAG:Ce — Lu3Al5O12:Ce wavelength shifter, density 6.73 g/cm3
    G4Material* luag = new G4Material("LuAG_Ce", 6.73*g/cm3, 4);
    luag->AddElement(Lu, 61.5*perCent);
    luag->AddElement(Al, 15.8*perCent);
    luag->AddElement(O,  22.6*perCent);
    luag->AddElement(Ce,  0.1*perCent);

    // =========================================================================
    // 1b. OPTICAL PROPERTIES  (only quartz, LuAG:Ce, and air get tables, so
    //     optical photons are produced/propagated only in the timing capillary
    //     system + surrounding air — keeps photon counts tractable.)
    //
    //   Photon energy grid: ~350–800 nm (1.55–3.54 eV).
    // =========================================================================
    std::vector<G4double> phE = {1.55*eV, 2.07*eV, 2.48*eV, 2.76*eV, 3.10*eV, 3.54*eV};

    // --- Fused quartz: Cherenkov radiator + light guide ---
    std::vector<G4double> qRI  = {1.455, 1.457, 1.460, 1.462, 1.466, 1.472};
    std::vector<G4double> qABS = {10.*m, 10.*m, 10.*m, 10.*m, 8.*m, 5.*m};
    auto qMPT = new G4MaterialPropertiesTable();
    qMPT->AddProperty("RINDEX",    phE, qRI);
    qMPT->AddProperty("ABSLENGTH", phE, qABS);
    quartz->SetMaterialPropertiesTable(qMPT);

    // --- Air: RINDEX=1.0 so optical boundaries work (no Cherenkov: beta*n<1). ---
    std::vector<G4double> aRI = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    auto aMPT = new G4MaterialPropertiesTable();
    aMPT->AddProperty("RINDEX", phE, aRI);
    air->SetMaterialPropertiesTable(aMPT);

    // --- LuAG:Ce: scintillator. Emission peaks green ~520–540 nm (~2.3–2.4 eV),
    //     light yield ~22000 ph/MeV, decay ~60 ns (literature). ---
    std::vector<G4double> lRI  = {1.84, 1.84, 1.84, 1.84, 1.84, 1.84};
    std::vector<G4double> lABS = {1.*m, 1.*m, 1.*m, 1.*m, 1.*m, 1.*m};
    // emission spectrum (relative), peaked ~2.3–2.4 eV
    std::vector<G4double> lEM  = {0.05, 0.35, 1.00, 0.60, 0.10, 0.02};
    auto lMPT = new G4MaterialPropertiesTable();
    lMPT->AddProperty("RINDEX",                 phE, lRI);
    lMPT->AddProperty("ABSLENGTH",              phE, lABS);
    lMPT->AddProperty("SCINTILLATIONCOMPONENT1", phE, lEM);
    lMPT->AddConstProperty("SCINTILLATIONYIELD",        22000./MeV);
    lMPT->AddConstProperty("RESOLUTIONSCALE",           1.0);
    lMPT->AddConstProperty("SCINTILLATIONTIMECONSTANT1", 60.*ns);
    lMPT->AddConstProperty("SCINTILLATIONYIELD1",        1.0);
    luag->SetMaterialPropertiesTable(lMPT);

    // =========================================================================
    // 2. KEY DIMENSIONS
    // =========================================================================

    // Tile cross-section
    static const G4double tileX = 14.0*mm;
    static const G4double tileY = 14.0*mm;

    // Layer thicknesses
    static const G4double lysoThick      = 1.5*mm;
    static const G4double wThick         = 2.5*mm;
    static const G4double tyvekSliceThick = 0.01*mm;  // inter-layer Tyvek

    // Stack: 29 LYSO + 28 W + 56 Tyvek slices
    static const G4int nLYSO  = 29;
    static const G4int nW     = 28;
    static const G4int nTiles = nLYSO + nW;   // 57
    static const G4int nInter = nTiles - 1;   // 56 inter-layer sheets
    static const G4double stackZ = nLYSO*lysoThick + nW*wThick + nInter*tyvekSliceThick;
    // = 43.5 + 70.0 + 0.56 = 114.06 mm

    // Capillary hole radii in tiles (paper dimensions)
    static const G4double centerHoleR = 0.45*mm;  // 0.9 mm diameter
    static const G4double cornerHoleR = 0.65*mm;  // 1.3 mm diameter

    // Corner capillary positions (±4.5 mm from center)
    static const G4double capOff = 4.5*mm;
    const G4ThreeVector capXY[5] = {
        {0,       0,      0},
        {+capOff, +capOff, 0},
        {-capOff, +capOff, 0},
        {+capOff, -capOff, 0},
        {-capOff, -capOff, 0},
    };

    // Energy capillary (center) — OD scaled to fit 0.9 mm hole
    // Paper: OD=1000 µm, bore=400 µm; scaled to fit: OD=0.88 mm, bore=0.352 mm
    static const G4double eCap_outR = centerHoleR;   // fully fill the 0.45 mm drilled hole
    static const G4double eCap_boreR = 0.200*mm;

    // Timing capillary (corners) — paper: OD=1150 µm, bore=950 µm, fiber=900 µm
    static const G4double tCap_outR  = 0.575*mm;   // 1.15 mm OD
    static const G4double tCap_boreR = 0.475*mm;   // 0.95 mm bore
    static const G4double wlsFiberR  = 0.450*mm;   // 0.9 mm LuAG:Ce fiber

    // Timing capillary segmentation — shower max measured at LYSO layer ~10–11.
    // Center of LYSO layer L sits at L*4.02 + 0.75 mm from the upstream face, so
    // the peak (layer ~10.5) is at ~43 mm — NOT the geometric middle (layer 14).
    // WLS section is centered on shower max (upstreamLen = showerMaxDepth − wlsLen/2),
    // so changing wlsLen keeps it bracketing shower max automatically.
    // (Beam travels +z: "upstream" = −z end, "downstream" = +z end.)
    static const G4double showerMaxDepth = 43.0*mm;  // measured peak, layers ~10–11
    static const G4double wlsLen         = 6.0*mm;   // LuAG:Ce WLS section (40–46 mm)
    static const G4double upstreamLen    = showerMaxDepth - wlsLen/2.0;       // 40.0 mm
    static const G4double downstreamLen  = stackZ - upstreamLen - wlsLen;     // 68.06 mm

    // Z centers of timing cap segments relative to calo center
    static const G4double z_upstream   = -stackZ/2.0 + upstreamLen/2.0;
    static const G4double z_wls        = -stackZ/2.0 + upstreamLen + wlsLen/2.0;
    static const G4double z_downstream = -stackZ/2.0 + upstreamLen + wlsLen + downstreamLen/2.0;

    // Delrin housing — 18 mm × 18 mm outer, 14 mm × 14 mm inner cavity
    static const G4double housingOuterHalf = 9.0*mm;
    static const G4double housingInnerHalf = 7.0*mm;
    static const G4double housingHalfZ     = 65.0*mm;   // 130 mm = 13 cm ✓
    static const G4double cavityHalfZ      = stackZ/2.0 + 0.05*mm; // slight clearance

    // =========================================================================
    // 3. WORLD
    // =========================================================================
    // World enlarged to host the full CERN test-beam line (trigger + MCP
    // upstream, Pb-glass downstream). RADiCAL module stays centered at z=0.
    auto solidWorld = new G4Box("World", 120.0*mm, 120.0*mm, 650.0*mm);
    auto logicWorld = new G4LogicalVolume(solidWorld, air, "World");
    auto physWorld  = new G4PVPlacement(nullptr, {}, logicWorld, "World", nullptr, false, 0);
    logicWorld->SetVisAttributes(G4VisAttributes::GetInvisible());

    // =========================================================================
    // 4. DELRIN HOUSING
    // =========================================================================
    auto solidDelrinOuter = new G4Box("Delrin_Outer", housingOuterHalf, housingOuterHalf, housingHalfZ);
    auto solidDelrinInner = new G4Box("Delrin_Inner", housingInnerHalf, housingInnerHalf, cavityHalfZ);
    auto solidDelrin      = new G4SubtractionSolid("Delrin", solidDelrinOuter, solidDelrinInner);
    auto logicDelrin      = new G4LogicalVolume(solidDelrin, delrin, "Delrin");
    new G4PVPlacement(nullptr, {}, logicDelrin, "Delrin_Phys", logicWorld, false, 0);

    // Housing as faint wireframe so the interior is fully visible from outside
    auto delrinVis = new G4VisAttributes(G4Colour(0.82, 0.71, 0.55, 0.4));
    delrinVis->SetForceWireframe(true);
    delrinVis->SetForceAuxEdgeVisible(true);
    logicDelrin->SetVisAttributes(delrinVis);

    // =========================================================================
    // 5. CALORIMETER CONTAINER (inner cavity — air envelope)
    // =========================================================================
    auto solidCalo = new G4Box("Calo", housingInnerHalf, housingInnerHalf, cavityHalfZ);
    auto logicCalo = new G4LogicalVolume(solidCalo, air, "Calo");
    new G4PVPlacement(nullptr, {}, logicCalo, "Calo_Phys", logicWorld, false, 0);
    logicCalo->SetVisAttributes(G4VisAttributes::GetInvisible());

    // =========================================================================
    // 6. BUILD DRILLED TILE SOLIDS (shared logical volumes for efficiency)
    //
    //  Drill tool — long enough to pierce any tile or Tyvek slice
    // =========================================================================
    auto drillHalf = 10.0*mm;  // longer than any tile

    // Center drill (0.9 mm hole)
    auto drill_center = new G4Tubs("Drill_Center", 0, centerHoleR, drillHalf, 0., 360.*deg);
    // Corner drill (1.3 mm hole)
    auto drill_corner = new G4Tubs("Drill_Corner", 0, cornerHoleR, drillHalf, 0., 360.*deg);

    auto DoDrills = [&](G4VSolid* base) -> G4VSolid* {
        base = new G4SubtractionSolid("d0", base, drill_center, nullptr, capXY[0]);
        for (int c = 1; c < 5; c++)
            base = new G4SubtractionSolid("dc", base, drill_corner, nullptr, capXY[c]);
        return base;
    };

    // LYSO tile solid
    auto solidLYSOBase = new G4Box("LYSO_Base", tileX/2, tileY/2, lysoThick/2);
    auto solidLYSO     = DoDrills(solidLYSOBase);
    auto logicLYSO     = new G4LogicalVolume(solidLYSO, lyso, "LYSO");

    // W tile solid
    auto solidWBase = new G4Box("W_Base", tileX/2, tileY/2, wThick/2);
    auto solidW     = DoDrills(solidWBase);
    auto logicW     = new G4LogicalVolume(solidW, wMat, "W_Absorber");

    // Inter-layer Tyvek slice solid
    auto solidTyvekBase = new G4Box("Tyvek_Base", tileX/2, tileY/2, tyvekSliceThick/2);
    auto solidTyvekSlice = DoDrills(solidTyvekBase);
    auto logicTyvekSlice = new G4LogicalVolume(solidTyvekSlice, tyvek, "Tyvek_Slice");

    // Visualisation — wireframe outlines so every tile edge is visible
    auto lysoVis = new G4VisAttributes(G4Colour(0.0, 0.6, 0.9, 1.0));   // blue
    lysoVis->SetForceWireframe(true);
    lysoVis->SetForceAuxEdgeVisible(true);
    logicLYSO->SetVisAttributes(lysoVis);

    auto wVis = new G4VisAttributes(G4Colour(0.7, 0.3, 0.3, 1.0));      // red (absorber)
    wVis->SetForceWireframe(true);
    wVis->SetForceAuxEdgeVisible(true);
    logicW->SetVisAttributes(wVis);

    auto tyvekSliceVis = new G4VisAttributes(G4Colour(0.9, 0.9, 0.9, 0.6)); // white
    tyvekSliceVis->SetForceWireframe(true);
    tyvekSliceVis->SetForceAuxEdgeVisible(true);
    logicTyvekSlice->SetVisAttributes(tyvekSliceVis);

    // =========================================================================
    // 7. PLACE TILE STACK
    //
    //  Pattern: LYSO(0) | Tyvek | W(0) | Tyvek | LYSO(1) | Tyvek | W(1) | ...
    //           ... | Tyvek | W(27) | Tyvek | LYSO(28)
    //  Even tile index → LYSO,  Odd tile index → W
    // =========================================================================
    G4double z = -stackZ / 2.0;
    G4int lysoCount = 0, wCount = 0, tyvekCount = 0;

    for (G4int t = 0; t < nTiles; t++) {
        bool isLYSO = (t % 2 == 0);
        G4double thick = isLYSO ? lysoThick : wThick;
        G4double zc = z + thick / 2.0;

        if (isLYSO) {
            new G4PVPlacement(nullptr, G4ThreeVector(0,0,zc),
                              logicLYSO, "LYSO_Phys", logicCalo, false, lysoCount++);
        } else {
            new G4PVPlacement(nullptr, G4ThreeVector(0,0,zc),
                              logicW, "W_Phys", logicCalo, false, wCount++);
        }
        z += thick;

        // Place inter-layer Tyvek after every tile except the last
        if (t < nTiles - 1) {
            G4double ztc = z + tyvekSliceThick / 2.0;
            new G4PVPlacement(nullptr, G4ThreeVector(0,0,ztc),
                              logicTyvekSlice, "TyvekSlice_Phys", logicCalo, false, tyvekCount++);
            z += tyvekSliceThick;
        }
    }

    // =========================================================================
    // 8. CAPILLARIES
    //
    //  CENTER (energy): quartz tube + EJ309 liquid bore, full stack length
    //  CORNERS (timing): quartz upstream/downstream rods + quartz tube mid-section
    //                    + LuAG:Ce WLS fiber at shower max
    // =========================================================================

    // --- Center energy capillary ---
    auto solidECapTube = new G4Tubs("ECapTube", eCap_boreR, eCap_outR, stackZ/2, 0., 360.*deg);
    auto solidECapBore = new G4Tubs("ECapBore", 0,          eCap_boreR, stackZ/2, 0., 360.*deg);

    auto logicECapTube = new G4LogicalVolume(solidECapTube, quartz,  "Cap_Center_Tube");
    auto logicECapBore = new G4LogicalVolume(solidECapBore, ej309,   "Cap_Center_EJ309");

    new G4PVPlacement(nullptr, capXY[0], logicECapTube, "ECapTube_Phys", logicCalo, false, 0);
    new G4PVPlacement(nullptr, capXY[0], logicECapBore, "ECapBore_Phys", logicCalo, false, 0);

    // EJ309 bore kept solid (key active volume); quartz tube as outline
    auto eCapVis = new G4VisAttributes(G4Colour(0.0, 0.9, 0.0, 0.9));
    eCapVis->SetForceSolid(true);
    eCapVis->SetForceAuxEdgeVisible(true);
    logicECapBore->SetVisAttributes(eCapVis);
    auto eCapTubeVis = new G4VisAttributes(G4Colour(0.8, 0.8, 1.0, 0.8));
    eCapTubeVis->SetForceWireframe(true);
    eCapTubeVis->SetForceAuxEdgeVisible(true);
    logicECapTube->SetVisAttributes(eCapTubeVis);

    // --- Corner timing capillaries (shared logical volumes) ---
    // Front rod: solid quartz cylinder (fills bore + wall region)
    auto solidTFront = new G4Tubs("TCapFront", 0, tCap_outR, frontLen/2, 0., 360.*deg);
    auto logicTFront = new G4LogicalVolume(solidTFront, quartz, "Cap_Corner_Front");

    // Middle WLS section: quartz tube wall
    auto solidTMidTube = new G4Tubs("TCapMidTube", wlsFiberR, tCap_outR, wlsLen/2, 0., 360.*deg);
    auto logicTMidTube = new G4LogicalVolume(solidTMidTube, quartz, "Cap_Corner_MidTube");

    // Middle WLS section: DSB1 polymer fiber (scoring volume for timing)
    auto solidTMidWLS = new G4Tubs("TCapMidWLS", 0, wlsFiberR, wlsLen/2, 0., 360.*deg);
    auto logicTMidWLS = new G4LogicalVolume(solidTMidWLS, luag, "Cap_Corner_WLS");

    // Back rod: solid quartz cylinder
    auto solidTBack = new G4Tubs("TCapBack", 0, tCap_outR, backLen/2, 0., 360.*deg);
    auto logicTBack = new G4LogicalVolume(solidTBack, quartz, "Cap_Corner_Back");

    // Quartz timing rods/tube as outlines
    auto tFrontBackVis = new G4VisAttributes(G4Colour(0.7, 0.9, 1.0, 0.7));
    tFrontBackVis->SetForceWireframe(true);
    tFrontBackVis->SetForceAuxEdgeVisible(true);
    logicTFront->SetVisAttributes(tFrontBackVis);
    logicTBack->SetVisAttributes(tFrontBackVis);
    logicTMidTube->SetVisAttributes(tFrontBackVis);

    // LuAG:Ce WLS fiber kept solid (key timing active volume)
    auto wlsVis = new G4VisAttributes(G4Colour(1.0, 0.6, 0.0, 0.95));
    wlsVis->SetForceSolid(true);
    wlsVis->SetForceAuxEdgeVisible(true);
    logicTMidWLS->SetVisAttributes(wlsVis);

    // --- Photodetectors at the front & back ends of each timing capillary ---
    // Thin Si pads, abutting the quartz rod ends, that detect optical photons
    // (SteppingAction applies the quantum efficiency and records arrival time).
    G4Material* siPD = nist->FindOrBuildMaterial("G4_Si");
    std::vector<G4double> pdRI  = {1.50, 1.50, 1.50, 1.50, 1.50, 1.50};
    std::vector<G4double> pdABS = {1.*um, 1.*um, 1.*um, 1.*um, 1.*um, 1.*um};
    auto pdMPT = new G4MaterialPropertiesTable();
    pdMPT->AddProperty("RINDEX",    phE, pdRI);   // so photons transmit in
    pdMPT->AddProperty("ABSLENGTH", phE, pdABS);  // then absorb immediately
    siPD->SetMaterialPropertiesTable(pdMPT);

    static const G4double pdHalfZ = 0.02*mm;
    auto solidPD = new G4Tubs("PD", 0, tCap_outR, pdHalfZ, 0., 360.*deg);
    auto logicPDFront = new G4LogicalVolume(solidPD, siPD, "PD_Front");
    auto logicPDBack  = new G4LogicalVolume(solidPD, siPD, "PD_Back");
    auto pdVis = new G4VisAttributes(G4Colour(1.0, 1.0, 0.0, 0.95));  // yellow
    pdVis->SetForceSolid(true);
    logicPDFront->SetVisAttributes(pdVis);
    logicPDBack->SetVisAttributes(pdVis);
    const G4double zPDFront = -stackZ/2.0 - pdHalfZ;   // abuts front rod end
    const G4double zPDBack  = +stackZ/2.0 + pdHalfZ;   // abuts back rod end

    for (G4int c = 1; c <= 4; c++) {
        G4ThreeVector xy = capXY[c];

        new G4PVPlacement(nullptr, xy + G4ThreeVector(0, 0, z_front),
                          logicTFront, "TCapFront_Phys", logicCalo, false, c-1);

        new G4PVPlacement(nullptr, xy + G4ThreeVector(0, 0, z_wls),
                          logicTMidTube, "TCapMidTube_Phys", logicCalo, false, c-1);

        new G4PVPlacement(nullptr, xy + G4ThreeVector(0, 0, z_wls),
                          logicTMidWLS, "TCapMidWLS_Phys", logicCalo, false, c-1);

        new G4PVPlacement(nullptr, xy + G4ThreeVector(0, 0, z_back),
                          logicTBack, "TCapBack_Phys", logicCalo, false, c-1);

        // photodetectors (copy number = corner index 0..3)
        new G4PVPlacement(nullptr, xy + G4ThreeVector(0, 0, zPDFront),
                          logicPDFront, "PDFront_Phys", logicCalo, false, c-1);
        new G4PVPlacement(nullptr, xy + G4ThreeVector(0, 0, zPDBack),
                          logicPDBack,  "PDBack_Phys",  logicCalo, false, c-1);
    }

    // =========================================================================
    // 9. CERN TEST-BEAM LINE
    //    Beam travels +z.  Upstream:  Trig1 -> Trig2 -> MCP
    //    Center:      RADiCAL module (z = 0, built above)
    //    Downstream:  Pb-glass calorimeter
    //    NOTE: dimensions/distances are standard test-beam defaults (a photo
    //    gives no exact metrology) — all gathered here for easy correction.
    // =========================================================================
    G4Material* plasticScint = nist->FindOrBuildMaterial("G4_PLASTIC_SC_VINYLTOLUENE");
    G4Material* alumina      = nist->FindOrBuildMaterial("G4_ALUMINUM_OXIDE");
    G4Material* pbGlass      = nist->FindOrBuildMaterial("G4_GLASS_LEAD");

    // --- Two coincidence trigger scintillators (define the beam particle) ---
    static const G4double trigHalfXY = 15.0*mm;   // 30 x 30 mm paddle
    static const G4double trigHalfZ  = 2.5*mm;    // 5 mm thick
    static const G4double z_trig1    = -400.0*mm;
    static const G4double z_trig2    = -350.0*mm;
    auto solidTrig  = new G4Box("Trig", trigHalfXY, trigHalfXY, trigHalfZ);
    auto logicTrig1 = new G4LogicalVolume(solidTrig, plasticScint, "Trig1");
    auto logicTrig2 = new G4LogicalVolume(solidTrig, plasticScint, "Trig2");
    new G4PVPlacement(nullptr, G4ThreeVector(0,0,z_trig1), logicTrig1, "Trig1_Phys", logicWorld, false, 0);
    new G4PVPlacement(nullptr, G4ThreeVector(0,0,z_trig2), logicTrig2, "Trig2_Phys", logicWorld, false, 0);

    auto trigVis = new G4VisAttributes(G4Colour(0.2, 0.85, 0.2, 0.5));
    trigVis->SetForceSolid(true);
    logicTrig1->SetVisAttributes(trigVis);
    logicTrig2->SetVisAttributes(trigVis);

    // --- MCP-PMT timing reference (~10 ps): fused-silica Cherenkov window
    //     (timing/scoring volume) backed by a thin alumina body. Kept low
    //     material budget so it does not pre-shower the beam (<0.05 X0). ---
    static const G4double mcpHalfXY   = 13.5*mm;  // 27 x 27 mm active area
    static const G4double mcpWinHalfZ = 1.5*mm;   // 3 mm fused silica
    static const G4double mcpBodyHalfZ = 1.5*mm;  // 3 mm alumina body
    static const G4double z_mcp        = -250.0*mm;
    auto solidMCPwin = new G4Box("MCPwin", mcpHalfXY, mcpHalfXY, mcpWinHalfZ);
    auto logicMCPwin = new G4LogicalVolume(solidMCPwin, quartz, "MCP_Radiator");
    new G4PVPlacement(nullptr, G4ThreeVector(0,0,z_mcp), logicMCPwin, "MCP_Win_Phys", logicWorld, false, 0);

    auto solidMCPbody = new G4Box("MCPbody", mcpHalfXY, mcpHalfXY, mcpBodyHalfZ);
    auto logicMCPbody = new G4LogicalVolume(solidMCPbody, alumina, "MCP_Body");
    new G4PVPlacement(nullptr, G4ThreeVector(0,0,z_mcp + mcpWinHalfZ + mcpBodyHalfZ),
                      logicMCPbody, "MCP_Body_Phys", logicWorld, false, 0);

    auto mcpWinVis = new G4VisAttributes(G4Colour(0.6, 0.9, 1.0, 0.85));
    mcpWinVis->SetForceSolid(true);
    logicMCPwin->SetVisAttributes(mcpWinVis);
    auto mcpBodyVis = new G4VisAttributes(G4Colour(0.35, 0.35, 0.4, 0.6));
    mcpBodyVis->SetForceSolid(true);
    logicMCPbody->SetVisAttributes(mcpBodyVis);

    // --- Downstream Pb-glass calorimeter (tail catcher / total-absorption) ---
    static const G4double pbgHalfXY = 50.0*mm;    // 100 x 100 mm
    static const G4double pbgHalfZ  = 200.0*mm;   // 400 mm (~30 X0 of lead glass)
    static const G4double pbgFront  = 120.0*mm;   // front face 120 mm behind module center
    static const G4double z_pbg     = pbgFront + pbgHalfZ;
    auto solidPbg = new G4Box("PbGlassBox", pbgHalfXY, pbgHalfXY, pbgHalfZ);
    auto logicPbg = new G4LogicalVolume(solidPbg, pbGlass, "PbGlass");
    new G4PVPlacement(nullptr, G4ThreeVector(0,0,z_pbg), logicPbg, "PbGlass_Phys", logicWorld, false, 0);

    auto pbgVis = new G4VisAttributes(G4Colour(0.7, 0.85, 0.95, 0.22));
    pbgVis->SetForceSolid(true);
    logicPbg->SetVisAttributes(pbgVis);

    return physWorld;
}
