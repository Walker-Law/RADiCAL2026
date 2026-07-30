#ifndef EventAction_h
#define EventAction_h

#include "G4UserEventAction.hh"
#include "globals.hh"
#include <vector>

class EventAction : public G4UserEventAction {
public:
    EventAction();
    ~EventAction() override;

    void BeginOfEventAction(const G4Event*) override;
    void EndOfEventAction(const G4Event*) override;

    void AddEdep(G4int layer, G4double edep) {
        if (layer >= 0 && layer < 33) fEdepPerLayer[layer] += edep;
    }

    void AddCentralCapEnergy(G4double edep) { fCentralCapEnergy += edep; }
    void RecordCornerHit(G4double deltaT, G4double trueZ) {
        fCornerDeltaTimes.push_back(deltaT);
        fCornerTrueZs.push_back(trueZ);
    }

private:
    std::vector<G4double> fEdepPerLayer;
    G4double fCentralCapEnergy;
    std::vector<G4double> fCornerDeltaTimes;
    std::vector<G4double> fCornerTrueZs;
};

#endif
