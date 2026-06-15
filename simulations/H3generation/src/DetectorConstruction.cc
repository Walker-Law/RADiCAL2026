#include "DetectorConstruction.hh"
#include "G4NistManager.hh"
#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"

DetectorConstruction::DetectorConstruction() {}
DetectorConstruction::~DetectorConstruction() {}

G4VPhysicalVolume* DetectorConstruction::Construct() {
    auto nist = G4NistManager::Instance();

    G4Material* air  = nist->FindOrBuildMaterial("G4_AIR");
    G4Material* lead = nist->FindOrBuildMaterial("G4_Pb");

    // World: 1 m cube of air
    auto solidWorld = new G4Box("World", 0.5*m, 0.5*m, 0.5*m);
    auto logicWorld = new G4LogicalVolume(solidWorld, air, "World");
    auto physWorld  = new G4PVPlacement(nullptr, G4ThreeVector(), logicWorld, "World", nullptr, false, 0);
    logicWorld->SetVisAttributes(G4VisAttributes::GetInvisible());

    // Box: 20 cm lead cube centered in world
    auto solidBox = new G4Box("Box", 10.*cm, 10.*cm, 10.*cm);
    auto logicBox = new G4LogicalVolume(solidBox, lead, "Box");
    new G4PVPlacement(nullptr, G4ThreeVector(), logicBox, "Box", logicWorld, false, 0);

    auto visAttr = new G4VisAttributes(G4Colour(1.0, 1.0, 1.0));
    visAttr->SetForceWireframe(true);
    visAttr->SetVisibility(true);
    logicBox->SetVisAttributes(visAttr);


    return physWorld;
}
