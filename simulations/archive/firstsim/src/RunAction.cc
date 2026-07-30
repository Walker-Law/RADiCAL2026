#include "RunAction.hh"
#include "G4AnalysisManager.hh"

RunAction::RunAction() {
    auto analysisManager = G4AnalysisManager::Instance();
    analysisManager->SetDefaultFileType("root");
    analysisManager->SetVerboseLevel(1);
    
    // Existing Structural Data
    analysisManager->CreateH1("ShowerProfile", "Energy Deposition per Layer", 33, 0., 33.);
    analysisManager->CreateH1("TotalEdep", "Total Sampled Energy in LYSO Plates (GeV)", 120, 0.0, 120.0);

    // NEW DAQ PLOTS
    // Histogram 2: Central Capillary Energy Integration (Shower Max Filtered)
    analysisManager->CreateH1("CentralCapEnergy", "Central Capillary Energy Yield at Shower Max (MeV)", 100, 0.0, 2500.0);
    
    // Histogram 3: Timing Differences from Corner SiPMs
    analysisManager->CreateH1("CornerDeltaT", "Corner SiPM Differential Arrival Time Delta T (ns)", 100, -1.0, 1.0);

    // 2D Histogram 0: Calibration Tracking Matrix (Delta T vs True Collision Z Coordinate)
    analysisManager->CreateH2("TimeVsPositionMatrix", "Timing vs Position Calibration Matrix;Delta T (ns);True Z Position (mm)", 
                              100, -0.8, 0.8, 100, -67.5, 67.5);
}

RunAction::~RunAction() {}

void RunAction::BeginOfRunAction(const G4Run*) {
    auto analysisManager = G4AnalysisManager::Instance();
    analysisManager->OpenFile("calorimeter_output.root");
}

void RunAction::EndOfRunAction(const G4Run*) {
    auto analysisManager = G4AnalysisManager::Instance();
    analysisManager->Write();
    analysisManager->CloseFile();
}
