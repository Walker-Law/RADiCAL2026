// RADiCAL module geometry
// Refs: [1] Beresovskyi et al., arXiv:2303.05580 (2023) — beam test at Fermilab
//       [2] Perez-Lara et al., arXiv:2401.01747, NIM A 1068 (2024) 169737 — geometry reference
//
// Stack: 29 LYSO tiles (1.5 mm) + 28 W tiles (2.5 mm), LYSO-W-LYSO-... pattern
// Each tile separated by 0.2032 mm (0.008") Tyvek sheet [2] §2 (56 sheets = 124.88 mm total)
// Capillaries: energy (EJ309 liquid) in center hole, timing (DSB1 WLS, 15 mm) in 4 corners
// Corner offset: 3.5 mm from tile center [2] Fig. 2; WLS at shower max ~40.4 mm depth [2] Fig. 7
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
#include <cstdlib>

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

    // LYSO:Ce — Lu1.8Y0.2SiO5:Ce, density 7.1 g/cm3 (Saint-Gobain/Luxium datasheet,
    // verified). Mass fractions computed from the molecular formula (M=440.82):
    // Lu 71.45%, Y 4.03%, Si 6.37%, O 18.15% (Ce dopant <0.2 mol%, omitted).
    // OPTICALLY ACTIVE as of July 2026 (see section 1b): 33200 ph/MeV (scaled by
    // RADICAL_LYSO_SCINT_SCALE), 36 ns decay, 420 nm emission, n=1.81 — feeding
    // DSB1's 425 nm WLS absorption band. Energy histograms (H1[1] TotalLYSO etc.)
    // remain raw dE/dx energy deposits, independent of the optical yield scale.
    G4Material* lyso = new G4Material("LYSO", 7.1*g/cm3, 4);
    lyso->AddElement(Lu, 71.45*perCent);
    lyso->AddElement(Y,   4.03*perCent);
    lyso->AddElement(Si,  6.37*perCent);
    lyso->AddElement(O,  18.15*perCent);

    // Tyvek — spunbonded HDPE, density 0.41 g/cm3
    G4Material* tyvek = new G4Material("Tyvek", 0.41*g/cm3, 1);
    tyvek->AddMaterial(nist->FindOrBuildMaterial("G4_POLYETHYLENE"), 100*perCent);

    // Delrin (POM, polyoxymethylene) — density 1.42 g/cm3, [CH2O]n
    G4Material* delrin = new G4Material("Delrin", 1.42*g/cm3, 3);
    delrin->AddElement(C, 40.0*perCent);
    delrin->AddElement(H,  6.7*perCent);
    delrin->AddElement(O, 53.3*perCent);

    // EJ309 organic liquid scintillator — density 0.959 g/cm3 (Eljen datasheet,
    // verified), H/C ~ 1.25. Approximated as C9H10 (phenyl ring + side chain).
    // Real EJ309 (verified): ~11500 ph/MeV (75% anthracene), 3.5 ns primary decay,
    // 424 nm emission peak. NOT optically active in this sim (no MPT set on
    // `ej309`) — H1[4] CenterCapEnergy is raw energy deposit (MeV), not a real
    // photoelectron yield; the center capillary is an energy-only readout proxy.
    G4Material* ej309 = new G4Material("EJ309", 0.959*g/cm3, 2);
    ej309->AddElement(C, 91.2*perCent);
    ej309->AddElement(H,  8.8*perCent);

    // DSB1 — radiation-hard polysiloxane scintillating WLS (RADiCAL timing fiber,
    // arXiv:2401.01747). Modeled as a PDMS-like polysiloxane [SiO(CH3)2]n.
    // NOTE: density/composition are literature estimates for a polysiloxane WLS —
    // verify against the DSB1 datasheet if exact stoichiometry matters.
    G4Material* dsb1 = new G4Material("DSB1", 1.05*g/cm3, 4);
    dsb1->AddElement(Si, 37.9*perCent);
    dsb1->AddElement(O,  21.6*perCent);
    dsb1->AddElement(C,  32.4*perCent);
    dsb1->AddElement(H,   8.1*perCent);

    // =========================================================================
    // 1b. OPTICAL PROPERTIES  (quartz, DSB1, air, Tyvek, and — as of July 2026 —
    //     LYSO carry tables. LYSO's yield is heavily scaled down via
    //     RADICAL_LYSO_SCINT_SCALE to keep photon counts tractable; the OpWLS
    //     re-emission population in the fiber is the realistic signal path.)
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

    // --- DSB1: fast polysiloxane WLS. Measured properties (RADiCAL):
    //       absorption peak  λ = 425 nm (2.917 eV)  — matches LYSO ~420 nm emission
    //       emission peak    λ = 495 nm (2.505 eV)
    //       fluorescence decay τ = 3.5 ns
    //     The FAST 3.5 ns decay (vs LuAG:Ce's 60 ns) is what gives the sharp
    //     leading edge behind the RADiCAL paper's ~27 ps σ_t. RINDEX ~1.50.
    //     NOTE: the sim models the fiber as a self-scintillator (light ∝ dE/dx in
    //     the fiber), so the 425 nm WLS *absorption* band is documented here but
    //     only becomes functional once LYSO 420 nm optical photons are propagated
    //     into the fiber (a future upgrade). Emission spectrum + decay ARE used. ---
    std::vector<G4double> lRI  = {1.50, 1.50, 1.50, 1.50, 1.50, 1.50};
    std::vector<G4double> lABS = {1.*m, 1.*m, 1.*m, 1.*m, 1.*m, 1.*m};
    // emission spectrum (relative), peaked at 495 nm (2.505 eV ≈ the 2.48 eV grid
    // point); Stokes-shifted to the long-wavelength side of the 425 nm absorption.
    // grid:  1.55   2.07   2.48   2.76   3.10   3.54  eV
    //  (nm)   800    599    500    449    400    350
    std::vector<G4double> lEM  = {0.08, 0.45, 1.00, 0.20, 0.02, 0.00};
    // WLS absorption band, strong at 425 nm (2.917 eV ≈ 2.76–3.10 grid). Provided
    // for the true-WLS path (needs G4OpWLS + LYSO optical light); short length at
    // 425 nm, transparent to its own 495 nm emission (good Stokes separation).
    std::vector<G4double> lWLSABS = {5.*m, 5.*m, 5.*m, 2.*mm, 2.*mm, 5.*mm};
    auto lMPT = new G4MaterialPropertiesTable();
    lMPT->AddProperty("RINDEX",                 phE, lRI);
    lMPT->AddProperty("ABSLENGTH",              phE, lABS);
    lMPT->AddProperty("SCINTILLATIONCOMPONENT1", phE, lEM);
    lMPT->AddProperty("WLSABSLENGTH",           phE, lWLSABS);
    lMPT->AddProperty("WLSCOMPONENT",           phE, lEM);
    // DSB1 light yield, with an optional speed/statistics knob.
    // RADICAL_SCINT_YIELD scales the nominal 10000 ph/MeV (default 1.0). Cutting
    // it (e.g. 0.1, 0.01) tracks proportionally fewer scintillation photons ->
    // faster, fewer p.e.; recover true-yield timing via the sqrt(N) scaling.
    G4double scintScale = 1.0;
    if (const char* s = std::getenv("RADICAL_SCINT_YIELD")) {
        double v = std::atof(s);
        if (v > 0.) scintScale = v;
    }
    // RADICAL_DSB_DECAY_NS overrides the decay constant (default 3.5 ns, measured)
    // so you can sweep σ_t vs scintillator speed without recompiling.
    G4double dsbDecayNs = 3.5;
    if (const char* d = std::getenv("RADICAL_DSB_DECAY_NS")) {
        double v = std::atof(d);
        if (v > 0.) dsbDecayNs = v;
    }
    lMPT->AddConstProperty("SCINTILLATIONYIELD",        10000./MeV * scintScale);
    lMPT->AddConstProperty("RESOLUTIONSCALE",           1.0);
    lMPT->AddConstProperty("SCINTILLATIONTIMECONSTANT1", dsbDecayNs*ns);
    lMPT->AddConstProperty("SCINTILLATIONYIELD1",        1.0);
    lMPT->AddConstProperty("WLSTIMECONSTANT",            dsbDecayNs*ns);
    dsb1->SetMaterialPropertiesTable(lMPT);

    // --- LYSO:Ce: OPTICALLY ACTIVE (realistic signal chain). Luxium datasheet:
    //     33200 ph/MeV, 36 ns decay, 420 nm emission peak, n = 1.81. Its 420 nm
    //     light lands in DSB1's 425 nm WLSABSLENGTH band -> absorbed in the
    //     fiber -> re-emitted at 495 nm by G4OpWLS (creator process "OpWLS") ->
    //     guided to the PDs. That OpWLS population IS the real RADiCAL signal.
    //     Full yield = ~5e8 photons/event at 120 GeV — intractable, so the
    //     yield is scaled by RADICAL_LYSO_SCINT_SCALE (default 1e-3 ->
    //     33.2 ph/MeV). Photostatistics extrapolate: the emission-jitter part
    //     of sigma_t scales as sqrt(scale); geometric floors do not.
    //     Cherenkov born in LYSO is killed in StackingAction (real device:
    //     ~0.1% of scint light; at scaled yield it would unphysically dominate
    //     — RADICAL_KEEP_LYSO_CHER=1 to keep it).
    G4double lysoScale = 1e-3;
    if (const char* ls = std::getenv("RADICAL_LYSO_SCINT_SCALE")) {
        double v = std::atof(ls);
        if (v > 0.) lysoScale = v;
    }
    std::vector<G4double> yRI (6, 1.81);
    std::vector<G4double> yABS(6, 40.*cm);   // bulk attenuation (lit. estimate)
    // emission spectrum peaked at 420 nm (between the 449/400 nm grid points)
    std::vector<G4double> yEM  = {0.00, 0.02, 0.25, 0.70, 0.80, 0.00};
    auto yMPT = new G4MaterialPropertiesTable();
    yMPT->AddProperty("RINDEX",                  phE, yRI);
    yMPT->AddProperty("ABSLENGTH",               phE, yABS);
    yMPT->AddProperty("SCINTILLATIONCOMPONENT1", phE, yEM);
    yMPT->AddConstProperty("SCINTILLATIONYIELD",         33200./MeV * lysoScale);
    yMPT->AddConstProperty("RESOLUTIONSCALE",            1.0);
    yMPT->AddConstProperty("SCINTILLATIONTIMECONSTANT1", 36.*ns);
    yMPT->AddConstProperty("SCINTILLATIONYIELD1",        1.0);
    lyso->SetMaterialPropertiesTable(yMPT);
    G4cout << "[RADiCAL] LYSO optical: ON, yield scale " << lysoScale << " -> "
           << 33200.*lysoScale << " ph/MeV (RADICAL_LYSO_SCINT_SCALE)" << G4endl;

    // --- Tyvek: add RINDEX so optical boundary physics activates at Tyvek faces.
    //     n ≈ 1.50 (HDPE). Reflectivity 98% is set via G4LogicalSkinSurface below.
    std::vector<G4double> tvRI = {1.50, 1.50, 1.50, 1.50, 1.50, 1.50};
    auto tvMPT = new G4MaterialPropertiesTable();
    tvMPT->AddProperty("RINDEX", phE, tvRI);
    tyvek->SetMaterialPropertiesTable(tvMPT);

    // =========================================================================
    // 2. KEY DIMENSIONS
    // =========================================================================

    // Tile cross-section
    static const G4double tileX = 14.0*mm;
    static const G4double tileY = 14.0*mm;

    // Layer thicknesses
    static const G4double lysoThick      = 1.5*mm;
    static const G4double wThick         = 2.5*mm;
    static const G4double tyvekSliceThick = 0.2032*mm; // 0.008" per arXiv:2401.01747 §2

    // Stack: 29 LYSO + 28 W + 56 Tyvek slices
    static const G4int nLYSO  = 29;
    static const G4int nW     = 28;
    static const G4int nTiles = nLYSO + nW;   // 57
    static const G4int nInter = nTiles - 1;   // 56 inter-layer sheets
    static const G4double stackZ = nLYSO*lysoThick + nW*wThick + nInter*tyvekSliceThick;
    // = 43.5 + 70.0 + 56*0.2032 = 124.88 mm  (arXiv:2401.01747: module ~135 mm incl. end cards)

    // Capillary hole radii in tiles (arXiv:2401.01747 Fig. 2 dimensions)
    static const G4double centerHoleR = 0.45*mm;  // 0.9 mm diameter
    static const G4double cornerHoleR = 0.65*mm;  // 1.3 mm diameter

    // Corner capillary positions: 3.5 mm from tile center, per arXiv:2401.01747 Fig. 2
    static const G4double capOff = 3.5*mm;
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
    static const G4double wlsFiberR  = 0.450*mm;   // 0.9 mm DSB1 fiber

    // Timing capillary segmentation.
    // With corrected Tyvek (0.2032 mm), one LYSO+W period = 1.5+2.5+2*0.2032 = 4.4064 mm.
    // Center of LYSO layer L from upstream face = L*4.4064 + 0.75 mm.
    // arXiv:2401.01747 Fig. 7: shower max at layers 8–10 for 20–30 GeV.
    // → WLS centered on layer 9: z = 9*4.4064 + 0.75 ≈ 40.4 mm from upstream face.
    // arXiv:2401.01747 §2: WLS filament length = 15 mm (DSB1 fiber, 900 µm diam).
    // (Beam travels +z: "upstream" = −z end, "downstream" = +z end.)
    static const G4double showerMaxDepth = 40.4*mm;  // layer 9 centre, arXiv:2401.01747 Fig. 7
    static const G4double wlsLen         = 15.0*mm;  // arXiv:2401.01747 §2: 15 mm WLS length
    static const G4double upstreamLen    = showerMaxDepth - wlsLen/2.0;       // 40.0 mm
    static const G4double downstreamLen  = stackZ - upstreamLen - wlsLen;     // 68.06 mm

    // Z centers of timing cap segments relative to calo center
    static const G4double z_upstream   = -stackZ/2.0 + upstreamLen/2.0;
    static const G4double z_wls        = -stackZ/2.0 + upstreamLen + wlsLen/2.0;
    static const G4double z_downstream = -stackZ/2.0 + upstreamLen + wlsLen + downstreamLen/2.0;

    // Photodetector half-thickness (defined early — also needed for cavity size)
    static const G4double pdHalfZ = 0.02*mm;

    // Tyvek outer wrap — one inter-layer sheet thickness on all 6 faces
    static const G4double wrapThick = tyvekSliceThick;  // 0.2032 mm

    // Delrin housing — 18 mm × 18 mm outer; inner cavity enlarged by wrapThick
    // to accommodate the Tyvek side panels between tiles and Delrin wall.
    static const G4double housingOuterHalf = 9.0*mm;
    static const G4double housingInnerHalf = 7.0*mm + wrapThick;  // 7.2032 mm
    static const G4double housingHalfZ     = 65.0*mm;   // 130 mm = 13 cm ✓
    // Cavity must fit: stack + Tyvek end caps (wrapThick each) + PDs (pdHalfZ each)
    static const G4double cavityHalfZ      = stackZ/2.0 + wrapThick + 2.0*pdHalfZ + 0.05*mm;

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

    // ── Tyvek optical surface: ~98% diffuse (Lambertian) white reflector ─────
    // dielectric_metal + ground finish → absorbed or diffusely reflected.
    auto tyvekSurface = new G4OpticalSurface("TyvekSurface");
    tyvekSurface->SetType(dielectric_metal);
    tyvekSurface->SetFinish(ground);
    tyvekSurface->SetModel(unified);
    std::vector<G4double> tvRefl(6, 0.98);
    auto tvSurfMPT = new G4MaterialPropertiesTable();
    tvSurfMPT->AddProperty("REFLECTIVITY", phE, tvRefl);
    tyvekSurface->SetMaterialPropertiesTable(tvSurfMPT);
    // Apply to inter-layer Tyvek slices
    new G4LogicalSkinSurface("TyvekSliceSurf", logicTyvekSlice, tyvekSurface);

    // ── Tyvek outer wrapper: 4 side panels ───────────────────────────────────
    // Hollow box (outer=housingInnerHalf, inner=tileX/2) fills the gap between
    // the 14×14 mm tile stack and the Delrin inner wall on all 4 lateral faces.
    auto solidWrapO = new G4Box("TyvekWrap_O", housingInnerHalf, housingInnerHalf, stackZ/2.0);
    auto solidWrapI = new G4Box("TyvekWrap_I", tileX/2, tileY/2, stackZ/2.0 + 1.0*mm);
    auto solidTyvekSides = new G4SubtractionSolid("TyvekSides", solidWrapO, solidWrapI);
    auto logicTyvekSides = new G4LogicalVolume(solidTyvekSides, tyvek, "TyvekSides");
    new G4PVPlacement(nullptr, {}, logicTyvekSides, "TyvekSides_Phys", logicCalo, false, 0);
    new G4LogicalSkinSurface("TyvekSidesSurf", logicTyvekSides, tyvekSurface);
    logicTyvekSides->SetVisAttributes(tyvekSliceVis);

    // ── Tyvek outer wrapper: upstream + downstream end caps ───────────────────
    // Thin discs with holes at all 5 capillary positions (drilled same as slices)
    // so quartz rods pass through unobstructed.
    auto ecDrillH = wrapThick;   // drill half-length > end cap half-z
    auto ecDrillC = new G4Tubs("ECDrill_C", 0, centerHoleR, ecDrillH, 0., 360.*deg);
    auto ecDrillK = new G4Tubs("ECDrill_K", 0, cornerHoleR, ecDrillH, 0., 360.*deg);
    G4VSolid* solidEC = new G4Box("TyvekEC_B", housingInnerHalf, housingInnerHalf, wrapThick/2.0);
    solidEC = new G4SubtractionSolid("ec0", solidEC, ecDrillC, nullptr, capXY[0]);
    for (int c = 1; c < 5; c++)
        solidEC = new G4SubtractionSolid("ecc", solidEC, ecDrillK, nullptr, capXY[c]);
    auto logicTyvekEC = new G4LogicalVolume(solidEC, tyvek, "TyvekEndCap");
    new G4PVPlacement(nullptr, G4ThreeVector(0, 0, -stackZ/2.0 - wrapThick/2.0),
                      logicTyvekEC, "TyvekECUp_Phys",  logicCalo, false, 0);
    new G4PVPlacement(nullptr, G4ThreeVector(0, 0, +stackZ/2.0 + wrapThick/2.0),
                      logicTyvekEC, "TyvekECDn_Phys",  logicCalo, false, 1);
    new G4LogicalSkinSurface("TyvekEndCapSurf", logicTyvekEC, tyvekSurface);
    logicTyvekEC->SetVisAttributes(tyvekSliceVis);

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
    //                    + DSB1 WLS fiber at shower max
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
    // HOLLOW quartz tubes (arXiv:2401.01747: "A RADiCAL capillary is a hollow
    // quartz tube"). Only the thin 0.1 mm wall (bore 0.475 -> outer 0.575 mm) is
    // quartz, so shower particles crossing a capillary radiate Cherenkov in
    // ~32% of the quartz path a SOLID rod would have given. The bore is air
    // (mother volume) and light propagates by TIR in the wall + the air lumen.
    // This is the PHYSICAL Cherenkov suppression — no artificial thinning needed
    // for the quartz-Cherenkov geometry (the earlier solid-rod model overstated
    // Cherenkov ~3x). Corrects the 2026-07-09 "solid capillary" misread.
    // Upstream tube: hollow quartz (air bore)
    auto solidTUpstream = new G4Tubs("TCapUpstream", tCap_boreR, tCap_outR, upstreamLen/2, 0., 360.*deg);
    auto logicTUpstream = new G4LogicalVolume(solidTUpstream, quartz, "Cap_Corner_Upstream");

    // Middle WLS section: quartz tube wall
    auto solidTMidTube = new G4Tubs("TCapMidTube", wlsFiberR, tCap_outR, wlsLen/2, 0., 360.*deg);
    auto logicTMidTube = new G4LogicalVolume(solidTMidTube, quartz, "Cap_Corner_MidTube");

    // Middle WLS section: DSB1 fiber (scoring volume for timing)
    auto solidTMidWLS = new G4Tubs("TCapMidWLS", 0, wlsFiberR, wlsLen/2, 0., 360.*deg);
    auto logicTMidWLS = new G4LogicalVolume(solidTMidWLS, dsb1, "Cap_Corner_WLS");

    // Downstream tube: hollow quartz (air bore)
    auto solidTDownstream = new G4Tubs("TCapDownstream", tCap_boreR, tCap_outR, downstreamLen/2, 0., 360.*deg);
    auto logicTDownstream = new G4LogicalVolume(solidTDownstream, quartz, "Cap_Corner_Downstream");

    // Explicit air-filled bore volumes (named, so they're identifiable in
    // diagnostics/vis — not currently treated specially by SteppingAction;
    // ordinary optical boundary physics applies, same as any air-quartz
    // interface). NOTE: a photon that fails TIR at the wall's inner surface
    // leaks in here; from the air side TIR back into the wall is impossible
    // (low->high index) — a July 2026 attempt to kill photons on entry (as a
    // tractability workaround) was REMOVED because it also killed genuine
    // near-axial signal photons transiting straight through the bore (the
    // DSB1 fiber radius 0.45mm and bore radius 0.475mm are close, so most
    // forward-going WLS light passes through here). If runs hang again, the
    // fix needs to distinguish "stuck bouncing" from "clean transit" (e.g. a
    // bounded per-track bore-residency counter), not an unconditional kill.
    auto solidBoreUpstream = new G4Tubs("TCapBoreUp", 0, tCap_boreR, upstreamLen/2, 0., 360.*deg);
    auto logicBoreUpstream = new G4LogicalVolume(solidBoreUpstream, air, "Cap_Corner_Bore");
    auto solidBoreDownstream = new G4Tubs("TCapBoreDown", 0, tCap_boreR, downstreamLen/2, 0., 360.*deg);
    auto logicBoreDownstream = new G4LogicalVolume(solidBoreDownstream, air, "Cap_Corner_Bore");

    // Quartz timing rods/tube as outlines
    auto tRodVis = new G4VisAttributes(G4Colour(0.7, 0.9, 1.0, 0.7));
    tRodVis->SetForceWireframe(true);
    tRodVis->SetForceAuxEdgeVisible(true);
    logicTUpstream->SetVisAttributes(tRodVis);
    logicTDownstream->SetVisAttributes(tRodVis);
    logicTMidTube->SetVisAttributes(tRodVis);

    // DSB1 WLS fiber kept solid (key timing active volume)
    auto wlsVis = new G4VisAttributes(G4Colour(1.0, 0.6, 0.0, 0.95));
    wlsVis->SetForceSolid(true);
    wlsVis->SetForceAuxEdgeVisible(true);
    logicTMidWLS->SetVisAttributes(wlsVis);

    // --- Photodetectors at the upstream & downstream ends of each capillary ---
    // Thin Si pads, abutting the quartz rod ends, that detect optical photons
    // (SteppingAction applies the quantum efficiency and records arrival time).
    G4Material* siPD = nist->FindOrBuildMaterial("G4_Si");
    std::vector<G4double> pdRI  = {1.50, 1.50, 1.50, 1.50, 1.50, 1.50};
    std::vector<G4double> pdABS = {1.*um, 1.*um, 1.*um, 1.*um, 1.*um, 1.*um};
    auto pdMPT = new G4MaterialPropertiesTable();
    pdMPT->AddProperty("RINDEX",    phE, pdRI);   // so photons transmit in
    pdMPT->AddProperty("ABSLENGTH", phE, pdABS);  // then absorb immediately
    siPD->SetMaterialPropertiesTable(pdMPT);

    auto solidPD = new G4Tubs("PD", 0, tCap_outR, pdHalfZ, 0., 360.*deg);
    auto logicPDUpstream   = new G4LogicalVolume(solidPD, siPD, "PD_Upstream");
    auto logicPDDownstream = new G4LogicalVolume(solidPD, siPD, "PD_Downstream");
    auto pdVis = new G4VisAttributes(G4Colour(1.0, 1.0, 0.0, 0.95));  // yellow
    pdVis->SetForceSolid(true);
    logicPDUpstream->SetVisAttributes(pdVis);
    logicPDDownstream->SetVisAttributes(pdVis);
    // PDs sit just outside the Tyvek end caps (quartz rod photons pass through the
    // drilled holes in the end caps and arrive at the Si PD through a thin air gap)
    const G4double zPDUpstream   = -stackZ/2.0 - wrapThick - pdHalfZ;
    const G4double zPDDownstream = +stackZ/2.0 + wrapThick + pdHalfZ;

    for (G4int c = 1; c <= 4; c++) {
        G4ThreeVector xy = capXY[c];

        new G4PVPlacement(nullptr, xy + G4ThreeVector(0, 0, z_upstream),
                          logicTUpstream, "TCapUpstream_Phys", logicCalo, false, c-1);
        new G4PVPlacement(nullptr, xy + G4ThreeVector(0, 0, z_upstream),
                          logicBoreUpstream, "TCapBoreUp_Phys", logicCalo, false, c-1);

        new G4PVPlacement(nullptr, xy + G4ThreeVector(0, 0, z_wls),
                          logicTMidTube, "TCapMidTube_Phys", logicCalo, false, c-1);

        new G4PVPlacement(nullptr, xy + G4ThreeVector(0, 0, z_wls),
                          logicTMidWLS, "TCapMidWLS_Phys", logicCalo, false, c-1);

        new G4PVPlacement(nullptr, xy + G4ThreeVector(0, 0, z_downstream),
                          logicTDownstream, "TCapDownstream_Phys", logicCalo, false, c-1);
        new G4PVPlacement(nullptr, xy + G4ThreeVector(0, 0, z_downstream),
                          logicBoreDownstream, "TCapBoreDown_Phys", logicCalo, false, c-1);

        // photodetectors (copy number = corner index 0..3)
        new G4PVPlacement(nullptr, xy + G4ThreeVector(0, 0, zPDUpstream),
                          logicPDUpstream, "PDUpstream_Phys", logicCalo, false, c-1);
        new G4PVPlacement(nullptr, xy + G4ThreeVector(0, 0, zPDDownstream),
                          logicPDDownstream, "PDDownstream_Phys", logicCalo, false, c-1);
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
    //     (timing/scoring volume) followed downstream by a thin alumina body.
    //     Kept low material budget so it does not pre-shower the beam (<0.05 X0). ---
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
    static const G4double pbgUpstreamFace = 120.0*mm;  // upstream face 120 mm downstream of module center
    static const G4double z_pbg           = pbgUpstreamFace + pbgHalfZ;
    auto solidPbg = new G4Box("PbGlassBox", pbgHalfXY, pbgHalfXY, pbgHalfZ);
    auto logicPbg = new G4LogicalVolume(solidPbg, pbGlass, "PbGlass");
    new G4PVPlacement(nullptr, G4ThreeVector(0,0,z_pbg), logicPbg, "PbGlass_Phys", logicWorld, false, 0);

    auto pbgVis = new G4VisAttributes(G4Colour(0.7, 0.85, 0.95, 0.22));
    pbgVis->SetForceSolid(true);
    logicPbg->SetVisAttributes(pbgVis);

    return physWorld;
}
