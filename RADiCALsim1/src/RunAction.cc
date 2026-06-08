#include "RunAction.hh"
#include "G4AnalysisManager.hh"

RunAction::RunAction() {
    auto am = G4AnalysisManager::Instance();
    am->SetDefaultFileType("root");
    am->SetVerboseLevel(1);

    // ── H1: EXISTING ─────────────────────────────────────────────────────────

    // H1[0]: Longitudinal shower profile — energy per LYSO layer
    am->CreateH1("ShowerProfile",
                 "Longitudinal shower profile;LYSO layer;Energy deposit (MeV)",
                 29, 0., 29.);

    // H1[1]: Total energy deposited in LYSO (sampling signal)
    am->CreateH1("TotalLYSO",
                 "Total LYSO energy (sampling);E_{LYSO} (GeV);Events",
                 150, 0., 150.);

    // H1[2]: Total energy deposited in W (absorber / invisible energy)
    am->CreateH1("TotalW",
                 "Total W absorber energy;E_{W} (GeV);Events",
                 300, 0., 300.);

    // H1[3]: Sampling fraction LYSO / (LYSO + W)
    am->CreateH1("SamplingFraction",
                 "Sampling fraction;f_{s} = E_{LYSO}/(E_{LYSO}+E_{W});Events",
                 100, 0., 0.5);

    // H1[4]: Central energy capillary (EJ309 liquid)
    am->CreateH1("CenterCapEnergy",
                 "Central capillary (EJ309) yield;E (MeV);Events",
                 200, 0., 5000.);

    // H1[5]: Corner DSB1 WLS fibre energy (all 4 corners combined)
    am->CreateH1("CornerWLSEnergy",
                 "Corner WLS fibre (DSB1) yield per corner;E (MeV);Hits",
                 200, 0., 500.);

    // H1[6]: Differential arrival time ΔT = t_back − t_front
    am->CreateH1("DeltaT",
                 "Timing capillary #DeltaT;#DeltaT (ns);Hits",
                 200, -1.0, 1.0);

    // ── H1: SHOWER SHAPE ─────────────────────────────────────────────────────

    // H1[7]: Shower maximum — LYSO layer with peak energy deposit
    am->CreateH1("ShowerMaxLayer",
                 "Shower maximum layer;LYSO layer index;Events",
                 29, 0., 29.);

    // H1[8]: Longitudinal centre-of-gravity (energy-weighted mean layer)
    am->CreateH1("ShowerCOG",
                 "Longitudinal centre of gravity;#bar{layer} (energy-weighted);Events",
                 58, 0., 29.);

    // H1[9]: Longitudinal shower RMS width (in layer units)
    am->CreateH1("ShowerRMS",
                 "Shower longitudinal RMS width;#sigma_{z} (layers);Events",
                 60, 0., 15.);

    // ── H1: CAPILLARY RESPONSE ───────────────────────────────────────────────

    // H1[10]: Center cap energy / total LYSO (capillary calibration fraction)
    am->CreateH1("CenterCapFraction",
                 "Central capillary fraction;E_{EJ309}/E_{LYSO};Events",
                 100, 0., 0.5);

    // H1[11]: Per-corner WLS energy — bar chart (x = corner index 0-3)
    am->CreateH1("CornerWLSPerCorner",
                 "DSB1 WLS energy per corner;Corner index;#sum E (MeV)",
                 4, 0., 4.);

    // ── H1: TIMING ───────────────────────────────────────────────────────────

    // H1[12]: Z-position reconstruction residual (z_reco - z_true)
    am->CreateH1("ZResidual",
                 "Longitudinal position residual;z_{reco} - z_{true} (mm);Hits",
                 100, -30., 30.);

    // H1[13]: Total corner WLS energy (sum of all 4 corners per event)
    am->CreateH1("TotalCornerWLS",
                 "Total corner WLS energy (4 corners);E_{WLS,total} (MeV);Events",
                 200, 0., 2000.);

    // ── H2: EXISTING ─────────────────────────────────────────────────────────

    // H2[0]: Timing calibration — ΔT vs true z
    am->CreateH2("DeltaT_vs_TrueZ",
                 "Timing calibration matrix;#DeltaT (ns);z_{true} (mm)",
                 100, -1.0, 1.0,
                 100, -60., 60.);

    // H2[1]: Position reconstruction — z_reco vs z_true (should be diagonal)
    am->CreateH2("ZReco_vs_ZTrue",
                 "Position reconstruction;z_{reco} (mm);z_{true} (mm)",
                 100, -60., 60.,
                 100, -60., 60.);

    // ── H2: NEW ──────────────────────────────────────────────────────────────

    // H2[2]: Lateral shower profile — X vs Y energy map in LYSO
    am->CreateH2("LateralProfile",
                 "Lateral shower profile (LYSO);x (mm);y (mm)",
                 70, -7., 7.,
                 70, -7., 7.);

    // H2[3]: EJ309 capillary yield vs total LYSO — linearity check
    am->CreateH2("CenterCapVsLYSO",
                 "Central capillary vs LYSO;E_{LYSO} (GeV);E_{EJ309} (MeV)",
                 75, 0., 150.,
                 100, 0., 5000.);

    // H2[4]: Total corner WLS vs total LYSO — correlation
    am->CreateH2("CornerWLSVsLYSO",
                 "Corner WLS vs LYSO;E_{LYSO} (GeV);E_{WLS} (MeV)",
                 75, 0., 150.,
                 100, 0., 2000.);

    // H2[5]: ΔT vs total LYSO — timing resolution as function of energy
    am->CreateH2("DeltaTVsLYSO",
                 "#DeltaT vs sampled energy;E_{LYSO} (GeV);#DeltaT (ns)",
                 75, 0., 150.,
                 100, -1.0, 1.0);

    // H2[6]: Shower max layer vs total LYSO — shower depth vs energy
    am->CreateH2("ShowerMaxVsLYSO",
                 "Shower max depth vs energy;E_{LYSO} (GeV);Max layer index",
                 75, 0., 150.,
                 29, 0., 29.);
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
