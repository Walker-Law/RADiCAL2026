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

    G4Material* vacuum = nist->FindOrBuildMaterial("G4_Galactic");
    G4Material* air    = nist->FindOrBuildMaterial("G4_AIR");

    // Beryllium multiplier — pure Be, density 1.85 g/cm3
    G4Material* beryllium = new G4Material("Beryllium_Mult", 1.85*g/cm3, 1);
    beryllium->AddElement(nist->FindOrBuildElement("Be"), 1.0);

    // Graphite moderator — density 1.70 g/cm3
    G4Material* graphite = new G4Material("Graphite_Mod", 1.70*g/cm3, 1);
    graphite->AddElement(nist->FindOrBuildElement("C"), 1.0);

    // LithiumSilicateGlass blanket — 60 mol% Li2O / 40 mol% SiO2
    // 90% Li-6 enriched, density 2.35 g/cm3
    // Atom fractions: Li-6=0.36, Li-7=0.04, Si=0.1333, O=0.4667
    G4Isotope* Li6 = new G4Isotope("Li6", 3, 6, 6.015*g/mole);
    G4Isotope* Li7 = new G4Isotope("Li7", 3, 7, 7.016*g/mole);
    G4Element* enrichedLi = new G4Element("EnrichedLithium", "Li", 2);
    enrichedLi->AddIsotope(Li6, 90.*perCent);
    enrichedLi->AddIsotope(Li7, 10.*perCent);

    G4Material* liSiOGlass = new G4Material("LiSiO_Glass", 2.35*g/cm3, 3);
    liSiOGlass->AddElement(enrichedLi,                        0.360000);
    liSiOGlass->AddElement(nist->FindOrBuildElement("Si"),    0.133333);
    liSiOGlass->AddElement(nist->FindOrBuildElement("O"),     0.466667 + 0.040000);
    // Li-7 fraction absorbed into Li element above via isotope enrichment

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

    // Layer 2 — Graphite moderator: r = 105–120 cm (15 cm)
    auto solidGraph = new G4Sphere("Graphite_Shell", 105.*cm, 120.*cm, 0, 360.*deg, 0, 180.*deg);
    auto logicGraph = new G4LogicalVolume(solidGraph, graphite, "Graphite_Shell");
    new G4PVPlacement(nullptr, G4ThreeVector(), logicGraph, "Graphite_Shell", logicWorld, false, 0);

    auto graphVis = new G4VisAttributes(G4Colour(0.4, 0.4, 0.4, 0.10));
    graphVis->SetForceSolid(true);
    logicGraph->SetVisAttributes(graphVis);

    // Layer 3 — LiSiO breeding blanket: r = 120–170 cm (50 cm)
    auto solidBlanket = new G4Sphere("Blanket", 120.*cm, 170.*cm, 0, 360.*deg, 0, 180.*deg);
    auto logicBlanket = new G4LogicalVolume(solidBlanket, liSiOGlass, "Blanket");
    new G4PVPlacement(nullptr, G4ThreeVector(), logicBlanket, "Blanket", logicWorld, false, 0);

    auto blanketVis = new G4VisAttributes(G4Colour(0.0, 0.6, 1.0, 0.12));
    blanketVis->SetForceSolid(true);
    logicBlanket->SetVisAttributes(blanketVis);

    return physWorld;
}
