#include "EventAction.hh"
#include "G4Event.hh"
#include "G4AnalysisManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4Threading.hh"
#include <numeric>
#include <cmath>
#include <algorithm>
#include <chrono>

// Sentinel for "no hit yet" times (ns); any real hit time is far below this.
static const G4double kBigTime = 1.0e9;

// Per-event wall-clock timer (thread-local). Set RADICAL_TIMING=1 to enable
// a one-line "[timing]" printout per event: wall time + photons stored.
static G4ThreadLocal std::chrono::steady_clock::time_point gEvtStart;
static bool gTimingOn() {
    static const bool on = (std::getenv("RADICAL_TIMING") != nullptr);
    return on;
}

// ── DRS4 uncalibrated-timebase model (datasheet-grounded) ───────────────────
// CAEN DT5742 uses the PSI DRS4 chip. On the NOMINAL (uncalibrated) time axis
// that the RADiCAL test-beam data was written with, the effective cell width
// deviates from the nominal 0.2 ns (5 GS/s) by up to ±100 ps (PSI DRS4 manual;
// integral nonlinearity ~0.4 ns). Within one DRS4 readout group all channels
// share the stop cell, so the common accumulated error CANCELS in the (DW−UP)
// corner difference; the residual is the differential width error over the ~1
// cell separating the down- and up-edge crossings. We model that residual per
// corner as Gaussian with σ = σ_cell·√(|ΔT|/0.2 ns) — it vanishes as ΔT→0 (both
// edges in the same cell → full cancellation) and grows with edge separation.
// σ_cell = per-cell width RMS in ps via RADICAL_DRS4_CELL_PS; default 50 ps
// (≈ half the ±100 ps datasheet max, i.e. a plausible RMS). Set 0 to disable.
// This is a grounded approximation, NOT a free tune to the data's 35 ps floor.
static G4double drs4CellSigmaNs() {
    static const G4double v = (std::getenv("RADICAL_DRS4_CELL_PS")
                               ? std::atof(std::getenv("RADICAL_DRS4_CELL_PS")) : 50.0) * 1e-3;
    return v;
}

EventAction::EventAction() {
    fEdepLYSO.fill(0.);
    fEdepW.fill(0.);
    fEdepCenter = 0.;
    fEdepWLS.fill(0.);
    fEdepTrig.fill(0.);
    fTimeTrig.fill(kBigTime);
    fEdepMCP = 0.;
    fTimeMCP = kBigTime;
    fEdepPbGlass = 0.;
    fTphUp.fill(kBigTime); fTphDown.fill(kBigTime);
    fNphUp.fill(0);        fNphDown.fill(0);
    fTphUpS.fill(kBigTime); fTphDownS.fill(kBigTime);
    fNphScint = 0; fNphCher = 0;
    fTphUpW.fill(kBigTime); fTphDownW.fill(kBigTime);
    fNphWls = 0;
}
EventAction::~EventAction() {}

void EventAction::BeginOfEventAction(const G4Event*) {
    if (gTimingOn()) gEvtStart = std::chrono::steady_clock::now();
    fEdepLYSO.fill(0.);
    fEdepW.fill(0.);
    fEdepCenter = 0.;
    fEdepWLS.fill(0.);
    fCornerHits.clear();
    fLYSOHits.clear();
    fEdepTrig.fill(0.);
    fTimeTrig.fill(kBigTime);
    fEdepMCP = 0.;
    fTimeMCP = kBigTime;
    fEdepPbGlass = 0.;
    fTphUp.fill(kBigTime); fTphDown.fill(kBigTime);
    fNphUp.fill(0);        fNphDown.fill(0);
    for (auto& v : fPhTUp)   v.clear();
    for (auto& v : fPhTDown) v.clear();
    fTphUpS.fill(kBigTime); fTphDownS.fill(kBigTime);
    fNphScint = 0; fNphCher = 0;
    for (auto& v : fPhTUpS)   v.clear();
    for (auto& v : fPhTDownS) v.clear();
    fTphUpW.fill(kBigTime); fTphDownW.fill(kBigTime);
    fNphWls = 0;
}

// ── DRS4-style waveform emulation (mirrors the CERN test-beam analysis) ─────
// Build the analog pulse as a sum of single-photon responses
//   SPR(t) = (1 − e^{−t/τ_r}) · e^{−t/τ_f},  τ_r = 1.0 ns, τ_f = 3.0 ns,
// sample at 5 GS/s (0.2 ns) like the DRS4 digitizer, then apply the identical
// 5% constant-fraction discriminator with linear interpolation that the
// test-beam waveform analysis uses (per user: CFD fraction is 5%, not 50%).
// Returns CFD time (ns) or −1; optionally reports pulse FWHM for validation.
static G4double pulseCFD(const std::vector<G4double>& tns, G4double* fwhmOut) {
    if (tns.size() < 5) return -1.;
    const G4double tauR = 1.0, tauF = 3.0, dt = 0.2;     // ns
    G4double t0 = *std::min_element(tns.begin(), tns.end());
    const int NS = 500;                                   // 100 ns window
    static G4ThreadLocal std::vector<G4double> wf;        // reused buffer
    wf.assign(NS, 0.);
    for (G4double tp : tns) {
        int s0 = (int)((tp - t0) / dt) + 1;
        for (int s = s0; s < NS; s++) {
            G4double td = t0 + s * dt - tp;
            wf[s] += (1. - std::exp(-td / tauR)) * std::exp(-td / tauF);
        }
    }
    int ipk = std::max_element(wf.begin(), wf.end()) - wf.begin();
    G4double pk = wf[ipk];
    if (pk <= 0. || ipk < 1) return -1.;
    if (fwhmOut) {                                        // FWHM for validation
        int s1 = ipk, s2 = ipk;
        while (s1 > 0 && wf[s1] > pk / 2) s1--;
        while (s2 < NS - 1 && wf[s2] > pk / 2) s2++;
        *fwhmOut = (s2 - s1) * dt;
    }
    G4double thr = 0.05 * pk;                             // 5% CFD, leading edge
    int s = ipk; while (s > 0 && wf[s] > thr) s--;
    G4double v1 = wf[s], v2 = wf[s + 1];
    return t0 + (s + (v2 > v1 ? (thr - v1) / (v2 - v1) : 0.)) * dt;
}

// ── Dual-gain SiPM readout (emulates the experiment's high/low gain channels) ─
static G4double envD(const char* k, G4double dflt) {
    const char* s = std::getenv(k); return s ? std::atof(s) : dflt;
}

// SiPM pixel saturation: a SiPM has a FINITE number of microcells (pixels), so
// the fired-pixel count saturates as N_fired = Npix*(1 - exp(-Ndet/Npix)).
// Linear at Ndet << Npix, saturates to Npix at Ndet >> Npix. This is the sensor
// nonlinearity the low-gain energy channel must live with (and why a single
// linear "photon count" over-reads the true energy at high light). Npix<=0
// disables it. Default 5676 = onsemi J-Series MicroFJ-30035-TSV datasheet
// (Table 2: 3.07x3.07 mm, 35 um microcell, 75% fill, 5676 microcells) — the
// ACTUAL RADiCAL sensor (per Walker). The large 35 um cell means only 5676
// pixels, so saturation is FIRST-ORDER here: at the 2% yield each SiPM sees
// ~2000 fired -> ~35% occupancy -> ~30% saturation at 120 GeV. This is exactly
// the nonlinearity the low-gain energy channel must handle. (NOTE: crosstalk
// 8-25% and afterpulsing are NOT modeled — they would raise N_fired ~10-20%.)
static G4double sipmNfired(G4double nDet) {
    static G4double npix = envD("RADICAL_SIPM_NPIX", 5676.);
    if (npix <= 0.) return nDet;
    return npix * (1. - std::exp(-nDet / npix));
}

// High-gain leading-edge timing: the SPR-sum pulse (normalized to unit peak per
// photon) crossing an ABSOLUTE threshold of thrPE photoelectrons. High gain =
// a small fixed threshold sitting low on the steep early leading edge -> the
// sharpest timing reference (the paper's "fixed threshold on the high-gain
// signal"). Unlike 5% CFD (scale-invariant), a fixed threshold rewards high
// gain: more light -> steeper rise through the threshold -> less time jitter.
// Returns crossing time (ns) or -1 if the pulse never reaches thrPE.
static G4double leadingEdgeFixed(const std::vector<G4double>& tns, G4double thrPE) {
    if (tns.empty()) return -1.;
    const G4double tauR = 1.0, tauF = 3.0, dt = 0.2;      // ns, same SPR as pulseCFD
    const G4double sprNorm = 1.0 / 0.472;                 // single-photon peak -> 1
    G4double t0 = *std::min_element(tns.begin(), tns.end());
    const int NS = 500;
    static G4ThreadLocal std::vector<G4double> wf;
    wf.assign(NS, 0.);
    for (G4double tp : tns) {
        int s0 = (int)((tp - t0) / dt) + 1;
        for (int s = s0; s < NS; s++) {
            G4double td = t0 + s * dt - tp;
            wf[s] += sprNorm * (1. - std::exp(-td / tauR)) * std::exp(-td / tauF);
        }
    }
    for (int s = 1; s < NS; s++) {                        // first upward crossing
        if (wf[s] >= thrPE) {
            G4double v1 = wf[s - 1], v2 = wf[s];
            return t0 + (s - 1 + (v2 > v1 ? (thrPE - v1) / (v2 - v1) : 0.)) * dt;
        }
    }
    return -1.;
}

void EventAction::EndOfEventAction(const G4Event*) {
    auto am = G4AnalysisManager::Instance();

    // =========================================================================
    // 1. LONGITUDINAL SHOWER PROFILE + SHOWER SHAPE OBSERVABLES
    // =========================================================================
    G4double totalLYSO = 0.;
    G4int    showerMaxLayer = 0;
    G4double maxLayerEdep   = 0.;
    G4double cogNumer = 0., cogDenom = 0.;
    G4double rmsNumer = 0.;

    for (G4int i = 0; i < 29; i++) {
        G4double e = fEdepLYSO[i];
        if (e > 0.) {
            am->FillH1(0, i + 0.5, e / MeV);   // shower profile (weighted fill)
            if (e > maxLayerEdep) { maxLayerEdep = e; showerMaxLayer = i; }
            cogNumer += (i + 0.5) * e;
            cogDenom += e;
        }
        totalLYSO += e;
    }

    G4double cog = (cogDenom > 0.) ? cogNumer / cogDenom : 0.;  // in layer units

    // Beam-core event selection (paper procedure): the test beam selected
    // well-centred events (position reconstruction / "events where measured
    // energy matched beam energy closely") before quoting shower-max resolution.
    // Our sigma=2.9 mm beam spot adds an energy-INDEPENDENT impact-position
    // variance to the shower-max slice; without the selection it masquerades as
    // a large flat constant term (~16% at all E, where the paper falls
    // 14.5% -> 10.2% over 25-150 GeV). RADICAL_SM_COG_CUT_MM > 0 keeps only
    // events whose lateral energy-weighted COG is within that radius of the
    // module axis — a data-like cut (COG is reconstructable in the real module).
    // Default 0 = off (no behaviour change).
    bool smInCore = true;
    {
        static G4ThreadLocal G4double cogCut = -1.;
        if (cogCut < 0.) cogCut = envD("RADICAL_SM_COG_CUT_MM", 0.);
        if (cogCut > 0.) {
            G4double cx = 0., cy = 0., ce = 0.;
            for (const auto& h : fLYSOHits) { cx += h.x*h.edep; cy += h.y*h.edep; ce += h.edep; }
            smInCore = (ce > 0.) &&
                (std::sqrt(cx/ce*cx/ce + cy/ce*cy/ce) < cogCut*mm);
        }
    }

    // Shower-max slice energy — the sim analog of the paper's Fig 17 (right):
    // LYSO deposit in the layers the 15 mm WLS window covers (8-10, centers
    // 36.0/40.4/44.8 mm, window 32.9-47.9 mm). The real SiPM amplitude is LYSO
    // light from this slab collected via the WLS fibers, so its resolution is
    // set by this slice's fluctuations — NOT by full-module containment (H1[20])
    // and NOT by dE/dx in the thin fibers (H1[13], ~95% RMS/mean).
    {
        G4double eSM = fEdepLYSO[8] + fEdepLYSO[9] + fEdepLYSO[10];
        if (eSM > 0. && smInCore) am->FillH1(28, eSM / GeV);
    }

    // RMS of longitudinal distribution
    if (cogDenom > 0.) {
        for (G4int i = 0; i < 29; i++) {
            G4double dl = (i + 0.5) - cog;
            rmsNumer += fEdepLYSO[i] * dl * dl;
        }
    }
    G4double showerRMS = (cogDenom > 0.) ? std::sqrt(rmsNumer / cogDenom) : 0.;

    // =========================================================================
    // 2. ABSORBER + SUMMARY
    // =========================================================================
    G4double totalW = 0.;
    for (G4int i = 0; i < 28; i++) totalW += fEdepW[i];

    if (totalLYSO > 0.) am->FillH1(1, totalLYSO / GeV);
    if (totalW    > 0.) am->FillH1(2, totalW    / GeV);
    G4double totalActive = totalLYSO + totalW;
    if (totalActive > 0.) am->FillH1(3, totalLYSO / totalActive);

    // =========================================================================
    // 3. CAPILLARY SIGNALS
    // =========================================================================
    if (fEdepCenter > 0.) {
        am->FillH1(4, fEdepCenter / MeV);
        // H1[10]: center cap / total LYSO ratio
        if (totalLYSO > 0.)
            am->FillH1(10, (fEdepCenter / MeV) / (totalLYSO / MeV));
        // H2[3]: EJ309 vs total LYSO linearity
        am->FillH2(3, totalLYSO / GeV, fEdepCenter / MeV);
    }

    G4double totalCornerWLS = 0.;
    for (G4int c = 0; c < 4; c++) {
        if (fEdepWLS[c] > 0.) {
            am->FillH1(5, fEdepWLS[c] / MeV);          // all corners combined
            am->FillH1(11, c + 0.5, fEdepWLS[c] / MeV); // per-corner bar
            totalCornerWLS += fEdepWLS[c];
        }
    }
    if (totalCornerWLS > 0.) {
        am->FillH1(13, totalCornerWLS / MeV);
        // H2[4]: total corner WLS vs total LYSO
        if (totalLYSO > 0.)
            am->FillH2(4, totalLYSO / GeV, totalCornerWLS / MeV);
    }

    // =========================================================================
    // 4. SHOWER SHAPE HISTOGRAMS (per-event)
    // =========================================================================
    if (totalLYSO > 0.) {
        am->FillH1(7, showerMaxLayer + 0.5);    // shower max layer
        am->FillH1(8, cog);                      // longitudinal COG (layer units)
        am->FillH1(9, showerRMS);                // shower longitudinal RMS
        // H2[6]: shower max vs total LYSO
        am->FillH2(6, totalLYSO / GeV, showerMaxLayer + 0.5);
    }

    // =========================================================================
    // 5. X-Y LATERAL SHOWER PROFILE — integrated + 6 depth slices
    // =========================================================================
    // Mapping: layer index → slice H2 id
    //   Slice 0 → H2[7]  layers  0– 4  (z ≈ −57 to −41 mm)
    //   Slice 1 → H2[8]  layers  5– 9  (z ≈ −41 to −21 mm)
    //   Slice 2 → H2[9]  layers 10–14  (z ≈ −21 to  −1 mm)  ← shower max
    //   Slice 3 → H2[10] layers 15–19  (z ≈  −1 to +19 mm)
    //   Slice 4 → H2[11] layers 20–24  (z ≈ +19 to +39 mm)
    //   Slice 5 → H2[12] layers 25–28  (z ≈ +39 to +57 mm)
    auto depthSliceH2 = [](G4int layer) -> G4int {
        if (layer <=  4) return 7;
        if (layer <=  9) return 8;
        if (layer <= 14) return 9;
        if (layer <= 19) return 10;
        if (layer <= 24) return 11;
        return 12;
    };

    for (const auto& hit : fLYSOHits) {
        // H2[2]: integrated X-Y map (all layers)
        am->FillH2(2, hit.x / mm, hit.y / mm, hit.edep / MeV);
        // H2[7-12]: depth-sliced X-Y map
        G4int sliceId = depthSliceH2((G4int)hit.layer);
        am->FillH2(sliceId, hit.x / mm, hit.y / mm, hit.edep / MeV);
    }

    // =========================================================================
    // 6. TIMING RECONSTRUCTION
    // =========================================================================
    const G4double stackHalfZ = 57.03 * mm;
    const G4double c_light    = 299.792458 * mm / ns;
    const G4double v_quartz   = c_light / 1.46;

    std::array<G4double, 4> cornerEdepSum = {0, 0, 0, 0};
    std::array<G4double, 4> cornerZSum    = {0, 0, 0, 0};
    std::array<G4double, 4> cornerTSum    = {0, 0, 0, 0};

    for (const auto& hit : fCornerHits) {
        int c = (int)hit.corner;
        cornerEdepSum[c] += hit.edep;
        cornerZSum[c]    += hit.edep * hit.z;
        cornerTSum[c]    += hit.edep * hit.t;
    }

    for (G4int c = 0; c < 4; c++) {
        if (cornerEdepSum[c] <= 0.) continue;
        G4double zHit = cornerZSum[c] / cornerEdepSum[c];
        G4double tHit = cornerTSum[c] / cornerEdepSum[c];

        G4double distUpstream   = zHit - (-stackHalfZ);
        G4double distDownstream = stackHalfZ - zHit;
        G4double tArrUpstream   = tHit + distUpstream   / v_quartz;
        G4double tArrDownstream = tHit + distDownstream / v_quartz;
        G4double deltaT    = tArrDownstream - tArrUpstream;
        G4double zReco     = -deltaT * v_quartz / 2.0;
        G4double zResid    = zReco - zHit;

        // Geometric z-reconstruction diagnostics (kept from the deposition model)
        am->FillH1(12, zResid / mm);                    // Z residual
        am->FillH2(0,  deltaT / ns, zHit / mm);         // DeltaT vs true z (geometric)
        am->FillH2(1,  zReco  / mm, zHit / mm);         // z_reco vs z_true
    }

    // =========================================================================
    // 6b. OPTICAL-PHOTON TIMING (the real measurement)
    //   Per corner, ΔT = t_downstream − t_upstream of the FIRST detected photon
    //   at each end PD (leading-edge). H1[6] = ΔT (raw difference, NOT ÷2).
    //
    //   (DW−UP)/2 corner trick (jwwetzel.github.io/RADiCAL/going_radical.html):
    //   ΔT = (L − 2z)/v_g, so σ(ΔT) = 2·σ_z/v_g = 2·σ_t.
    //   Physical timing resolution: σ_t = RMS(H1[6]) / 2.
    //   The /2 also cancels MCP jitter, DRS4 timebase error, and beam-arrival
    //   jitter — all common-mode in the DW−UP subtraction.
    // =========================================================================
    G4int nPhotTot = 0;
    for (G4int c = 0; c < 4; c++) {
        nPhotTot += fNphUp[c] + fNphDown[c];
        if (fTphUp[c] < kBigTime && fTphDown[c] < kBigTime) {
            G4double dT = fTphDown[c] - fTphUp[c];      // downstream − upstream (positive)
            am->FillH1(6, dT / ns);                     // optical ΔT
            if (totalLYSO > 0.) am->FillH2(5, totalLYSO / GeV, dT / ns);
        }
    }
    if (nPhotTot > 0) am->FillH1(21, nPhotTot);         // photons detected / event

    // =========================================================================
    // 6c. WAVEFORM-EMULATED TIMING (data-identical estimator)
    //   Pulse built from ALL detected photon times, digitized DRS4-style, then
    //   5% CFD — the same estimator as the CERN test-beam waveform analysis.
    //   H1[22] = per-corner ΔT_CFD (downstream − upstream), H1[23] = pulse FWHM.
    //   H1[31] = the DATA-MATCHED estimator: the 4 corners are averaged PER EVENT
    //   before differencing, exactly like test-beam Method A
    //   (RADiCAL/Analysis/timingEnergyBins.C: mean{DW} − mean{UP}). This √4
    //   per-event averaging is why H1[31] < H1[22]; analysis reports σ_t=σ/2.
    // =========================================================================
    G4double sumDT = 0., sumDT_drs4 = 0.; G4int nDT = 0;   // 4-corner accumulators (all light)
    const G4double sCellA = drs4CellSigmaNs();
    for (G4int c = 0; c < 4; c++) {
        G4double fwUp = -1., fwDn = -1.;
        G4double tUp = pulseCFD(fPhTUp[c],   &fwUp);
        G4double tDn = pulseCFD(fPhTDown[c], &fwDn);
        if (tUp > 0. && tDn > 0.) {
            G4double dtc = tDn - tUp;
            am->FillH1(22, dtc / 1.0);           // already in ns (per-corner)
            sumDT += dtc; ++nDT;                 // accumulate for the 4-corner mean
            // datasheet DRS4 uncalibrated-cell residual (same model as H1[34]):
            // ALL light + DRS4 is the physically faithful headline once the WLS
            // yield is realistic (real capillary is solid -> Cherenkov is real).
            G4double ec = (sCellA > 0.)
                ? G4RandGauss::shoot(0., sCellA * std::sqrt(std::fabs(dtc) / 0.2)) : 0.;
            sumDT_drs4 += (dtc + ec);
            if (fwUp > 0.) am->FillH1(23, fwUp);
            if (fwDn > 0.) am->FillH1(23, fwDn);
        }
    }
    if (nDT > 0) {
        am->FillH1(31, sumDT      / nDT);    // H1[31] data-matched, all light, ideal DRS4
        am->FillH1(35, sumDT_drs4 / nDT);    // H1[35] all light + datasheet DRS4 (faithful headline)
    }

    // =========================================================================
    // 6d. SCINTILLATION-ONLY TIMING (Cherenkov excluded)
    //   Emulates the real WLS-band SiPM signal: in the physical device the
    //   quartz sections are thin-wall hollow capillaries (negligible Cherenkov)
    //   and the SiPM sees DSB1's 495 nm re-emission. In this sim the rods are
    //   solid quartz, so prompt Cherenkov dominates the leading edge and sets
    //   an artificial ~45-50 ps floor tied to shower-depth fluctuations.
    //   H1[32] = data-matched (4-corner mean per event, 5% CFD) scint-only —
    //   the closest apples-to-apples analog of the real detector's σ_t.
    // =========================================================================
    G4double sumDTS = 0., sumDTS_drs4 = 0.; G4int nDTS = 0;  // 4-corner accumulators (scint)
    const G4double sCell = drs4CellSigmaNs();
    for (G4int c = 0; c < 4; c++) {
        if (fTphUpS[c] < kBigTime && fTphDownS[c] < kBigTime)
            am->FillH1(24, (fTphDownS[c] - fTphUpS[c]) / ns);
        G4double fw = -1.;
        G4double tUpS = pulseCFD(fPhTUpS[c],   &fw);
        G4double tDnS = pulseCFD(fPhTDownS[c], &fw);
        if (tUpS > 0. && tDnS > 0.) {
            G4double dtc = tDnS - tUpS;
            am->FillH1(25, dtc / 1.0);         // per-corner
            sumDTS += dtc; ++nDTS;             // clean 4-corner mean (H1[32])
            // datasheet DRS4 uncalibrated-cell residual (differential, per corner)
            G4double ec = (sCell > 0.)
                ? G4RandGauss::shoot(0., sCell * std::sqrt(std::fabs(dtc) / 0.2)) : 0.;
            sumDTS_drs4 += (dtc + ec);         // DRS4-processed 4-corner mean (H1[34])
        }
    }
    if (nDTS > 0) {
        am->FillH1(32, sumDTS      / nDTS);    // H1[32] data-matched, scint, ideal DRS4
        am->FillH1(34, sumDTS_drs4 / nDTS);    // H1[34] data-matched, scint, uncalibrated DRS4
    }
    if (fNphScint > 0) am->FillH1(26, fNphScint);
    if (fNphCher  > 0) am->FillH1(27, fNphCher);

    // 6e. WLS-ONLY TIMING — the realistic RADiCAL chain (LYSO 420 nm light
    //     absorbed by DSB1, re-emitted at 495 nm, creator process "OpWLS").
    //     First-photon ΔT per corner; population is small at scaled LYSO yield,
    //     so photostatistics extrapolate as sqrt(RADICAL_LYSO_SCINT_SCALE).
    for (G4int c = 0; c < 4; c++) {
        if (fTphUpW[c] < kBigTime && fTphDownW[c] < kBigTime)
            am->FillH1(29, (fTphDownW[c] - fTphUpW[c]) / ns);
    }
    if (fNphWls > 0) am->FillH1(30, fNphWls);

    // 6f. DUAL-GAIN SiPM READOUT (experiment's high/low gain channels)
    //   Both channels use ALL detected light (Cherenkov INCLUDED) — the real
    //   SiPM cannot distinguish photon origins. This is faithful ONLY when the
    //   run restores the physical scint:Cherenkov ratio by scaling every light
    //   source uniformly, e.g. the "2% universe":
    //     RADICAL_LYSO_SCINT_SCALE=2e-2  RADICAL_SCINT_YIELD=0.02
    //     RADICAL_KEEP_LYSO_CHER=1       RADICAL_QUARTZ_CHER_KEEP=0.02
    //   (all sources at 2% of physical -> ratios exact, statistics /50).
    //   At non-uniform scalings this estimator over-weights Cherenkov; use the
    //   scint/WLS-only histograms (H1[24..30]) as the proxy in that regime.
    //   Each channel applies its own response, split by gain:
    //     HIGH GAIN -> TIMING: fixed-threshold leading edge. Big amplification
    //       puts a low fixed threshold on the steep early edge -> sharp t_ref.
    //     LOW  GAIN -> ENERGY: integrate the signal = SiPM fired-pixel count
    //       (saturated), linear electronics, no clip -> full dynamic range.
    {
        const G4double thrHG = envD("RADICAL_HG_THRESH_PE", 2.5);   // pe
        G4double eLowGain = 0.;
        G4double sumHGdt = 0.; G4int nHG = 0;          // per-event 4-corner mean ΔT
        for (G4int c = 0; c < 4; c++) {
            // low-gain ENERGY: integrated charge ~ fired pixels (both ends)
            eLowGain += sipmNfired((G4double)fPhTUp[c].size());
            eLowGain += sipmNfired((G4double)fPhTDown[c].size());
            // high-gain TIMING: fixed-threshold leading edge, downstream - upstream
            G4double tHGu = leadingEdgeFixed(fPhTUp[c],   thrHG);
            G4double tHGd = leadingEdgeFixed(fPhTDown[c], thrHG);
            if (tHGu > 0. && tHGd > 0.) {
                G4double d = tHGd - tHGu;
                am->FillH1(37, d);                     // H1[37] per-corner high-gain ΔT
                sumHGdt += d; ++nHG;
            }
        }
        if (eLowGain > 0.) am->FillH1(36, eLowGain);   // H1[36] low-gain energy
        // H2[15]: energy vs timing, for the paper's energy-binned (time-walk-
        // corrected) sigma_t. Uses the per-event 4-corner-mean ΔT.
        if (eLowGain > 0. && nHG > 0)
            am->FillH2(15, eLowGain, sumHGdt / nHG);
    }

    // =========================================================================
    // 7. CERN TEST-BEAM LINE OBSERVABLES
    //    Trigger counters, MCP timing reference (t0), Pb-glass tail catcher.
    // =========================================================================
    if (fEdepTrig[0] > 0.) am->FillH1(14, fEdepTrig[0] / MeV);  // trigger 1 dE
    if (fEdepTrig[1] > 0.) am->FillH1(15, fEdepTrig[1] / MeV);  // trigger 2 dE
    if (fEdepMCP    > 0.) am->FillH1(16, fEdepMCP     / MeV);  // MCP radiator dE
    if (fEdepPbGlass > 0.) am->FillH1(17, fEdepPbGlass / GeV);  // Pb-glass energy

    // Beam time-of-flight: trigger 1 -> MCP (both seen by the primary)
    if (fTimeTrig[0] < kBigTime && fTimeMCP < kBigTime)
        am->FillH1(19, (fTimeMCP - fTimeTrig[0]) / ns);

    // Energy-weighted mean WLS arrival time across all 4 corners
    G4double wlsESum = 0., wlsTSum = 0.;
    for (const auto& hit : fCornerHits) { wlsESum += hit.edep; wlsTSum += hit.edep * hit.t; }
    G4double wlsMeanT = (wlsESum > 0.) ? wlsTSum / wlsESum : kBigTime;

    // RADiCAL timing relative to the MCP reference (t0): the key resolution plot
    if (wlsESum > 0. && fTimeMCP < kBigTime) {
        am->FillH1(18, (wlsMeanT - fTimeMCP) / ns);             // H1[18]
        am->FillH2(14, fTimeMCP / ns, wlsMeanT / ns);           // H2[14]
    }

    // Tail-catcher correlation: RADiCAL sampled energy vs Pb-glass leakage energy
    if (totalLYSO > 0.)
        am->FillH2(13, totalLYSO / GeV, fEdepPbGlass / GeV);    // H2[13]

    // Tail-catcher-corrected energy estimator (H1[20]):
    //   E_comb = E_LYSO + f_s * E_PbGlass,  f_s = LYSO sampling fraction.
    // The −0.94 LYSO/PbGlass anti-correlation means restoring the (sampling-scaled)
    // forward leakage cancels the leakage fluctuation, tightening sigma/E.
    static const G4double kSamplingFrac = 0.18;
    G4double eComb = totalLYSO + kSamplingFrac * fEdepPbGlass;

    // Beam-acceptance cut: reject halo events that missed the ±7 mm module and
    // showered straight into the Pb-glass (these form a spurious sharp peak).
    // A real test beam removes these via trigger/tracking. Keep events where the
    // module-reconstructed energy exceeds the tail-catcher energy (>50% in module);
    // this preserves genuine forward-leakage events while cutting clean misses.
    G4double eModuleReco = totalLYSO / kSamplingFrac;
    bool inAcceptance = (eModuleReco > fEdepPbGlass);
    if (eComb > 0. && inAcceptance) am->FillH1(20, eComb / GeV);   // H1[20]

    // ── DATA-MATCHED ENERGY (H1[33]) — fiber-light sum, containment-veto style ──
    //   Test-beam energy = Σ of the 8 corner-fiber LIGHT peaks (sum_lg,
    //   RADiCAL/Analysis/processRun.C) with a Pb-glass CONTAINMENT VETO (reject
    //   sum_pb > 0.30·sum_lg) — the OPPOSITE of H1[20]'s tail-catcher ADD-BACK.
    //   Sim analog of sum_lg = detected scintillation-origin fiber light
    //   (fNphScint). The data's halo/pion veto is largely inert on the clean sim
    //   beam, so we reuse the beam-acceptance cut (module reco > Pb-glass) as the
    //   veto. CAVEAT: N_pe carries the LYSO-yield scaling (RADICAL_LYSO_SCINT_SCALE),
    //   so σ/mean has a photostatistics floor set by that yield — trust the
    //   absolute σ/E only at a realistic yield; the trend vs E is robust.
    if (fNphScint > 0 && inAcceptance) am->FillH1(33, fNphScint);

    // ── Optional per-event timing diagnostic (RADICAL_TIMING=1) ──────────────
    if (gTimingOn()) {
        auto dt = std::chrono::duration<double>(
                      std::chrono::steady_clock::now() - gEvtStart).count();
        size_t nph = 0;
        for (int c = 0; c < 4; c++) nph += fPhTUp[c].size() + fPhTDown[c].size();
        G4cout << "[timing] T" << G4Threading::G4GetThreadId()
               << "  evt wall=" << dt << " s"
               << "  E_LYSO=" << totalLYSO / GeV << " GeV"
               << "  photons_stored=" << nph
               << (nph >= 4 * 60000 ? " (CAPPED)" : "") << G4endl;
    }
}
