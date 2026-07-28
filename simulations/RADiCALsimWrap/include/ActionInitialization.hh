// ActionInitialization — tells Geant4 which user-action objects to create.
#ifndef ActionInitialization_h
#define ActionInitialization_h
#include "G4VUserActionInitialization.hh"
class ActionInitialization : public G4VUserActionInitialization {
public:
    void Build() const override;
    void BuildForMaster() const override;   // master thread: owns the merged output
};
#endif
