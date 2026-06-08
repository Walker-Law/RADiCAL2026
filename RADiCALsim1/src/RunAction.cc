#include "RunAction.hh"
#include "G4AnalysisManager.hh"

RunAction::RunAction() {
    auto am = G4AnalysisManager::Instance();
    am->SetDefaultFileType("root");
    am->SetVerboseLevel(1);

    // H1: 0 — Longitudinal shower profile (energy/MeV per LYSO layer)
    am->CreateH1("ShowerProfile",
                 "Longitudinal shower profile;LYSO layer;Energy deposit (MeV)",
                 29, 0., 29.);

    // H1: 1 — Total energy deposited in LYSO (sampling signal)
    am->CreateH1("TotalLYSO",
                 "Total energy in LYSO;E (GeV);Events",
                 150, 0., 150.);

    // H1: 2 — Total energy deposited in W (absorber / invisible energy)
    am->CreateH1("TotalW",
                 "Total energy in W absorber;E (GeV);Events",
                 300, 0., 300.);

    // H1: 3 — Sampling fraction  LYSO / (LYSO + W)
    am->CreateH1("SamplingFraction",
                 "Sampling fraction LYSO/(LYSO+W);f_{s};Events",
                 100, 0., 0.5);

    // H1: 4 — Central energy capillary (EJ309 liquid) energy yield
    am->CreateH1("CenterCapEnergy",
                 "Central capillary (EJ309) energy;E (MeV);Events",
                 200, 0., 5000.);

    // H1: 5 — Corner timing capillary WLS (DSB1) energy yield (all 4 corners)
    am->CreateH1("CornerWLSEnergy",
                 "Corner WLS fibre (DSB1) energy;E (MeV);Events",
                 200, 0., 500.);

    // H1: 6 — Differential arrival time ΔT = t_back − t_front
    am->CreateH1("DeltaT",
                 "Timing capillary #DeltaT;#DeltaT (ns);Hits",
                 200, -1.0, 1.0);

    // H2: 0 — Calibration matrix: ΔT vs true z
    am->CreateH2("DeltaT_vs_TrueZ",
                 "Timing calibration;#DeltaT (ns);True z (mm)",
                 200, -1.0, 1.0,
                 200, -60., 60.);

    // H2: 1 — Position reconstruction: z_reco vs z_true
    am->CreateH2("ZReco_vs_ZTrue",
                 "Longitudinal position reco;z_{reco} (mm);z_{true} (mm)",
                 200, -60., 60.,
                 200, -60., 60.);
}

RunAction::~RunAction() {}

void RunAction::BeginOfRunAction(const G4Run*) {
    G4AnalysisManager::Instance()->OpenFile("radical_output.root");
}

void RunAction::EndOfRunAction(const G4Run*) {
    auto am = G4AnalysisManager::Instance();
    am->Write();
    am->CloseFile();
}
