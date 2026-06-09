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

    // H1[5]: Corner LuAG:Ce WLS fibre energy (all 4 corners combined)
    am->CreateH1("CornerWLSEnergy",
                 "Corner WLS fibre (LuAG:Ce) yield per corner;E (MeV);Hits",
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
                 "LuAG:Ce WLS energy per corner;Corner index;#sum E (MeV)",
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

    // ── H1: CERN TEST-BEAM LINE ────────────────────────────────────────────

    // H1[14]: Trigger scintillator 1 energy deposit
    am->CreateH1("Trig1Edep",
                 "Trigger 1 energy deposit;E (MeV);Events",
                 100, 0., 10.);

    // H1[15]: Trigger scintillator 2 energy deposit
    am->CreateH1("Trig2Edep",
                 "Trigger 2 energy deposit;E (MeV);Events",
                 100, 0., 10.);

    // H1[16]: MCP fused-silica radiator energy deposit
    am->CreateH1("MCPEdep",
                 "MCP radiator energy deposit;E (MeV);Events",
                 100, 0., 20.);

    // H1[17]: Pb-glass calorimeter energy (tail catcher / leakage)
    am->CreateH1("PbGlassEnergy",
                 "Pb-glass calorimeter energy;E (GeV);Events",
                 200, 0., 20.);

    // H1[18]: RADiCAL WLS arrival time relative to MCP t0 (key timing plot)
    am->CreateH1("WLS_minus_MCP",
                 "RADiCAL WLS time #minus MCP t_{0};t_{WLS} - t_{MCP} (ns);Events",
                 200, 0., 3.);

    // H1[19]: Beam time-of-flight, trigger 1 -> MCP
    am->CreateH1("TOF_Trig1_MCP",
                 "Beam TOF: Trig1 #rightarrow MCP;#Deltat (ns);Events",
                 100, 0., 2.);

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

    // ── H2[7-12]: Lateral shower profiles at 6 longitudinal depth slices ──────
    // Slice 0: LYSO layers  0– 4  (z ≈ −57 to −41 mm)  — entrance / early shower
    am->CreateH2("LateralProfile_Slice0",
                 "Lateral profile layers 0-4 (z #approx -57 to -41 mm);x (mm);y (mm)",
                 70, -7., 7.,
                 70, -7., 7.);

    // Slice 1: LYSO layers  5– 9  (z ≈ −41 to −21 mm)  — shower development
    am->CreateH2("LateralProfile_Slice1",
                 "Lateral profile layers 5-9 (z #approx -41 to -21 mm);x (mm);y (mm)",
                 70, -7., 7.,
                 70, -7., 7.);

    // Slice 2: LYSO layers 10–14  (z ≈ −21 to  −1 mm)  — shower maximum region
    am->CreateH2("LateralProfile_Slice2",
                 "Lateral profile layers 10-14 (z #approx -21 to -1 mm);x (mm);y (mm)",
                 70, -7., 7.,
                 70, -7., 7.);

    // Slice 3: LYSO layers 15–19  (z ≈  −1 to +19 mm)  — post-maximum
    am->CreateH2("LateralProfile_Slice3",
                 "Lateral profile layers 15-19 (z #approx -1 to +19 mm);x (mm);y (mm)",
                 70, -7., 7.,
                 70, -7., 7.);

    // Slice 4: LYSO layers 20–24  (z ≈ +19 to +39 mm)  — shower tail
    am->CreateH2("LateralProfile_Slice4",
                 "Lateral profile layers 20-24 (z #approx +19 to +39 mm);x (mm);y (mm)",
                 70, -7., 7.,
                 70, -7., 7.);

    // Slice 5: LYSO layers 25–28  (z ≈ +39 to +57 mm)  — deep tail
    am->CreateH2("LateralProfile_Slice5",
                 "Lateral profile layers 25-28 (z #approx +39 to +57 mm);x (mm);y (mm)",
                 70, -7., 7.,
                 70, -7., 7.);
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
