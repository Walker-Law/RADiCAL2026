// DetectorConstruction.cc — geometry and materials for the August 2026 CERN T10
// test-beam simulation (upstream-only readout).
//
// THE PICTURE (beam travels +z):
//
//     e- --->   [ LYSO | W | LYSO | W | ... | LYSO ]        (57 plates, Tyvek between)
//                        |                     |
//                 4 fibres run through the corners of every plate:
//                        quartz --- filament (15 mm at shower max) --- quartz
//                        |                                              |
//                   PD_Up (SiPM)                                   open end
//
// WHAT IS DIFFERENT FROM RADiCALsimSIMPLE (the 2023-2024 H2 module model):
//   * ONE silicon photomultiplier per corner, at the UPSTREAM end only. The
//     downstream sensors and their readout card were physically removed for
//     this beam test, so the downstream fibre end is an open quartz face in air.
//   * The filament material is selectable at run time (RADSIMPLE_CAPILLARY):
//     DSB1 (organic wavelength shifter, unchanged from SIMPLE) or LuAG:Ce
//     (ceramic wavelength shifter that also scintillates on its own).
//     EJ199 is a placeholder until its properties are known.
//   * No lead-glass tail catcher (none was installed). The micro-channel-plate
//     timing reference is modelled but OFF by default (none was used in the
//     analysis); the two trigger scintillators are ON.
//   * Everything else — the 14 x 14 mm module, 29 LYSO + 28 W plates, the
//     15 mm filament window at 40.4 mm depth, the drilled-but-empty centre
//     hole — is the SIMPLE geometry, unchanged, as confirmed for this run.
//
// LIGHT CHAIN:
//   1. the shower deposits energy in the LYSO plates,
//   2. LYSO scintillates -> 420 nm blue photons,
//   3. a blue photon that reaches a corner fibre enters the quartz and, in the
//      15 mm filament at shower max, may be ABSORBED and RE-EMITTED at longer
//      wavelength (process "OpWLS"). With LuAG:Ce the filament ALSO scintillates
//      from the shower energy deposited directly in it,
//   4. the shifted photon is guided by total internal reflection along the
//      quartz to the upstream sensor (or lost out of the open downstream end),
//   5. every detected photon's arrival time is stored (the "perfect waveform"),
//      and one electronics-free trigger is computed from it (see EventAction).
//
// NO electronics: no waveform shaping, threshold, gain, noise, or digitization.

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
#include "G4Exception.hh"
#include <cstdlib>
#include <cmath>
#include <string>
#include <algorithm>

// -------------------------------------------------------------------------
// Which filament is in the four corner capillaries this run.
// RADSIMPLE_CAPILLARY = LUAG | DSB1  (EJ199 is recognised but refused until its
// optical properties are known). No default on purpose: a 20-hour cluster run
// on the wrong material is far worse than an immediate, loud abort.
// -------------------------------------------------------------------------
std::string DetectorConstruction::CapillaryChoice() {
    const char* s = std::getenv("RADSIMPLE_CAPILLARY");
    std::string v = s ? s : "";
    std::transform(v.begin(), v.end(), v.begin(), ::toupper);
    v.erase(std::remove(v.begin(), v.end(), ':'), v.end());   // "LuAG:Ce" -> "LUAGCE"
    if (v == "LUAGCE") v = "LUAG";
    if (v == "LUAG" || v == "DSB1") return v;
    if (v == "EJ199") {
        G4Exception("DetectorConstruction", "TB26-EJ199", FatalException,
            "RADSIMPLE_CAPILLARY=EJ199: EJ199 is a wavelength shifter whose optical "
            "properties (emission, absorption, decay time, refractive index) are not "
            "yet known. Add them in DefineMaterials() before running configuration C.");
    }
    G4Exception("DetectorConstruction", "TB26-CAP", FatalException,
        ("RADSIMPLE_CAPILLARY must be set to LUAG or DSB1 (got '" +
         std::string(s ? s : "<unset>") + "'). Every run needs an explicit filament.").c_str());
    return "";
}

// -------------------------------------------------------------------------
// MATERIALS.  LYSO / quartz / DSB1 / Tyvek are byte-for-byte the verified
// RADiCALsimSIMPLE tables (including the 2026-08-08 chromatic-dispersion
// curves). LuAG:Ce is new here and built from literature values — every
// number is annotated with its source and confidence.
// -------------------------------------------------------------------------
void DetectorConstruction::DefineMaterials() {
    auto nist = G4NistManager::Instance();
    nist->FindOrBuildMaterial("G4_W");               // tungsten absorber
    nist->FindOrBuildMaterial("G4_Si");              // SiPM sensor body
    nist->FindOrBuildMaterial("G4_AIR");
    auto quartz = nist->FindOrBuildMaterial("G4_SILICON_DIOXIDE");  // fibre light guide

    auto Lu = nist->FindOrBuildElement("Lu");
    auto Y  = nist->FindOrBuildElement("Y");
    auto Si = nist->FindOrBuildElement("Si");
    auto O  = nist->FindOrBuildElement("O");
    auto C  = nist->FindOrBuildElement("C");
    auto H  = nist->FindOrBuildElement("H");
    auto Al = nist->FindOrBuildElement("Al");
    auto Ce = nist->FindOrBuildElement("Ce");

    // LYSO:Ce, built from its elements (not in the NIST table).
    auto lyso = new G4Material("LYSO", 7.1*g/cm3, 4);
    lyso->AddElement(Lu, 0.7145); lyso->AddElement(Y, 0.0403);
    lyso->AddElement(Si, 0.0637); lyso->AddElement(O, 0.1815);

    // DSB1 WLS shifter, modelled PDMS-like (a soft silicone-based plastic).
    auto dsb1 = new G4Material("DSB1", 1.05*g/cm3, 4);
    dsb1->AddElement(Si, 0.379); dsb1->AddElement(O, 0.216);
    dsb1->AddElement(C, 0.324);  dsb1->AddElement(H, 0.081);

    // LuAG:Ce — Lu3Al5O12 with ~0.1% cerium; density 6.73 g/cm3 (same
    // composition as RADiCALsimLuAG, which took it from the crystal datasheets).
    auto luag = new G4Material("LuAG_Ce", 6.73*g/cm3, 4);
    luag->AddElement(Lu, 0.615); luag->AddElement(Al, 0.158);
    luag->AddElement(O,  0.226); luag->AddElement(Ce, 0.001);

    // Tyvek reflective wrap = polyethylene (CH2)n at nonwoven density.
    auto tyvek = new G4Material("Tyvek", 0.38*g/cm3, 2);
    tyvek->AddElement(C, 1); tyvek->AddElement(H, 2);

    // Coarse grid for spectra / absorption (800..350 nm), and the fine grid the
    // refractive-index curves live on (Geant4 derives the group velocity from
    // dn/dE, so RINDEX needs the dense sampling; spectra do not).
    const std::vector<G4double> phE =
        {1.55*eV, 2.07*eV, 2.48*eV, 2.76*eV, 3.10*eV, 3.54*eV};   // 800..350 nm
    const std::vector<G4double> phEfine =
        {1.5498*eV, 1.7712*eV, 1.9997*eV, 2.2140*eV, 2.4311*eV, 2.5830*eV,
         2.6953*eV, 2.8178*eV, 2.9173*eV, 3.0240*eV, 3.0996*eV, 3.1791*eV,
         3.2627*eV, 3.3509*eV, 3.4440*eV, 3.5424*eV};   // 800..350 nm, dense in blue
    const G4double hc_eVnm = 1239.842;                 // photon energy (eV) = hc / lambda (nm)

    // --- Fused quartz: transparent light guide (Malitson Sellmeier). ---
    auto qMPT = new G4MaterialPropertiesTable();
    qMPT->AddProperty("RINDEX", phEfine,
        {1.4533, 1.4553, 1.4574, 1.4595, 1.4618, 1.4635,
         1.4648, 1.4663, 1.4676, 1.4691, 1.4701, 1.4713,
         1.4725, 1.4738, 1.4753, 1.4769});
    qMPT->AddProperty("ABSLENGTH", phE, {10.*m,10.*m,10.*m,10.*m,8.*m,5.*m});
    quartz->SetMaterialPropertiesTable(qMPT);

    // --- Air: n=1 so optical boundaries work (photons can cross gaps and
    //     leave the open downstream fibre end). ---
    auto aMPT = new G4MaterialPropertiesTable();
    aMPT->AddProperty("RINDEX", phE, std::vector<G4double>(6, 1.0));
    G4Material::GetMaterial("G4_AIR")->SetMaterialPropertiesTable(aMPT);

    // --- Light scale: coherent thinning of ALL light (LYSO and LuAG
    //     scintillation here, Cherenkov in StackingAction) by one factor f.
    //     Default 1e-2 for quick local checks; the run script sets 1.0 (true
    //     light) for cluster production. RADSIMPLE_LYSO_SCALE is the old name.
    G4double lightScale = 1e-2;
    if (const char* s = std::getenv("RADSIMPLE_LIGHT_SCALE")) {
        double v = std::atof(s); if (v > 0.) lightScale = v;
    } else if (const char* s = std::getenv("RADSIMPLE_LYSO_SCALE")) {
        double v = std::atof(s); if (v > 0.) lightScale = v;
    }

    // --- LYSO: SCINTILLATOR. 33200 ph/MeV (datasheet), 36 ns, emits ~420 nm.
    //     Effective single-pole Sellmeier anchored to n(420)=1.82 and group
    //     index n_g(420)~1.95 (see RADiCALsimSIMPLE for the derivation). ---
    auto yMPT = new G4MaterialPropertiesTable();
    yMPT->AddProperty("RINDEX", phEfine,
        {1.7756, 1.7805, 1.7864, 1.7926, 1.7996, 1.8051,
         1.8093, 1.8143, 1.8185, 1.8232, 1.8267, 1.8305,
         1.8347, 1.8392, 1.8442, 1.8497});
    yMPT->AddProperty("ABSLENGTH", phE, std::vector<G4double>(6, 40.*cm));
    yMPT->AddProperty("SCINTILLATIONCOMPONENT1", phE, {0.00,0.02,0.25,0.70,0.80,0.00}); // 420 nm peak
    yMPT->AddConstProperty("SCINTILLATIONYIELD",        33200./MeV * lightScale);
    yMPT->AddConstProperty("RESOLUTIONSCALE",           1.0);
    yMPT->AddConstProperty("SCINTILLATIONTIMECONSTANT1", 36.*ns);
    yMPT->AddConstProperty("SCINTILLATIONYIELD1",        1.0);
    lyso->SetMaterialPropertiesTable(yMPT);
    G4cout << "[TB26] light scale " << lightScale << " -> "
           << 33200.*lightScale << " ph/MeV LYSO, Cherenkov and LuAG thinned to match"
           << " (RADSIMPLE_LIGHT_SCALE)" << G4endl;

    // --- DSB1: PURE wavelength shifter (no self-scintillation). Absorbs blue
    //     (covering LYSO's 420 nm), re-emits green (495 nm peak), 3.5 ns.
    //     n renormalised to 1.500 at 500 nm with polystyrene-like dispersion.
    //
    //     STOKES SAFETY (2026-09-15). The absorption and emission tables carried
    //     over from RADiCALsimSIMPLE crashed a true-light run with G4Exception
    //     WSL01 ("sampled photon energy is greater than the primary photon
    //     energy"). G4OpWLS samples the emission spectrum without regard to the
    //     absorbed photon's energy and aborts if the draw comes out higher, so
    //     the two tables must not overlap in energy at all. Two overlaps existed:
    //       1. WLSABSLENGTH was 5 m at 800/600/500 nm — meant as "transparent",
    //          but a photon rattling down a fibre covers metres of path, so red
    //          Cherenkov light DID get absorbed, and then every possible emission
    //          energy was above it. This is what aborted the DSB1 run (an 800 nm
    //          Cherenkov photon at 1.54981 eV, re-emitted at 1.55 eV).
    //       2. Emission was non-zero at 450 and 400 nm, inside the absorbing
    //          band, so even a correctly absorbed blue photon could be re-emitted
    //          bluer than it arrived.
    //     Now: absorption only above 2.76 eV (below ~450 nm), emission density
    //     strictly zero above 2.58 eV (above ~480 nm) — a 0.18 eV margin, checked
    //     at construction by CheckStokesSafety(). The cost is the 450 nm emission
    //     shoulder, which in reality is self-absorbed by the shifter anyway. ---
    auto dMPT = new G4MaterialPropertiesTable();
    dMPT->AddProperty("RINDEX", phEfine,
        {1.4782, 1.4825, 1.4875, 1.4927, 1.4986, 1.5030,
         1.5065, 1.5104, 1.5138, 1.5174, 1.5201, 1.5230,
         1.5262, 1.5296, 1.5333, 1.5373});
    dMPT->AddProperty("ABSLENGTH",    phE, std::vector<G4double>(6, 1.*m));
    //                                    800nm   500nm   480nm    449nm   400nm   350nm
    dMPT->AddProperty("WLSABSLENGTH",
        {1.55*eV, 2.48*eV, 2.58*eV, 2.76*eV, 3.10*eV, 3.54*eV},
        {  1e9*mm,  1e9*mm,  1e9*mm,   2.*mm,   2.*mm,   5.*mm});   // eats blue, truly transparent in the red
    dMPT->AddProperty("WLSCOMPONENT",
        {1.55*eV, 2.07*eV, 2.48*eV, 2.58*eV, 3.54*eV},
        {    0.08,    0.45,    1.00,    0.00,    0.00});            // emits green, 495 nm peak, nothing above 480 nm
    dMPT->AddConstProperty("WLSTIMECONSTANT", 3.5*ns);
    dsb1->SetMaterialPropertiesTable(dMPT);

    // --- LuAG:Ce: ceramic wavelength shifter AND scintillator. Literature
    //     values, all flagged (2026-09-11):
    //   refractive index  n(633 nm) = 1.842, n(450 nm) ~ 1.863 -> Cauchy fit
    //                     n = 1.8205 + 0.0086/lambda^2 [um]; n(530) = 1.851.
    //   absorption        Ce3+ 4f->5d bands: strong at 450 nm (+-20 nm) and at
    //                     345 nm; peak alpha ~ 2 /mm for ~0.1-0.2% Ce, so the
    //                     LYSO 420 nm light sits on the SHOULDER of the main
    //                     band (absorption length ~1.5 mm there, ~0.5 mm at the
    //                     450 nm peak, transparent above ~500 nm).
    //   emission          broad, 480-650 nm, peak 520-540 nm (modelled as a
    //                     Gaussian at 530 nm, sigma 40 nm).
    //   decay             ~60 ns (5d->4f Ce lifetime; literature 55-70 ns),
    //                     used for BOTH the shifted re-emission and the
    //                     self-scintillation — this is 17x slower than DSB1
    //                     and is expected to dominate the timing difference.
    //   scintillation     25000 photons/MeV (literature 20-26 k), RESOLUTIONSCALE 1.
    //   quantum efficiency of the shift  WLSMEANNUMBERPHOTONS = 0.7 (ceramic
    //                     Ce:LuAG quantum efficiency ~0.6-0.7 in the literature;
    //                     DSB1 keeps Geant4's default of 1.0 as in SIMPLE — a
    //                     known asymmetry, stated here so it is not forgotten).
    auto lMPT = new G4MaterialPropertiesTable();
    {
        std::vector<G4double> nE, nV;
        for (double lam : {800.,700.,620.,560.,510.,480.,460.,440.,425.,410.,400.,390.,380.,370.,360.,350.}) {
            const double um = lam/1000.;
            nE.push_back(hc_eVnm/lam*eV);
            nV.push_back(1.8205 + 0.0086/(um*um));
        }
        lMPT->AddProperty("RINDEX", nE, nV);

        // Wavelength-shifting absorption: two Gaussian Ce3+ bands. Above 500 nm
        // the material is transparent to the shift process (1e9 mm): a finite
        // "cap" there let rare red Cherenkov photons be absorbed, which then
        // had no legal re-emission energy (Geant4 cannot up-convert) — a
        // fatal G4OpWLS exception, found in the first 240-event check.
        std::vector<G4double> aE, aL;
        for (double lam = 800.; lam >= 350.; lam -= 10.) {
            const double alpha = 2.0*std::exp(-0.5*std::pow((lam-450.)/20.,2))
                               + 2.0*std::exp(-0.5*std::pow((lam-345.)/15.,2));   // per mm
            const double L = (lam > 500. || alpha < 1e-6) ? 1e9 : 1./alpha;        // mm
            aE.push_back(hc_eVnm/lam*eV);
            aL.push_back(L*mm);
        }
        lMPT->AddProperty("WLSABSLENGTH", aE, aL);
        lMPT->AddProperty("ABSLENGTH", phE, std::vector<G4double>(6, 1.*m));   // bulk, non-shifting

        // Emission spectrum (shared by the shift and the scintillation): a
        // Gaussian at 530 nm, 40 nm wide, TABULATED all the way to 800 nm (as
        // DSB1's is) so that every absorbed photon has re-emission energies at
        // or below its own — Geant4 samples the emission at or below the
        // absorbed energy, and needs the table to reach that low.
        std::vector<G4double> eE, eV_;
        for (double lam = 800.; lam >= 440.; lam -= 10.) {
            eE.push_back(hc_eVnm/lam*eV);
            eV_.push_back(std::max(1e-6, std::exp(-0.5*std::pow((lam-530.)/40.,2))));
        }
        lMPT->AddProperty("WLSCOMPONENT",            eE, eV_);
        lMPT->AddProperty("SCINTILLATIONCOMPONENT1", eE, eV_);
    }
    lMPT->AddConstProperty("WLSTIMECONSTANT",           60.*ns);
    lMPT->AddConstProperty("WLSMEANNUMBERPHOTONS",      0.7);
    lMPT->AddConstProperty("SCINTILLATIONYIELD",        25000./MeV * lightScale);
    lMPT->AddConstProperty("RESOLUTIONSCALE",           1.0);
    lMPT->AddConstProperty("SCINTILLATIONTIMECONSTANT1", 60.*ns);
    lMPT->AddConstProperty("SCINTILLATIONYIELD1",        1.0);
    luag->SetMaterialPropertiesTable(lMPT);

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
    const std::string cap = CapillaryChoice();

    auto air    = G4Material::GetMaterial("G4_AIR");
    auto lyso   = G4Material::GetMaterial("LYSO");
    auto tung   = G4Material::GetMaterial("G4_W");
    auto tyvek  = G4Material::GetMaterial("Tyvek");
    auto quartz = G4Material::GetMaterial("G4_SILICON_DIOXIDE");
    auto silic  = G4Material::GetMaterial("G4_Si");
    auto filMat = G4Material::GetMaterial(cap == "LUAG" ? "LuAG_Ce" : "DSB1");

    const G4double hXY = tileXY/2.*mm;

    // ---- Optional beam-line pieces --------------------------------------
    // Triggers ON (ScintA/ScintB were in the beam). MCP OFF: none was used in
    // the analysis; set RADSIMPLE_WITH_MCP=1 to put the SIMPLE model back.
    const bool useTrig = flagOn("RADSIMPLE_WITH_TRIGGERS", true);
    const bool useMCP  = flagOn("RADSIMPLE_WITH_MCP",      false);
    G4cout << "[TB26] capillary filament: " << cap
           << "   triggers=" << useTrig << " MCP=" << useMCP
           << "  (RADSIMPLE_CAPILLARY, RADSIMPLE_WITH_TRIGGERS, RADSIMPLE_WITH_MCP)"
           << G4endl;

    // World: room for the upstream counters (-400 mm) and an optional MCP.
    auto worldS = new G4Box("World", 70.*mm, 70.*mm, 620.*mm);
    auto worldLV = new G4LogicalVolume(worldS, air, "World");
    worldLV->SetVisAttributes(G4VisAttributes::GetInvisible());
    auto worldPV = new G4PVPlacement(nullptr, {}, worldLV, "World", nullptr, false, 0);

    // The 4 corner fibre (x,y) positions. Index convention (used everywhere):
    //   0 = (+,+)  1 = (+,-)  2 = (-,+)  3 = (-,-)   -> diagonals are 0-3 and 1-2.
    const G4double c = cornerOff*mm;
    const G4ThreeVector corner[4] =
        { {+c,+c,0}, {+c,-c,0}, {-c,+c,0}, {-c,-c,0} };

    // Drill the FIVE capillary holes through a plate (4 corners + the empty
    // centre hole, which is real: uninstrumented but present).
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

    // Reflective Tyvek surface (98% diffuse) on the foils.
    auto tyvekSurf = new G4OpticalSurface("Tyvek");
    tyvekSurf->SetType(dielectric_metal); tyvekSurf->SetFinish(ground);
    tyvekSurf->SetModel(unified);
    auto tsMPT = new G4MaterialPropertiesTable();
    tsMPT->AddProperty("REFLECTIVITY", {1.55*eV,3.54*eV}, {0.98,0.98});
    tyvekSurf->SetMaterialPropertiesTable(tsMPT);

    // Vis colours.
    auto lysoVis = new G4VisAttributes(G4Colour(0.3,0.5,1.0,0.3)); // blue
    auto wVis    = new G4VisAttributes(G4Colour(1.0,0.3,0.3,0.3)); // red
    wVis->SetForceSolid(true);
    auto filVis  = new G4VisAttributes(cap == "LUAG" ? G4Colour(0.2,0.9,0.2)     // green
                                                    : G4Colour(1.0,0.6,0.0));   // orange
    filVis->SetForceSolid(true);
    auto qVis    = new G4VisAttributes(G4Colour(0.7,0.9,1.0,0.4));
    auto pdVis   = new G4VisAttributes(G4Colour(1.0,1.0,0.0));     // yellow
    pdVis->SetForceSolid(true);

    // ---- Build the stack: LYSO | Tyvek | W | Tyvek | LYSO | ... ----
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
        new G4PVPlacement(nullptr, G4ThreeVector(0,0,z), lv,
                          isLyso ? "LYSO" : "W", worldLV, false, i);
        z += th/2;
        if (i < nPlates - 1) {
            z += tyvekThick*mm/2;
            auto ts = drill(new G4Box("tyv", hXY, hXY, tyvekThick*mm/2), "tyv");
            auto tlv = new G4LogicalVolume(ts, tyvek, "Tyvek");
            tlv->SetVisAttributes(G4VisAttributes::GetInvisible());
            new G4LogicalSkinSurface("TyvekSurf", tlv, tyvekSurf);
            new G4PVPlacement(nullptr, G4ThreeVector(0,0,z), tlv, "Tyvek",
                              worldLV, false, i);
            z += tyvekThick*mm/2;
        }
    }

    // ---- The 4 corner fibres: quartz | filament | quartz, ONE sensor ----
    // T-type only (confirmed for every configuration of this beam test): a
    // 15 mm filament centred at the 120 GeV shower-max depth of 40.4 mm, the
    // rest of the bore filled with quartz rod. NOTE for 1-11 GeV: shower max
    // sits at roughly 21-33 mm depth, upstream of the 33-48 mm window. The
    // window was NOT moved for this beam test, so that mismatch is part of
    // what these simulations measure.
    const G4double front = -stackZ/2.*mm;
    const G4double back  = +stackZ/2.*mm;
    const G4double filC  = front + showerMaxDepth*mm;
    const G4double filLo = filC - wlsLen*mm/2, filHi = filC + wlsLen*mm/2;
    const G4double upLen = filLo - front, dnLen = back - filHi;

    auto qUpS = new G4Tubs("qUp", 0, fibreR*mm, upLen/2, 0, 360*deg);
    auto qDnS = new G4Tubs("qDn", 0, fibreR*mm, dnLen/2, 0, 360*deg);
    auto qUpLV = new G4LogicalVolume(qUpS, quartz, "QuartzUp"); qUpLV->SetVisAttributes(qVis);
    auto qDnLV = new G4LogicalVolume(qDnS, quartz, "QuartzDn"); qDnLV->SetVisAttributes(qVis);
    auto filS  = new G4Tubs("fil", 0, fibreR*mm, wlsLen*mm/2, 0, 360*deg);
    auto filLV = new G4LogicalVolume(filS, filMat, "FIL");      filLV->SetVisAttributes(filVis);

    // Upstream silicon photomultiplier only: a thin silicon disc glued to the
    // upstream fibre face. The downstream face is left open to air.
    const G4double pdHz = 0.05*mm;
    auto pdS    = new G4Tubs("pd", 0, fibreR*mm, pdHz, 0, 360*deg);
    auto pdUpLV = new G4LogicalVolume(pdS, silic, "PD_Up");   pdUpLV->SetVisAttributes(pdVis);

    for (int k = 0; k < 4; ++k) {
        G4ThreeVector p = corner[k];
        new G4PVPlacement(nullptr, p + G4ThreeVector(0,0,(front+filLo)/2), qUpLV, "QuartzUp", worldLV, false, k);
        new G4PVPlacement(nullptr, p + G4ThreeVector(0,0,(filHi+back)/2),  qDnLV, "QuartzDn", worldLV, false, k);
        new G4PVPlacement(nullptr, p + G4ThreeVector(0,0,filC),            filLV, "FIL",      worldLV, false, k);
        new G4PVPlacement(nullptr, p + G4ThreeVector(0,0,front-pdHz),      pdUpLV,"PD_Up",    worldLV, false, k);
    }

    // ---- Beam line -----------------------------------------------------------
    // T10 positions relative to the module are not documented for this run;
    // the SIMPLE (H2) spacings are kept as placeholders. Two 2 x 2 cm^2 plastic
    // scintillator counters (5 mm thick) = ScintA/ScintB. No lead glass.
    auto trigVis = new G4VisAttributes(G4Colour(0.1,0.2,0.6,0.5)); trigVis->SetForceSolid(true);
    auto mcpVis  = new G4VisAttributes(G4Colour(0.6,0.8,1.0,0.5)); mcpVis->SetForceSolid(true);
    if (useTrig) {
        auto scint = G4NistManager::Instance()->FindOrBuildMaterial("G4_PLASTIC_SC_VINYLTOLUENE");
        auto tS  = new G4Box("trig", 10.*mm, 10.*mm, 2.5*mm);
        auto t1LV = new G4LogicalVolume(tS, scint, "Trig1"); t1LV->SetVisAttributes(trigVis);
        auto t2LV = new G4LogicalVolume(tS, scint, "Trig2"); t2LV->SetVisAttributes(trigVis);
        new G4PVPlacement(nullptr, {0,0,-400.*mm}, t1LV, "Trig1", worldLV, false, 0);
        new G4PVPlacement(nullptr, {0,0,-350.*mm}, t2LV, "Trig2", worldLV, false, 0);
    }
    if (useMCP) {
        auto alox = G4NistManager::Instance()->FindOrBuildMaterial("G4_ALUMINUM_OXIDE");
        auto rS  = new G4Box("mcpr", 13.5*mm, 13.5*mm, 1.5*mm);
        auto rLV = new G4LogicalVolume(rS, quartz, "MCPRadiator"); rLV->SetVisAttributes(mcpVis);
        auto bLV = new G4LogicalVolume(rS, alox,   "MCPBody");     bLV->SetVisAttributes(mcpVis);
        new G4PVPlacement(nullptr, {0,0,-250.*mm}, rLV, "MCPRadiator", worldLV, false, 0);
        new G4PVPlacement(nullptr, {0,0,-247.*mm}, bLV, "MCPBody",     worldLV, false, 0);
    }

    G4cout << "[TB26] stack " << stackZ << " mm, filament (" << cap << ") centre z = "
           << filC/mm << " mm, window " << (filLo-front)/mm << "-" << (filHi-front)/mm
           << " mm from the front face; 4 corner fibres, sensor at the UPSTREAM end only,"
           << " downstream end open." << G4endl;
    return worldPV;
}
