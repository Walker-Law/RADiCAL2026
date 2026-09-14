// EventAction — per-event bookkeeping. SteppingAction calls the Add*/Record*
// methods during the event; EndOfEventAction() turns them into observables and
// fills the histograms + ntuple.
//
// THIS SIMULATION IS A LIGHT RECORDER (same design as RADiCALsimSIMPLE): every
// detected photon's arrival time, corner, and origin is stored unconditionally
// — the complete record of the light just before any electronics — and ONE
// electronics-free trigger is computed from it. Any other estimator is an
// offline analysis choice on the stored photons, not a rerun.
//
// UPSTREAM-ONLY READOUT. The August 2026 beam test had a silicon photomultiplier
// at the upstream end of each corner fibre and nothing at the downstream end,
// so the old "downstream minus upstream" timing difference does not exist.
// Two timing observables are computed instead, from the per-corner 5% quantile
// times t05[k] (the arrival time of the ceil(0.05 N)-th photon at corner k,
// the light-level analog of the test-beam's 5% constant-fraction convention):
//
//   dTpair  = ( t05[0]+t05[3] )/2  -  ( t05[1]+t05[2] )/2
//             The difference of the two DIAGONAL corner-pair means — the beam
//             test's own reference-free estimator (radical-t10-2026,
//             macros/DiagDiff.C, "(NW+SE)/2 - (NE+SW)/2"). It cancels the
//             unknown event start time (no external reference needed), so it
//             is REALIZABLE with exactly the channels that were read out, and
//             each pair mean is first-order insensitive to a beam-position
//             shift in x AND y, so position drift cancels too. For four equal,
//             independent corners sigma(dTpair) equals the single-corner
//             resolution, and sigma(dTpair)/2 is the intrinsic four-corner-
//             average resolution — the experiment's "sigma_intr" column.
//             Corner index convention: 0=(+,+) 1=(+,-) 2=(-,+) 3=(-,-);
//             the diagonals are {0,3} and {1,2}.
//   tUpMean = mean of the t05[k] that exist, ABSOLUTE (relative to the gun
//             firing at t=0). Its event-to-event spread is the resolution one
//             would get against a PERFECT external time reference — an upper
//             bound on what an MCP-referenced measurement could reach. Not
//             realizable by itself; kept because it separates "the light" from
//             "the reference".
//
// PHOTON ORIGIN CODES (stored per photon as phOrigin, and summed per event):
//   0  Cherenkov, born anywhere (quartz, filament, LYSO)
//   1  LYSO scintillation that reached the sensor WITHOUT being shifted
//   2  wavelength-shifted light (absorbed in the filament, re-emitted: OpWLS)
//   3  the filament's OWN scintillation from shower energy deposited in it
//      (LuAG:Ce only; DSB1 does not scintillate in this model)
#ifndef EventAction_h
#define EventAction_h
#include "G4UserEventAction.hh"
#include "globals.hh"
#include <array>
#include <vector>
class RunAction;

class EventAction : public G4UserEventAction {
public:
    explicit EventAction(RunAction*) {}
    void BeginOfEventAction(const G4Event*) override;
    void EndOfEventAction(const G4Event*) override;

    void AddLYSO(G4double edep, G4int layer) {
        fElyso += edep;
        if (layer >= 0 && layer < kNLayers) fLayerEacc[layer] += edep;
    }
    void AddW(G4double edep) { fEw += edep; }
    void AddFil(G4double edep) { fEfil += edep; }

    // corner 0..3, arrival time in ns, origin code 0..3 (see above)
    void RecordPhoton(G4int corner, G4double t, G4int origin) {
        if (corner < 0 || corner > 3) return;
        fT[corner].push_back(t);
        fPhT.push_back(t);
        fPhId.push_back(corner);
        fPhOrigin.push_back(origin);
        ++fNpe;
        ++fCornerNpeAcc[corner];
        if (origin >= 0 && origin < 4) ++fNorigin[origin];
    }

    void RecordMCP(G4double t) { if (t < fTmcp) fTmcp = t; }
    void AddTrig(G4int i, G4double e) { if (i==0||i==1) fEtrig[i] += e; }

    // Vector ntuple columns (bound by reference in RunAction).
    std::vector<G4double> fLayerE;     // GeV per LYSO layer [29]
    std::vector<G4double> fCornerNpe;  // photons per corner [4]
    std::vector<G4double> fT05Up;      // 5% quantile time per corner [4], -999 = no light
    std::vector<G4double> fPhT;        // every detected photon: arrival time (ns)
    std::vector<G4double> fPhId;       //                        corner 0..3
    std::vector<G4double> fPhOrigin;   //                        origin code 0..3

    static constexpr G4int    kNLayers = 29;
    static constexpr G4double kCfdFrac = 0.05;   // the test-beam convention

private:
    static constexpr G4double kBig = 1e9;
    std::array<std::vector<G4double>,4> fT;
    G4double fElyso = 0., fEw = 0., fEfil = 0.;
    G4double fNpe = 0.;
    std::array<G4double,4> fNorigin{};
    G4double fTmcp = kBig;
    std::array<G4double,2> fEtrig{};
    std::array<G4double,kNLayers> fLayerEacc{};
    std::array<G4double,4> fCornerNpeAcc{};
};
#endif
