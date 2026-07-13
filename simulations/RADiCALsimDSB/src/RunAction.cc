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
    // Fine binning over the sampled-energy range (≈12% of beam E, so 5→120 GeV
    // beam lands at ≈0.6→15 GeV sampled). 5 MeV bins resolve the narrow 5 GeV peak.
    am->CreateH1("TotalLYSO",
                 "Total LYSO energy (sampling);E_{LYSO} (GeV);Events",
                 5000, 0., 25.);

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

    // H1[6]: Differential arrival time ΔT = t_downstream − t_upstream
    // (first detected photon at each end PD; MCP reference cancels).
    // Range allows occasional negative ΔT (photon-stat fluctuations). 0.2 ps/bin.
    am->CreateH1("DeltaT",
                 "Optical timing #DeltaT (downstream #minus upstream);#DeltaT (ns);Corners",
                 4000, -0.2, 0.6);

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

    // H1[20]: Tail-catcher-corrected energy = E_LYSO + f_s*E_PbGlass.
    // Same fine binning as TotalLYSO so per-energy core fits stay clean.
    am->CreateH1("ECombined",
                 "Tail-catcher-corrected energy;E_{LYSO} + f_{s}E_{PbGlass} (GeV);Events",
                 5000, 0., 25.);

    // H1[21]: optically-detected photons per event (timing photostatistics).
    am->CreateH1("PhotonsDetected",
                 "Detected optical photons per event;N_{p.e.};Events",
                 300, 0., 60000.);

    // H1[22]: waveform-emulated ΔT (DRS4-style pulse + 5% CFD), the
    // data-identical timing estimator for direct test-beam comparison.
    am->CreateH1("DeltaT_CFD",
                 "Waveform CFD #DeltaT (downstream #minus upstream);#DeltaT (ns);Corners",
                 800, -4., 4.);

    // H1[23]: emulated pulse FWHM — validate against the measured ~8 ns.
    am->CreateH1("PulseFWHM",
                 "Emulated pulse FWHM;FWHM (ns);Pulses",
                 200, 0., 40.);

    // ── Creation-process-tagged timing (scintillation vs Cherenkov) ──────────
    // In the real device the SiPM signal is DSB1 WLS light (495 nm re-emission);
    // Cherenkov from the thin hollow capillary wall is negligible. In this sim
    // the rods are SOLID quartz, so prompt Cherenkov born along the full ~110 mm
    // rod length dominates the leading edge and imprints the shower's
    // longitudinal fluctuations onto ΔT (~45-50 ps floor). Scoring timing from
    // scintillation-origin photons only emulates the real WLS-band readout.
    // H1[24]: first-photon ΔT, scintillation-origin photons only
    am->CreateH1("DeltaT_Scint",
                 "First-photon #DeltaT, scintillation only (downstream #minus upstream);#DeltaT (ns);Corners",
                 4000, -0.2, 0.6);
    // H1[25]: waveform CFD ΔT, scintillation-origin photons only
    am->CreateH1("DeltaT_CFD_Scint",
                 "Waveform CFD #DeltaT, scintillation only;#DeltaT (ns);Corners",
                 800, -4., 4.);
    // H1[26]/H1[27]: detected photons per event by creation process
    am->CreateH1("PhotonsScint",
                 "Detected scintillation photons per event;N_{p.e.};Events",
                 300, 0., 60000.);
    am->CreateH1("PhotonsCher",
                 "Detected Cherenkov photons per event;N_{p.e.};Events",
                 300, 0., 60000.);

    // H1[28]: shower-max slice energy (LYSO layers 8-10, the WLS window) —
    // sim analog of the paper's Fig 17 (right) SiPM-sum energy estimator.
    am->CreateH1("EShowerMax",
                 "LYSO energy in shower-max slice (layers 8-10);E (GeV);Events",
                 2000, 0., 10.);

    // H1[29]/H1[30]: OpWLS-only — LYSO scint absorbed + re-emitted by DSB1,
    // the realistic RADiCAL signal path (needs LYSO optical + scaled yield).
    am->CreateH1("DeltaT_WLS",
                 "First-photon #DeltaT, WLS-shifted LYSO light only;#DeltaT (ns);Corners",
                 4000, -0.2, 0.6);
    // Range sized for LYSO yield scales up to ~2e-2 (at 5e-3, 120 GeV gives
    // ~7.5k WLS pe/event; a 6000 cap would silently clip the mean via overflow).
    am->CreateH1("PhotonsWLS",
                 "Detected WLS-shifted photons per event;N_{p.e.};Events",
                 400, 0., 40000.);

    // ── Dual-gain SiPM readout (high gain = timing, low gain = energy) ───────
    // H1[31]: LOW-GAIN energy signal = summed SiPM fired-pixel count over all
    // corners (pixel-saturated, linear electronics). The realistic energy proxy.
    am->CreateH1("EnergyLowGain",
                 "Low-gain energy signal (SiPM fired pixels, all corners);N_{fired};Events",
                 400, 0., 120000.);
    // H1[32]: HIGH-GAIN timing = fixed-threshold leading-edge #DeltaT (dn - up).
    am->CreateH1("DeltaT_HighGain",
                 "High-gain fixed-threshold #DeltaT (downstream #minus upstream);#DeltaT (ns);Corners",
                 4000, -0.2, 0.6);

    // ── H1[31-33]: DATA-MATCHED ESTIMATORS (direct test-beam comparison) ──────
    // Constructed identically to RADiCAL/Analysis so sim and real data land on
    // the same axes: per-event 4-corner average, 5% CFD, MCP-free (DW−UP)/2 for
    // timing; fiber-light sum (sum_lg analog) with containment veto for energy.

    // H1[31]: 4-corner-averaged 5% CFD ΔT, ALL detected light. σ_t = σ(ΔT)/2.
    // Same range/binning as H1[22] so per-corner vs 4-corner can be overlaid.
    am->CreateH1("DeltaT_CFD_4c",
                 "Data-matched #DeltaT (4-corner mean, 5% CFD, all light);#DeltaT (ns);Events",
                 800, -4., 4.);

    // H1[32]: same, SCINTILLATION-origin only — the physical WLS-band SiPM analog
    // (solid-rod Cherenkov excluded). This is the headline sim#minusdata number.
    am->CreateH1("DeltaT_CFD_4c_Scint",
                 "Data-matched #DeltaT (4-corner mean, 5% CFD, scint only);#DeltaT (ns);Events",
                 800, -4., 4.);

    // H1[33]: fiber-light energy = scint N_{p.e.} with containment veto — the
    // sum_lg analog (light-based, Pb-glass as veto not add-back). Fine binning
    // (10 p.e./bin) so the core fit resolves the peak at any LYSO yield scale.
    am->CreateH1("Npe_Scint_veto",
                 "Data-matched fiber-light energy (scint N_{p.e.}, veto);N_{p.e.};Events",
                 6000, 0., 60000.);

    // H1[34]: same as H1[32] (4-corner, 5% CFD, scint) but with the datasheet
    // DRS4 UNCALIBRATED-cell timebase residual added (RADICAL_DRS4_CELL_PS, PSI
    // DRS4 ±100 ps/cell). This is the honest data-comparison timing: sim light
    // physics + the DRS4 electronics term the real detector carries. σ_t=σ(ΔT)/2.
    am->CreateH1("DeltaT_CFD_4c_Scint_DRS4",
                 "Data-matched #DeltaT (4-corner, 5% CFD, scint, uncalib DRS4);#DeltaT (ns);Events",
                 800, -4., 4.);

    // H1[35]: ALL light (Cherenkov included) + datasheet DRS4 — the physically
    // FAITHFUL headline (2026-07-09 correction: the real capillary is SOLID, so
    // the SiPM does see quartz Cherenkov). Only meaningful when the WLS yield is
    // realistic enough that scint dominates the leading edge as in the real
    // device (raise RADICAL_LYSO_SCINT_SCALE); at the default 1e-3 scale
    // Cherenkov is over-weighted 1000x and this histogram overstates σ_t.
    am->CreateH1("DeltaT_CFD_4c_DRS4",
                 "Faithful #DeltaT (4-corner, 5% CFD, all light, uncalib DRS4);#DeltaT (ns);Events",
                 800, -4., 4.);

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

    // ── H2: CERN TEST-BEAM LINE ────────────────────────────────────────────

    // H2[13]: tail-catcher correlation — RADiCAL sampled E vs Pb-glass leakage E
    am->CreateH2("LYSOvsPbGlass",
                 "Tail catcher;E_{LYSO} (GeV);E_{PbGlass} (GeV)",
                 75, 0., 30.,
                 100, 0., 20.);

    // H2[14]: MCP t0 vs RADiCAL WLS arrival time
    am->CreateH2("MCPtime_vs_WLStime",
                 "Timing correlation;t_{MCP} (ns);t_{WLS} (ns)",
                 100, 0., 2.,
                 100, 0., 3.);
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
