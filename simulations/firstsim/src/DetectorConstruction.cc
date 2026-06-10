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

DetectorConstruction::DetectorConstruction() {}
DetectorConstruction::~DetectorConstruction() {}

G4VPhysicalVolume* DetectorConstruction::Construct() {
    auto nist = G4NistManager::Instance();

    // 1. Core Materials Registry
    G4Material* worldMat  = nist->FindOrBuildMaterial("G4_AIR");
    G4Material* wMat      = nist->FindOrBuildMaterial("G4_W");
    G4Material* quartzMat = nist->FindOrBuildMaterial("G4_SILICON_DIOXIDE"); 

    G4Element* Lu = nist->FindOrBuildElement("Lu");
    G4Element* Y  = nist->FindOrBuildElement("Y");
    G4Element* Al = nist->FindOrBuildElement("Al");
    G4Element* Si = nist->FindOrBuildElement("Si");
    G4Element* O  = nist->FindOrBuildElement("O");
    G4Element* Ce = nist->FindOrBuildElement("Ce");

    G4Material* lysoMat = new G4Material("LYSO", 7.1*g/cm3, 4);
    lysoMat->AddElement(Lu, 71.0*perCent);
    lysoMat->AddElement(Y,   4.0*perCent);
    lysoMat->AddElement(Si,  6.0*perCent);
    lysoMat->AddElement(O,  19.0*perCent);

    G4Material* wlsMat = new G4Material("LuAG_WLS", 6.73*g/cm3, 4);
    wlsMat->AddElement(Lu, 61.5*perCent);
    wlsMat->AddElement(Al, 15.8*perCent);
    wlsMat->AddElement(O,  22.6*perCent);
    wlsMat->AddElement(Ce,  0.1*perCent);

    G4Material* tyvekMat = new G4Material("Tyvek", 0.41*g/cm3, 1);
    tyvekMat->AddMaterial(nist->FindOrBuildMaterial("G4_POLYETHYLENE"), 100*perCent);

    // 2. Optimized World Bounds
    G4double worldX = 40.0 * mm;
    G4double worldY = 40.0 * mm;
    G4double worldZ = 250.0 * mm; 
    auto solidWorld = new G4Box("World", worldX/2, worldY/2, worldZ/2);
    auto logicWorld = new G4LogicalVolume(solidWorld, worldMat, "World");
    auto physWorld  = new G4PVPlacement(nullptr, G4ThreeVector(), logicWorld, "World", nullptr, false, 0);

    // 3. Tyvek Insulation Wrapping Envelope
    G4double caloX = 14.0 * mm;
    G4double caloY = 14.0 * mm;
    G4double caloZ = 135.0 * mm;
    
    G4double tyvekThickness = 0.20 * mm; 
    G4double tyvekX = caloX + (2.0 * tyvekThickness);
    G4double tyvekY = caloY + (2.0 * tyvekThickness);
    G4double tyvekZ = caloZ + (2.0 * tyvekThickness);

    auto solidTyvek = new G4Box("Tyvek_Envelope", tyvekX/2, tyvekY/2, tyvekZ/2);
    auto logicTyvek = new G4LogicalVolume(solidTyvek, tyvekMat, "Tyvek_Logic");
    new G4PVPlacement(nullptr, G4ThreeVector(0,0,0), logicTyvek, "Tyvek_Phys", logicWorld, false, 0);

    // 4. Inner Calorimeter Container
    auto solidCalo = new G4Box("Calorimeter", caloX/2, caloY/2, caloZ/2);
    auto logicCalo = new G4LogicalVolume(solidCalo, worldMat, "Calorimeter");
    new G4PVPlacement(nullptr, G4ThreeVector(0,0,0), logicCalo, "Calorimeter_Phys", logicTyvek, false, 0);

    // 5. Capillary Hole Matrix Drilling
    G4double capRadius = 0.6 * mm; 
    G4double cPos = 4.5 * mm;     
    G4ThreeVector capLocations[5] = {
        G4ThreeVector(0.0, 0.0, 0.0),      
        G4ThreeVector(cPos, cPos, 0.0),    
        G4ThreeVector(-cPos, cPos, 0.0),   
        G4ThreeVector(cPos, -cPos, 0.0),   
        G4ThreeVector(-cPos, -cPos, 0.0)   
    };

    // 6. Build Absorber and Sampling Plates
    G4double wThickness    = 2.5 * mm;
    G4double lysoThickness = 1.5 * mm;
    G4double pairThickness = wThickness + lysoThickness;

    G4VSolid* baseW    = new G4Box("W_Base", caloX/2, caloY/2, wThickness/2);
    G4VSolid* baseLYSO = new G4Box("LYSO_Base", caloX/2, caloY/2, lysoThickness/2);
    
    auto drillBit = new G4Tubs("Drill", 0, capRadius, 10.0*mm, 0., 360.*deg);
    for (int i = 0; i < 5; i++) {
        baseW    = new G4SubtractionSolid("W_Hole", baseW, drillBit, nullptr, capLocations[i]);
        baseLYSO = new G4SubtractionSolid("LYSO_Hole", baseLYSO, drillBit, nullptr, capLocations[i]);
    }

    auto logicW    = new G4LogicalVolume(baseW, wMat, "W_Logic");
    auto logicLYSO = new G4LogicalVolume(baseLYSO, lysoMat, "LYSO");

    G4double startZ = -caloZ / 2.0;
    G4int numLayers = 33;
    for (G4int i = 0; i < numLayers; i++) {
        G4double currentZ = startZ + (i * pairThickness);
        new G4PVPlacement(nullptr, G4ThreeVector(0, 0, currentZ + wThickness/2.0), logicW, "W_Phys", logicCalo, false, i);
        new G4PVPlacement(nullptr, G4ThreeVector(0, 0, currentZ + wThickness + lysoThickness/2.0), logicLYSO, "LYSO_Phys", logicCalo, false, i);
    }

    G4double finalWThickness = 3.0 * mm;
    G4VSolid* baseFinalW = new G4Box("FinalW_Base", caloX/2, caloY/2, finalWThickness/2);
    for (int i = 0; i < 5; i++) baseFinalW = new G4SubtractionSolid("FinalW_Hole", baseFinalW, drillBit, nullptr, capLocations[i]);
    auto logicFinalW = new G4LogicalVolume(baseFinalW, wMat, "FinalW_Logic");
    new G4PVPlacement(nullptr, G4ThreeVector(0, 0, startZ + (numLayers * pairThickness) + finalWThickness/2.0), logicFinalW, "FinalW_Phys", logicCalo, false, numLayers);

    // 7. Inject Active Waveguide Optical Core Elements
    // Central Energy Channel: Full Length LuAG:Ce Core
    auto solidCentral = new G4Tubs("Central_Solid", 0, capRadius, caloZ/2, 0., 360.*deg);
    auto logicCentral = new G4LogicalVolume(solidCentral, wlsMat, "Cap_Central_WLS");
    new G4PVPlacement(nullptr, capLocations[0], logicCentral, "Central_Cap_Phys", logicCalo, false, 0);

    // Corner Timing Channels: Segmented Precision 10mm Core Targets at Shower Max
    G4double lenFront = 57.5 * mm; 
    G4double lenMid   = 10.0 * mm; 
    G4double lenBack  = 67.5 * mm; 

    auto solidCornerFront = new G4Tubs("Corner_Front_Solid", 0, capRadius, lenFront/2, 0., 360.*deg);
    auto solidCornerMid   = new G4Tubs("Corner_Mid_Solid",   0, capRadius, lenMid/2,   0., 360.*deg);
    auto solidCornerBack  = new G4Tubs("Corner_Back_Solid",  0, capRadius, lenBack/2,  0., 360.*deg);

    auto logicCornerFront = new G4LogicalVolume(solidCornerFront, quartzMat, "Cap_Corner_Clear");
    auto logicCornerMid   = new G4LogicalVolume(solidCornerMid,   wlsMat,    "Cap_Corner_WLS");
    auto logicCornerBack  = new G4LogicalVolume(solidCornerBack,  quartzMat, "Cap_Corner_Clear");

    for (int c = 1; c < 5; c++) {
        new G4PVPlacement(nullptr, capLocations[c] + G4ThreeVector(0, 0, -38.75*mm), logicCornerFront, "Corner_Cap_Front_Phys", logicCalo, false, c);
        new G4PVPlacement(nullptr, capLocations[c] + G4ThreeVector(0, 0, -5.0*mm),   logicCornerMid,   "Corner_Cap_Mid_Phys",   logicCalo, false, c);
        new G4PVPlacement(nullptr, capLocations[c] + G4ThreeVector(0, 0, 33.75*mm),  logicCornerBack,  "Corner_Cap_Back_Phys",  logicCalo, false, c);
    }

    // =================================================================================
    // 8. VISUALIZATION CHARACTERISTIC DESIGNATIONS (Updated Opacities)
    // =================================================================================
    logicWorld->SetVisAttributes(G4VisAttributes::GetInvisible());
    logicCalo->SetVisAttributes(G4VisAttributes::GetInvisible());

    // Tyvek Wrapping Skin -> Prominent Translucent Soft White envelope shell (Alpha = 0.35)
    auto tyvekVis = new G4VisAttributes(G4Colour(0.92, 0.92, 0.95, 0.35)); 
    tyvekVis->SetForceSolid(true);
    logicTyvek->SetVisAttributes(tyvekVis);

    // WLS Active Subsections -> Vibrant Solid Green
    auto wlsVis = new G4VisAttributes(G4Colour(0.0, 0.9, 0.0)); 
    wlsVis->SetForceSolid(true);
    logicCentral->SetVisAttributes(wlsVis);
    logicCornerMid->SetVisAttributes(wlsVis);

    // Clear Waveguides -> Translucent Light Green Pass-Through Tubes
    auto clearVis = new G4VisAttributes(G4Colour(0.6, 1.0, 0.6, 0.40)); 
    clearVis->SetForceSolid(true);
    logicCornerFront->SetVisAttributes(clearVis);
    logicCornerBack->SetVisAttributes(clearVis);

    // INTERNAL TILES -> High Transparency (Fainter than the Tyvek wrapping)
    // Tungsten Plates -> Ghostly Dark Gray (Alpha = 0.03)
    auto wVis = new G4VisAttributes(G4Colour(0.4, 0.4, 0.4, 0.03)); 
    wVis->SetForceSolid(true);
    logicW->SetVisAttributes(wVis);
    logicFinalW->SetVisAttributes(wVis);

    // LYSO Crystal Plates -> Ghostly Cyan (Alpha = 0.04)
    auto lysoVis = new G4VisAttributes(G4Colour(0.0, 0.6, 0.9, 0.04)); 
    lysoVis->SetForceSolid(true);
    logicLYSO->SetVisAttributes(lysoVis);

    return physWorld;
}
