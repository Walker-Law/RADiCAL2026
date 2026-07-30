#include "EventAction.hh"
#include "G4SystemOfUnits.hh"
#include "G4Event.hh"
#include "G4AnalysisManager.hh"
#include <algorithm>

EventAction::EventAction() {
    fEdepPerLayer.resize(33, 0.);
}
EventAction::~EventAction() {}

void EventAction::BeginOfEventAction(const G4Event*) {
    std::fill(fEdepPerLayer.begin(), fEdepPerLayer.end(), 0.);
    fCentralCapEnergy = 0.0;
    fCornerDeltaTimes.clear();
    fCornerTrueZs.clear();
}

void EventAction::EndOfEventAction(const G4Event*) {
    auto analysisManager = G4AnalysisManager::Instance();
    
    // 1. Process Standard Active Plate Data
    G4double totalLYSOEnergy = 0.;
    for (G4int i = 0; i < 33; i++) {
        if (fEdepPerLayer[i] > 0.) {
            analysisManager->FillH1(0, i, fEdepPerLayer[i]/MeV);
            totalLYSOEnergy += fEdepPerLayer[i];
        }
    }
    if (totalLYSOEnergy > 0.) analysisManager->FillH1(1, totalLYSOEnergy/GeV);
    
    // 2. Process Central Capillary (Energy Mode)
    if (fCentralCapEnergy > 0.) {
        analysisManager->FillH1(2, fCentralCapEnergy/MeV);
    }

    // 3. Process Corner Capillaries (Timing/Position Reconstruction Mode)
    for (size_t i = 0; i < fCornerDeltaTimes.size(); ++i) {
        analysisManager->FillH1(3, fCornerDeltaTimes[i]/ns);
        analysisManager->FillH2(0, fCornerDeltaTimes[i]/ns, fCornerTrueZs[i]/mm);
    }
}
