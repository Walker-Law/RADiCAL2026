#include "DetectorConstruction.hh"
#include "G4NistManager.hh"
#include "G4Sphere.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"
#include "G4Element.hh"
#include "G4Isotope.hh"
#include "G4Material.hh"

DetectorConstruction::DetectorConstruction() {}
DetectorConstruction::~DetectorConstruction() {}

G4VPhysicalVolume* DetectorConstruction::Construct() {
    auto nist = G4NistManager::Instance();

    // --- Materials ---

    G4Material* vacuum   = nist->FindOrBuildMaterial("G4_Galactic");
    G4Material* air      = nist->FindOrBuildMaterial("G4_AIR");

    // Beryllium neutron multiplier — pure Be, 1.85 g/cm3
    G4Material* beryllium = new G4Material("Beryllium_Mult", 1.85*g/cm3, 1);
    beryllium->AddElement(nist->FindOrBuildElement("Be"), 1.0);

    // Tungsten thermalizing layer — pure W, 19.3 g/cm3
    G4Material* tungsten = new G4Material("Tungsten_Therm", 19.3*g/cm3, 1);
    tungsten->AddElement(nist->FindOrBuildElement("W"), 1.0);

    // Lithium Orthosilicate breeding blanket — Li4SiO4
    // Formula unit: 4 Li + 1 Si + 4 O  (9 atoms total)
    // Density: 2.39 g/cm3
    // 90% Li-6 enriched
    G4Isotope* Li6 = new G4Isotope("Li6", 3, 6, 6.015*g/mole);
    G4Isotope* Li7 = new G4Isotope("Li7", 3, 7, 7.016*g/mole);
    G4Element* enrichedLi = new G4Element("EnrichedLithium", "Li", 2);
    enrichedLi->AddIsotope(Li6, 90.*perCent);
    enrichedLi->AddIsotope(Li7, 10.*perCent);

    G4Material* li4SiO4 = new G4Material("Li4SiO4", 2.39*g/cm3, 3);
    li4SiO4->AddElement(enrichedLi,                      4);  // 4 Li per formula unit
    li4SiO4->AddElement(nist->FindOrBuildElement("Si"),  1);  // 1 Si
    li4SiO4->AddElement(nist->FindOrBuildElement("O"),   4);  // 4 O

    // --- Geometry (concentric spheres, all centred at origin) ---

    // World: air, r = 200 cm
    auto solidWorld = new G4Sphere("World", 0, 200.*cm, 0, 360.*deg, 0, 180.*deg);
    auto logicWorld = new G4LogicalVolume(solidWorld, air, "World");
    auto physWorld  = new G4PVPlacement(nullptr, G4ThreeVector(), logicWorld, "World", nullptr, false, 0);
    logicWorld->SetVisAttributes(G4VisAttributes::GetInvisible());

    // Inner void (D-T source region): r = 0–100 cm
    auto solidVoid = new G4Sphere("Void", 0, 100.*cm, 0, 360.*deg, 0, 180.*deg);
    auto logicVoid = new G4LogicalVolume(solidVoid, vacuum, "Void");
    new G4PVPlacement(nullptr, G4ThreeVector(), logicVoid, "Void", logicWorld, false, 0);
    logicVoid->SetVisAttributes(G4VisAttributes::GetInvisible());

    // Layer 1 — Beryllium multiplier: r = 100–105 cm (5 cm)
    auto solidBe = new G4Sphere("Be_Shell", 100.*cm, 105.*cm, 0, 360.*deg, 0, 180.*deg);
    auto logicBe = new G4LogicalVolume(solidBe, beryllium, "Be_Shell");
    new G4PVPlacement(nullptr, G4ThreeVector(), logicBe, "Be_Shell", logicWorld, false, 0);

    auto beVis = new G4VisAttributes(G4Colour(0.7, 0.7, 0.2, 0.12));
    beVis->SetForceSolid(true);
    logicBe->SetVisAttributes(beVis);

    // Layer 2 — Tungsten thermalizer: r = 105–120 cm (15 cm)
    auto solidW = new G4Sphere("W_Shell", 105.*cm, 120.*cm, 0, 360.*deg, 0, 180.*deg);
    auto logicW = new G4LogicalVolume(solidW, tungsten, "W_Shell");
    new G4PVPlacement(nullptr, G4ThreeVector(), logicW, "W_Shell", logicWorld, false, 0);

    auto wVis = new G4VisAttributes(G4Colour(0.6, 0.6, 0.6, 0.12));
    wVis->SetForceSolid(true);
    logicW->SetVisAttributes(wVis);

    // Layer 3 — Li4SiO4 breeding blanket: r = 120–170 cm (50 cm)
    auto solidBlanket = new G4Sphere("Blanket", 120.*cm, 170.*cm, 0, 360.*deg, 0, 180.*deg);
    auto logicBlanket = new G4LogicalVolume(solidBlanket, li4SiO4, "Blanket");
    new G4PVPlacement(nullptr, G4ThreeVector(), logicBlanket, "Blanket", logicWorld, false, 0);

    auto blanketVis = new G4VisAttributes(G4Colour(0.0, 0.6, 1.0, 0.12));
    blanketVis->SetForceSolid(true);
    logicBlanket->SetVisAttributes(blanketVis);

    return physWorld;
}
