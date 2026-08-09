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

    // DISPERSION (2026-08-08). RINDEX gets its own finer grid: Geant4
    // propagates optical photons at the GROUP velocity it derives numerically
    // from n(E), so flat or coarsely-sampled RINDEX silently means "no
    // chromatic dispersion" — photons of every colour travel at c/n. At our
    // ~25 ps floor precision that is a real omission: the group index of
    // LYSO at its 420 nm emission is ~1.95 vs the phase index 1.82, i.e.
    // in-crystal light is ~7% slower than the flat table said, and the
    // emission band's group-index spread adds genuine arrival-time jitter
    // that the old tables could not produce. Same energy SPAN as phE on
    // purpose: the RINDEX range also defines the Cherenkov emission band,
    // and widening it would change the Cherenkov yield.
    const std::vector<G4double> phEfine =
        {1.5498*eV, 1.7712*eV, 1.9997*eV, 2.2140*eV, 2.4311*eV, 2.5830*eV,
         2.6953*eV, 2.8178*eV, 2.9173*eV, 3.0240*eV, 3.0996*eV, 3.1791*eV,
         3.2627*eV, 3.3509*eV, 3.4440*eV, 3.5424*eV};   // 800..350 nm, dense in blue

    // --- Fused quartz: transparent light guide (n ~ 1.46). ---
    // Malitson (1965) Sellmeier, evaluated on the fine grid. Derived group
    // index at 420 nm comes out ~1.508 — silica's blue-end dispersion is
    // strong, and the old 6-point table under-resolved it slightly.
    auto qMPT = new G4MaterialPropertiesTable();
    qMPT->AddProperty("RINDEX", phEfine,
        {1.4533, 1.4553, 1.4574, 1.4595, 1.4618, 1.4635,
         1.4648, 1.4663, 1.4676, 1.4691, 1.4701, 1.4713,
         1.4725, 1.4738, 1.4753, 1.4769});
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
    // RADSIMPLE_LIGHT_SCALE — coherent thinning of ALL light (this scales the
    // LYSO yield at the source; StackingAction applies the SAME factor to
    // Cherenkov). Old name RADSIMPLE_LYSO_SCALE still accepted.
    G4double lysoScale = 1e-2;
    if (const char* s = std::getenv("RADSIMPLE_LIGHT_SCALE")) {
        double v = std::atof(s); if (v > 0.) lysoScale = v;
    } else if (const char* s = std::getenv("RADSIMPLE_LYSO_SCALE")) {
        double v = std::atof(s); if (v > 0.) lysoScale = v;
    }
    auto yMPT = new G4MaterialPropertiesTable();
    // Effective single-pole Sellmeier n² = 1 + 2.0978·λ²/(λ² − 0.12796² µm²),
    // anchored to the two numbers that matter for timing: phase index
    // n(420 nm) = 1.820 (datasheet) and group index n_g(420 nm) ≈ 1.95
    // (literature, LSO light-transport measurements). CAVEAT: this effective
    // curve runs ~0.015 low in the red (n(633) = 1.785 vs ~1.80 measured) —
    // acceptable, since almost no detected light is red — and n_g ≈ 1.95
    // should be re-checked against a Sellmeier fit for our specific LYSO.
    yMPT->AddProperty("RINDEX", phEfine,
        {1.7756, 1.7805, 1.7864, 1.7926, 1.7996, 1.8051,
         1.8093, 1.8143, 1.8185, 1.8232, 1.8267, 1.8305,
         1.8347, 1.8392, 1.8442, 1.8497});
    yMPT->AddProperty("ABSLENGTH", phE, std::vector<G4double>(6, 40.*cm));
    yMPT->AddProperty("SCINTILLATIONCOMPONENT1", phE, {0.00,0.02,0.25,0.70,0.80,0.00}); // 420 nm peak
    yMPT->AddConstProperty("SCINTILLATIONYIELD",        33200./MeV * lysoScale);
    yMPT->AddConstProperty("RESOLUTIONSCALE",           1.0);
    yMPT->AddConstProperty("SCINTILLATIONTIMECONSTANT1", 36.*ns);
    yMPT->AddConstProperty("SCINTILLATIONYIELD1",        1.0);
    lyso->SetMaterialPropertiesTable(yMPT);
    G4cout << "[SIMPLE] light scale " << lysoScale << " -> "
           << 33200.*lysoScale << " ph/MeV LYSO, Cherenkov thinned to match"
           << " (RADSIMPLE_LIGHT_SCALE)" << G4endl;

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

// On/off flags for optional components. Each reads an environment variable
// once; "0" disables, anything else (or unset, if dflt=true) enables.
static bool flagOn(const char* envName, bool dflt) {
    const char* s = std::getenv(envName);
    if (!s) return dflt;
    return std::atof(s) != 0.;
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

    // ---- Optional-component flags (see README "Flags") ----------------------
    // Beam line (default ON — exclude any piece with FLAG=0):
    const bool useMCP     = flagOn("RADSIMPLE_WITH_MCP",      true);
    const bool useTrig    = flagOn("RADSIMPLE_WITH_TRIGGERS", true);
    const bool usePbGlass = flagOn("RADSIMPLE_WITH_PBGLASS",  true);
    // Central E-type capillary (default OFF): the papers' TESTED module left the
    // central hole "unused in these studies" (2401.01747 sec 2), so the faithful
    // default is hole-drilled-but-empty. Set =1 to instrument it with a
    // full-length WLS fibre + SiPMs (the paper's future energy option).
    const bool useCenter  = flagOn("RADSIMPLE_CENTER_ETYPE",  false);
    G4cout << "[SIMPLE] beamline: MCP=" << useMCP << " triggers=" << useTrig
           << " PbGlass=" << usePbGlass
           << "   center E-type capillary=" << useCenter
           << "  (RADSIMPLE_WITH_MCP/_TRIGGERS/_PBGLASS, RADSIMPLE_CENTER_ETYPE)"
           << G4endl;

    // World: big enough for the full CERN H2 test-beam line
    // (triggers at -400/-350 mm ... Pb-glass back face at +520 mm).
    auto worldS = new G4Box("World", 70.*mm, 70.*mm, 620.*mm);
    auto worldLV = new G4LogicalVolume(worldS, air, "World");
    worldLV->SetVisAttributes(G4VisAttributes::GetInvisible());
    auto worldPV = new G4PVPlacement(nullptr, {}, worldLV, "World", nullptr, false, 0);

    // The 4 corner fibre (x,y) positions.
    const G4double c = cornerOff*mm;
    const G4ThreeVector corner[4] =
        { {+c,+c,0}, {+c,-c,0}, {-c,+c,0}, {-c,-c,0} };

    // Helper: drill the FIVE capillary holes through a plate solid (over-long
    // in z): 4 corners + the central one. The papers' tiles have all five holes
    // (2401.01747 Fig. 2) — the central hole exists even when uninstrumented
    // ("available for calibration or additional measurement ... not used in
    // these tests"). 2303.05580 even needed an explicit cut against particles
    // sneaking down it, so the empty hole is real physics, not decoration.
    auto drill = [&](G4VSolid* s, const G4String& nm) -> G4VSolid* {
        auto bore = new G4Tubs(nm+"_bore", 0, holeR*mm, 200.*mm, 0, 360*deg);
        G4VSolid* out = s;
        for (int k = 0; k < 4; ++k)
            out = new G4SubtractionSolid(nm+"_d"+std::to_string(k), out, bore,
                                         nullptr, corner[k]);
        out = new G4SubtractionSolid(nm+"_dc", out, bore, nullptr,
                                     G4ThreeVector(0,0,0));       // central hole
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

    // ---- Build the 4 corner fibres: quartz | DSB1 | quartz ----
    //
    // TWO CAPILLARY TYPES, per arXiv:2401.01747 sec 2:
    //   T-TYPE (default, RADSIMPLE_ETYPE unset) — "solid WLS filaments of short
    //     length ... inserted ... to a position corresponding to the region of
    //     EM shower max", the rest of the core filled with quartz rod. This is
    //     the TIMING capillary and is what the papers' beam test used.
    //     -> light samples only the 15 mm window at 40.4 mm depth.
    //   E-TYPE (RADSIMPLE_ETYPE=1) — "the capillary cores are filled ... with a
    //     solid WLS filament that runs the full length of the module". This is
    //     the ENERGY capillary: it collects light from the WHOLE stack, so its
    //     resolution is a full-module LIGHT readout (neither the shower-max
    //     light nor the truth dE/dx).
    // E-type is opt-in so the default geometry -- and every run already taken
    // with it -- is completely unchanged.
    const G4double front = -stackZ/2.*mm;
    const G4double back  = +stackZ/2.*mm;
    // RADSIMPLE_CORNER_ETYPE=1 turns the 4 CORNER fibres into full-length
    // E-type (the old name RADSIMPLE_ETYPE still works as a fallback).
    const bool eType = flagOn("RADSIMPLE_CORNER_ETYPE",
                              flagOn("RADSIMPLE_ETYPE", false));
    const G4double wlsL = eType ? stackZ*mm : wlsLen*mm;            // full stack, or 15 mm
    const G4double dsbC = eType ? 0.0 : front + showerMaxDepth*mm;  // centred, or at shower max
    const G4double dsbHi = dsbC + wlsL/2;                          // downstream edge
    const G4double dsbLo = dsbC - wlsL/2;                          // upstream edge
    G4cout << "[SIMPLE] corner capillary type: " << (eType ? "E (full-length WLS, energy)"
                                                   : "T (15 mm WLS at shower max, timing)")
           << "  (RADSIMPLE_CORNER_ETYPE)" << G4endl;

    const G4double upLen = dsbLo - front;                         // upstream quartz length
    const G4double dnLen = back - dsbHi;                          // downstream quartz length
    // In E-type the WLS fills the whole fibre, so the quartz stubs have zero
    // length -> skip them entirely (a zero-half-length G4Tubs is invalid).
    const bool haveStubs = (upLen > 1e-6 && dnLen > 1e-6);
    G4LogicalVolume *qUpLV = nullptr, *qDnLV = nullptr;
    if (haveStubs) {
        auto qUpS = new G4Tubs("qUp", 0, fibreR*mm, upLen/2, 0, 360*deg);
        auto qDnS = new G4Tubs("qDn", 0, fibreR*mm, dnLen/2, 0, 360*deg);
        qUpLV = new G4LogicalVolume(qUpS, quartz, "QuartzUp"); qUpLV->SetVisAttributes(qVis);
        qDnLV = new G4LogicalVolume(qDnS, quartz, "QuartzDn"); qDnLV->SetVisAttributes(qVis);
    }
    auto dsbS  = new G4Tubs("dsb",  0, fibreR*mm, wlsL/2, 0, 360*deg);
    auto dsbLV = new G4LogicalVolume(dsbS, dsb1,   "DSB1");       dsbLV->SetVisAttributes(dsbVis);

    // Photodetectors (SiPMs): thin silicon discs at each stack end.
    const G4double pdHz = 0.05*mm;
    auto pdS   = new G4Tubs("pd", 0, fibreR*mm, pdHz, 0, 360*deg);
    auto pdUpLV = new G4LogicalVolume(pdS, silic, "PD_Up");   pdUpLV->SetVisAttributes(pdVis);
    auto pdDnLV = new G4LogicalVolume(pdS, silic, "PD_Down"); pdDnLV->SetVisAttributes(pdVis);

    for (int k = 0; k < 4; ++k) {
        G4ThreeVector p = corner[k];
        if (haveStubs) {
            new G4PVPlacement(nullptr, p + G4ThreeVector(0,0,(front+dsbLo)/2), qUpLV, "QuartzUp", worldLV, false, k);
            new G4PVPlacement(nullptr, p + G4ThreeVector(0,0,(dsbHi+back)/2),  qDnLV, "QuartzDn", worldLV, false, k);
        }
        new G4PVPlacement(nullptr, p + G4ThreeVector(0,0,dsbC),            dsbLV, "DSB1",      worldLV, false, k);
        new G4PVPlacement(nullptr, p + G4ThreeVector(0,0,front-pdHz),      pdUpLV,"PD_Up",     worldLV, false, k);
        new G4PVPlacement(nullptr, p + G4ThreeVector(0,0,back +pdHz),      pdDnLV,"PD_Down",   worldLV, false, k);
    }

    // ---- OPTIONAL: central E-type capillary (RADSIMPLE_CENTER_ETYPE=1) ------
    // A full-length DSB1 WLS fibre in the (always-drilled) central hole, read
    // out at both ends — the papers' E-type ENERGY capillary (2401.01747 sec 2:
    // "the capillary cores are filled ... with a solid WLS filament that runs
    // the full length of the module") placed in the centre slot that the
    // tested module left free. SteppingAction sees these PDs as copy number 4;
    // its light is recorded separately as NpeCenter (never mixed into the
    // 4-corner timing average).
    if (useCenter) {
        auto cS  = new G4Tubs("cdsb", 0, fibreR*mm, stackZ*mm/2, 0, 360*deg);
        auto cLV = new G4LogicalVolume(cS, dsb1, "DSB1");
        cLV->SetVisAttributes(dsbVis);
        new G4PVPlacement(nullptr, {0,0,0},               cLV,   "DSB1",    worldLV, false, 4);
        new G4PVPlacement(nullptr, {0,0,front-pdHz},      pdUpLV,"PD_Up",   worldLV, false, 4);
        new G4PVPlacement(nullptr, {0,0,back +pdHz},      pdDnLV,"PD_Down", worldLV, false, 4);
    }

    // ---- OPTIONAL: CERN H2 test-beam line ----------------------------------
    // Layout per the papers (2401.01747 sec 4-5, Fig. 11; 2303.05580 sec 3):
    // upstream trigger counters and an MCP-PMT timing reference, the module,
    // and a Pb-glass array behind it as tail catcher. The papers give the
    // trigger size (2 x 2 cm^2, 2401 Fig. 11 caption) but no z positions or
    // thicknesses (photographs only) — those follow RADiCALsimDSB's replication.
    // No electronics/noise anywhere: the MCP records a pure particle-arrival
    // time, the counters and Pb-glass record pure energy deposits.
    auto trigVis = new G4VisAttributes(G4Colour(0.1,0.2,0.6,0.5));   // darker blue
    trigVis->SetForceSolid(true);
    auto mcpVis  = new G4VisAttributes(G4Colour(0.6,0.8,1.0,0.5));   // light blue
    mcpVis->SetForceSolid(true);
    auto pbglassVis = new G4VisAttributes(G4Colour(0.6,1.0,0.6,0.5)); // light green
    pbglassVis->SetForceSolid(true);

    if (useTrig) {
        // Two 2 x 2 cm^2 plastic-scintillator coincidence counters (5 mm thick).
        auto scint = G4NistManager::Instance()->FindOrBuildMaterial("G4_PLASTIC_SC_VINYLTOLUENE");
        auto tS  = new G4Box("trig", 10.*mm, 10.*mm, 2.5*mm);
        auto t1LV = new G4LogicalVolume(tS, scint, "Trig1"); t1LV->SetVisAttributes(trigVis);
        auto t2LV = new G4LogicalVolume(tS, scint, "Trig2"); t2LV->SetVisAttributes(trigVis);
        new G4PVPlacement(nullptr, {0,0,-400.*mm}, t1LV, "Trig1", worldLV, false, 0);
        new G4PVPlacement(nullptr, {0,0,-350.*mm}, t2LV, "Trig2", worldLV, false, 0);
    }
    if (useMCP) {
        // MCP-PMT timing reference (2303.05580: Photek MCP): a fused-silica
        // entrance window ("radiator") whose particle-arrival time is the event
        // t0, plus a thin ceramic body. Kept thin so it is a negligible
        // preshower (few % of X0).
        auto alox = G4NistManager::Instance()->FindOrBuildMaterial("G4_ALUMINUM_OXIDE");
        auto rS  = new G4Box("mcpr", 13.5*mm, 13.5*mm, 1.5*mm);
        auto rLV = new G4LogicalVolume(rS, quartz, "MCPRadiator"); rLV->SetVisAttributes(mcpVis);
        auto bLV = new G4LogicalVolume(rS, alox,   "MCPBody");     bLV->SetVisAttributes(mcpVis);
        new G4PVPlacement(nullptr, {0,0,-250.*mm}, rLV, "MCPRadiator", worldLV, false, 0);
        new G4PVPlacement(nullptr, {0,0,-247.*mm}, bLV, "MCPBody",     worldLV, false, 0);
    }
    if (usePbGlass) {
        // Pb-glass array downstream (2401 Fig. 11: "the upstream end of the Pb
        // glass array"): 10 x 10 x 40 cm block, ~30 X0 — catches longitudinal
        // leakage out of the ~23 X0 module (tail catcher / leakage veto).
        auto pbgl = G4NistManager::Instance()->FindOrBuildMaterial("G4_GLASS_LEAD");
        auto gS  = new G4Box("pbg", 50.*mm, 50.*mm, 200.*mm);
        auto gLV = new G4LogicalVolume(gS, pbgl, "PbGlass"); gLV->SetVisAttributes(pbglassVis);
        new G4PVPlacement(nullptr, {0,0,+320.*mm}, gLV, "PbGlass", worldLV, false, 0);
    }

    G4cout << "[SIMPLE] stack " << stackZ << " mm, DSB1 centre z = " << dsbC/mm
           << " mm; 4 corner fibres" << (useCenter ? " + central E-type" : "")
           << ", PDs at both ends." << G4endl;
    return worldPV;
}
