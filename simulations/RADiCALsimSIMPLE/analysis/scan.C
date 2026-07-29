// scan.C — resolution curves from the run.mac energy sweep.
// Usage (from RADiCALsimSIMPLE/):   root -l -b -q analysis/scan.C
//
// Reads build/E5GeV.root ... E120GeV.root (must match run.mac's energy list)
// and produces, in build/plots/:
//   sigma_t_vs_E.png          timing resolution sigma_t = sigma(dT)/2 vs E,
//                             fitted to sigma_t(E) = a/sqrt(E) (+) b
//   sigma_E_vs_E.png          energy resolution sigma_E/E vs E (in %),
//                             fitted to the same a/sqrt(E) (+) b form
//   fits/dT_E<N>GeV.png       the dT distribution AT EACH ENERGY with its fit —
//                             the histogram each sigma_t point came from
//   fits/Elyso_E<N>GeV.png    the LYSO-energy spread AT EACH ENERGY with its fit —
//                             the histogram each sigma_E/E point came from
//
// Every resolution number is a Gaussian CORE fit: fit once over the full range,
// then refit within mu +- 2 sigma. The second pass keeps leakage/straggler
// tails from inflating the width (visible in the per-energy PNGs: the curve is
// drawn only over the range actually fitted).
//
// REMINDER (README "Knobs"): at RADSIMPLE_LYSO_SCALE = f (default 1e-2) the
// photon-statistics part of sigma_t is inflated by ~1/sqrt(f); the energy
// resolution is a pure dE/dx quantity and does NOT depend on f.

// Two-pass Gaussian core fit. Fills mu, sigma, sigmaErr; draws h + fit into png.
//   pass 1: fit within the histogram's mean +- 3*RMS,
//   pass 2: refit within mu +- 2*sigma (the Gaussian CORE, tails excluded).
void coreFitAndSave(TH1* h, const char* title, const char* png,
                    double& mu, double& sg, double& sgErr) {
    double m = h->GetMean(), r = h->GetRMS();
    h->Fit("gaus", "Q0", "", m - 3*r, m + 3*r);              // pass 1
    TF1* g = h->GetFunction("gaus");
    mu = g->GetParameter(1); sg = g->GetParameter(2);
    h->Fit("gaus", "Q0", "", mu - 2*sg, mu + 2*sg);          // pass 2: core
    g = h->GetFunction("gaus");
    mu = g->GetParameter(1); sg = g->GetParameter(2); sgErr = g->GetParError(2);

    auto c = new TCanvas("cfit", "", 700, 500);
    h->SetTitle(title);
    h->GetXaxis()->SetRangeUser(mu - 6*sg, mu + 6*sg);       // zoom to the peak
    h->Draw("hist");
    g->SetLineColor(kRed); g->Draw("same");
    c->SaveAs(png);
    delete c;
}

// Draws the fit's functional form (e.g. "sigma_t = a/sqrt(E) (+) b") on the
// current pad, so it's legible next to the stat box's numeric a/b values
// (ROOT's default box would otherwise just say "p0"/"p1" with no formula).
void drawFormula(const char* latex, double x = 0.15, double y = 0.82) {
    TLatex t;
    t.SetNDC(true);
    t.SetTextSize(0.045);
    t.DrawLatex(x, y, latex);
}

// dir = folder holding E<N>GeV.root (default "build"). Pass another folder to
// analyze a second run without clobbering the first, e.g.
//   root -l -b -q 'analysis/scan.C("build/short")'
// Plots are written to <dir>/plots/ so each run keeps its own.
// ---------------------------------------------------------------------------
// BEAM FIDUCIAL CUT — required, not cosmetic (added 2026-07-29).
//
// The beam is a 2.9 mm Gaussian spot on a 14 x 14 mm tile that has FIVE holes
// drilled through it. Three distinct pathologies therefore contaminate an
// uncut sample, all of them acceptance rather than detector physics:
//
//   beam radius   <E_LYSO>   sigma/E     what is happening
//   0-1 mm        3.76 GeV     82%       straight down the CENTRAL hole
//   1-4 mm        6.5  GeV    2-3%       <- the module actually working
//   4-5 mm        5.42 GeV     33%       down a CORNER capillary hole (r=4.95)
//   5-10 mm       4.24 GeV     47%       clipping the tile edge, or missing
//                                        entirely (5.4% of events: for sigma =
//                                        2.9 mm, P(r>7mm) = exp(-7^2/2/2.9^2))
//
// Uncut, these average into a ~31% "resolution" that is FLAT in energy — the
// signature of a geometric effect, not sampling statistics. They also drop the
// timing efficiency to ~93%, and because the lost events are the dim ones the
// surviving sigma_t reads optimistically.
//
// The real experiment cuts the same way: arXiv:2303.05580 sec 3 explicitly
// rejects "particles through the central hole".
//
// Default below keeps ~39% of events, restores 100% timing efficiency, and
// recovers proper 1/sqrt(E) scaling (5.6% -> 1.9% over 5-120 GeV). Pass "" to
// disable and see the raw sample.
const char* kHoleDist =
    "min(min(sqrt(x*x+y*y),"
    "min(sqrt((x-3.5)*(x-3.5)+(y-3.5)*(y-3.5)),sqrt((x+3.5)*(x+3.5)+(y+3.5)*(y+3.5)))),"
    "min(sqrt((x-3.5)*(x-3.5)+(y+3.5)*(y+3.5)),sqrt((x+3.5)*(x+3.5)+(y-3.5)*(y-3.5))))";

TString fiducialCut(double rMax = 3.5, double holeClear = 1.5) {
    return Form("%s>%g && sqrt(x*x+y*y)<%g", kHoleDist, holeClear, rMax);
}

// dir = folder holding E<N>GeV.root (default "build"). Pass another folder to
// analyze a second run without clobbering the first, e.g.
//   root -l -b -q 'analysis/scan.C("build/short")'
// Plots are written to <dir>/plots/ so each run keeps its own.
// pbFrac: CONTAINMENT VETO — drop events leaking more than this fraction of
// the beam energy into the Pb-glass tail catcher. Full containment does not
// exist here: measured 2026-07-29, at >=50 GeV 100% of events leak >1 GeV
// (median at 120 GeV = 6.2 GeV = 5.2% of beam), so a zero-leakage cut keeps
// nothing. 0.05 keeps the least-leaky ~half at every energy. Note the veto
// preferentially keeps showers that START EARLY (less punch-through), which
// slightly reshapes the shower-max sampling — keep% is printed so this bias
// is never invisible. Set pbFrac=0 to disable.
void scan(const char* dir = "build", double rMax = 3.5, double pbFrac = 0.05) {
    // must match the energies (and order) in the macro that produced the files
    const int N = 6;
    const double E[N] = {5, 10, 25, 50, 100, 120};

    gStyle->SetOptStat(0);
    gSystem->mkdir(Form("%s/plots/fits", dir), true);

    const TString FIDbase = (rMax > 0) ? fiducialCut(rMax) : TString("1");
    printf("fiducial: %s\n", (rMax > 0) ? FIDbase.Data() : "NONE (raw sample)");
    printf("containment veto: %s\n\n",
           pbFrac > 0 ? Form("ePbGlass < %.0f%% of E_beam", 100*pbFrac) : "NONE");

    double sigT[N], sigTerr[N];      // timing:  sigma_t (ps)
    double resE[N], resEerr[N];      // energy, TRUTH:    sigma(Elyso)/Elyso (%)
    double resN[N], resNerr[N];      // energy, MEASURED: sigma(Npe)/Npe   (%)

    const int NL = 29;               // LYSO layers
    double prof[N][NL] = {};         // <Elayer> per layer -> shower profile
    // Npe vs deposited energy over ALL 29 LYSO layers. "Elyso" is already that
    // full sum, so this asks the question you actually want: how well does the
    // detected light let you recover the TOTAL sampled energy?
    //
    // Expect this to be LOOSER than a window-only correlation: the light is
    // generated at the 15 mm WLS window (layers 8-10), so Npe measures the
    // shower AT SHOWER MAX, and the window/total ratio drifts with energy as
    // shower max walks deeper. Whether the six energies land on one line is
    // exactly the "can I read E from Npe" test — the per-energy offsets ARE
    // the answer.
    const char* EDEP = "Elyso";
    double meanWin[N] = {}, meanNpe[N] = {};
    TProfile* pwin[N] = {};

    printf("%-7s %14s %6s %14s %14s %7s\n",
           "E(GeV)", "sigma_t (ps)", "eff%", "Npe res (%)", "Elyso res (%)", "keep%");
    for (int i = 0; i < N; ++i) {
        TString fname = Form("%s/E%.0fGeV.root", dir, E[i]);
        TFile f(fname);
        if (f.IsZombie()) { printf("  missing %s\n", fname.Data());
                            sigT[i]=resE[i]=resN[i]=0; sigTerr[i]=resEerr[i]=resNerr[i]=0; continue; }
        double mu, sg, sgErr;

        // full selection for THIS energy: fiducial + containment veto
        const TString FID = (pbFrac > 0)
            ? FIDbase + Form(" && ePbGlass < %g", pbFrac * E[i])
            : FIDbase;

        // --- timing: dT distribution -> sigma_t = sigma(dT)/2 ---
        // Prefer the WLS-only timing histogram (2026-07-29 fix). The all-light
        // "dT" is MULTI-MODAL at thinned light: ~1% of detected photons are
        // prompt Cherenkov arriving ~3.7 ns before the WLS bulk, and whether a
        // corner catches one is a Poisson coin flip, so the 4-corner mean forms
        // a comb of ~5 spikes. A Gaussian core fit across that comb returns
        // confident nonsense. Older files have only "dT" — fall back, but the
        // resulting sigma_t is not trustworthy.
        // The STORED dT/dTwls histograms have no fiducial cut, so rebuild from
        // the ntuple with FID applied — otherwise the beam-acceptance
        // pathologies documented above leak straight back into sigma_t.
        TTree* t = (TTree*)f.Get("ev");
        const bool wlsTiming = (t->GetBranch("dTwls") != nullptr);
        if (!wlsTiming)
            printf("  [!] %s: no dTwls (pre-2026-07-29 file) -- sigma_t from"
                   " all-light dT is NOT reliable\n", fname.Data());
        const char* tvar = wlsTiming ? "dTwls" : "dT";
        const TString tcut = FID + Form(" && %s>-999", tvar);

        TH1D* d = new TH1D("dTfine", "", 400, -3, 3);
        t->Draw(Form("%s>>dTfine", tvar), tcut, "goff");
        double mD = d->GetMean(), rD = d->GetRMS();
        if (rD > 0) { d->SetBins(300, mD - 5*rD, mD + 5*rD);
                      t->Draw(Form("%s>>dTfine", tvar), tcut, "goff"); }
        d->SetDirectory(nullptr);
        coreFitAndSave(d, Form("#DeltaT%s at %.0f GeV;#DeltaT = t_{down}-t_{up} (ns);events",
                               wlsTiming ? " (WLS only, fiducial)" : " (ALL light - unfittable)", E[i]),
                       Form("%s/plots/fits/dT_E%.0fGeV.png", dir, E[i]), mu, sg, sgErr);
        sigT[i]    = 1000 * sg    / 2;                       // ns->ps, /2 corner-trick
        sigTerr[i] = 1000 * sgErr / 2;

        // --- energy: Elyso spread -> sigma/mean at this fixed beam energy ---
        // The stored Elyso H1 has a FIXED 100 MeV bin width (250 bins over
        // 0-25 GeV) so it can hold any energy — but at 5 GeV the whole peak
        // (sigma ~ 40 MeV) lands in 2-3 bins and the Gaussian fit is quantized
        // garbage. Rebuild a per-energy histogram from the UNBINNED per-event
        // values in the ev ntuple instead: range = mean +- 5*RMS, 100 bins,
        // so every energy gets a well-resolved peak.
        double m0  = t->GetMinimum("Elyso"), m1 = t->GetMaximum("Elyso");
        TH1D* e = new TH1D("ElysoFine", "", 100, m0, m1);
        t->Draw("Elyso>>ElysoFine", FID, "goff");
        double mE = e->GetMean(), rE = e->GetRMS();
        e->SetBins(100, mE - 5*rE, mE + 5*rE);
        t->Draw("Elyso>>ElysoFine", FID, "goff");            // refill, focused range
        e->SetDirectory(nullptr);                            // detach from the file
        double muE, sgE, sgEErr;
        coreFitAndSave(e, Form("E_{LYSO} at %.0f GeV;E_{LYSO} (GeV);events", E[i]),
                       Form("%s/plots/fits/Elyso_E%.0fGeV.png", dir, E[i]), muE, sgE, sgEErr);
        delete e;
        resE[i]    = 100 * sgE / muE;
        resEerr[i] = resE[i] * sqrt(pow(sgEErr/sgE, 2));     // sigma error dominates

        // --- energy, the MEASURED observable: spread of DETECTED LIGHT ---
        // This is the papers' energy measurement: they sum the 8 low-gain SiPM
        // amplitudes, i.e. the WLS light from the ~3 tiles at shower max, NOT
        // the full-module dE/dx above. 2401.01747 sec 5.1.3 is explicit that
        // their number "does not represent the energy resolution that would
        // result from ... all 29 LYSO:Ce layers". Npe is the direct analog.
        // Same ntuple rebuild as Elyso, for the same binning reason.
        TH1D* nh = new TH1D("NpeFine", "", 100, t->GetMinimum("Npe"), t->GetMaximum("Npe"));
        t->Draw("Npe>>NpeFine", FID, "goff");
        double mN = nh->GetMean(), rN = nh->GetRMS();
        nh->SetBins(100, mN - 5*rN, mN + 5*rN);
        t->Draw("Npe>>NpeFine", FID, "goff");
        nh->SetDirectory(nullptr);
        double muN, sgN, sgNErr;
        coreFitAndSave(nh, Form("N_{pe} at %.0f GeV;detected photons;events", E[i]),
                       Form("%s/plots/fits/Npe_E%.0fGeV.png", dir, E[i]), muN, sgN, sgNErr);
        delete nh;
        resN[i]    = 100 * sgN / muN;
        resNerr[i] = resN[i] * (sgNErr / sgN);

        // --- shower profile + Npe<->window-eDep correlation (both fiducial) ---
        for (int L = 0; L < NL; ++L) {
            // fresh histogram each time: reusing ">>hL" keeps the FIRST call's
            // auto-binning, and later layers with bigger deposits overflow it,
            // silently computing the mean from whatever fraction stayed in range
            gDirectory->Delete("hL");
            t->Draw(Form("Elayer[%d]>>hL", L), FID, "goff");
            prof[i][L] = ((TH1*)gDirectory->Get("hL"))->GetMean();
        }
        gDirectory->Delete("hW"); gDirectory->Delete("hNm");
        t->Draw(Form("%s>>hW", EDEP), FID, "goff");
        meanWin[i] = ((TH1*)gDirectory->Get("hW"))->GetMean();
        t->Draw("Npe>>hNm", FID, "goff");
        meanNpe[i] = ((TH1*)gDirectory->Get("hNm"))->GetMean();
        pwin[i] = new TProfile(Form("pw%d", i), "", 40, 0., meanWin[i]*2.0);
        t->Draw(Form("Npe:%s>>pw%d", EDEP, i), FID, "prof goff");
        pwin[i]->SetDirectory(nullptr);

        // --- timing EFFICIENCY, measured WITHIN the fiducial ---
        // EventAction only fills dT when some corner saw light at BOTH ends. Dim
        // events silently vanish, and the survivors are the BRIGHTER ones — so a
        // sigma_t quoted at low efficiency is biased optimistic. Inside the
        // fiducial this should read 100%: the events that used to fail were the
        // ones whose beam went down a hole or missed the tile.
        const double inFid = t->Draw("Npe", FID, "goff");
        const double eff   = (inFid > 0) ? 100.0 * d->GetEntries() / inFid : 0.;
        const double keep  = 100.0 * inFid / t->GetEntries();
        delete d;

        printf("%-7.0f %7.1f +- %-4.1f %5.1f%% %7.2f +- %-4.2f %7.2f +- %-4.2f  %5.1f%%%s\n",
               E[i], sigT[i], sigTerr[i], eff, resN[i], resNerr[i], resE[i], resEerr[i],
               keep, (eff < 99.0 ? "   <-- BIASED" : ""));
        f.Close();
    }

    // --- graph 1: sigma_t vs E,  fit a/sqrt(E) (+) b ---
    {
        auto gr = new TGraphErrors(N, E, sigT, nullptr, sigTerr);
        gr->SetTitle("SIMPLE timing resolution;E_{beam} (GeV);#sigma_{t} (ps)");
        gr->SetMarkerStyle(20);
        auto fit = new TF1("ft", "sqrt([0]*[0]/x + [1]*[1])", E[0], E[N-1]);
        fit->SetParName(0, "a");                             // short names — long
        fit->SetParName(1, "b");                             // ones crash PaintStats
        fit->SetParameters(300, 30);
        gr->Fit(fit, "Q");
        gStyle->SetOptFit(1);
        auto c = new TCanvas("ct", "", 800, 600);
        gr->Draw("AP");
        drawFormula("#sigma_{t} = a / #sqrt{E} #oplus b   [a: ps#sqrt{GeV}, b: ps]");
        c->SaveAs(Form("%s/plots/sigma_t_vs_E.png", dir));
        printf("\ntiming: sigma_t = %.1f/sqrt(E) (+) %.1f ps   (+- %.1f / %.1f)\n",
               fit->GetParameter(0), fabs(fit->GetParameter(1)),
               fit->GetParError(0), fit->GetParError(1));
        delete c;
    }

    // --- graph 2: MEASURED energy resolution (from detected light) ---
    // IMPORTANT: the resolution does NOT improve monotonically here — it bottoms
    // out around 50 GeV and then gets WORSE (see the printed table). The papers'
    // a/sqrt(E) (+) b/E (+) c form is monotonically decreasing and therefore
    // CANNOT describe that, so fitting it over the full range returns a
    // meaningless compromise. Fit only the monotonic region and report the
    // turn-up separately, because the turn-up is a real physics result:
    // the DSB1 window is a FIXED 15 mm at 40.4 mm, while shower max walks
    // deeper with energy (2401.01747 Fig. 7: layers 8-10 at 25 GeV -> 11-13 at
    // 125 GeV), so the sampled fraction degrades at high E. The paper flags
    // exactly this ("adequate although not optimized ... corrected in future work").
    // Two-term (no b/E): SIMPLE has no electronic noise for a b/E term to model.
    const double EFITMAX = 50.;
    {
        auto gr = new TGraphErrors(N, E, resN, nullptr, resNerr);
        gr->SetTitle("SIMPLE energy resolution (detected light);E_{beam} (GeV);#sigma_{E}/E (%)");
        gr->SetMarkerStyle(20); gr->SetMarkerColor(kAzure+2); gr->SetLineColor(kAzure+2);
        auto fit = new TF1("fn", "sqrt([0]*[0]/x + [1]*[1])", E[0], EFITMAX);
        fit->SetParName(0, "a"); fit->SetParName(1, "c");
        fit->SetParameters(40, 9);
        gr->Fit(fit, "Q", "", E[0], EFITMAX);       // monotonic region only
        gStyle->SetOptFit(1);
        auto c = new TCanvas("cn", "", 800, 600);
        gr->Draw("AP");
        drawFormula("#sigma_{E}/E = a/#sqrt{E} #oplus c   (fit #leq 50 GeV)   [%]");
        c->SaveAs(Form("%s/plots/sigma_E_Npe_vs_E.png", dir));
        printf("energy (MEASURED, detected light, fit <=%.0f GeV):"
               " %.1f%%/sqrt(E) (+) %.2f%%\n"
               "        paper 2401.01747: 52.04%%/sqrt(E) (+) 31.62%%/E (+) 9.31%%"
               "  (their b/E is electronic noise, absent here)\n",
               EFITMAX, fit->GetParameter(0), fabs(fit->GetParameter(1)));
        // quantify the high-E degradation that the fit deliberately excludes
        double best = 1e9; int ibest = 0;
        for (int i = 0; i < N; ++i) if (resN[i] > 0 && resN[i] < best) { best = resN[i]; ibest = i; }
        if (ibest < N-1)
            printf("        TURN-UP: best %.2f%% at %.0f GeV, degrades to %.2f%% at %.0f GeV"
                   " (+%.0f%% rel) -- fixed 15 mm WLS window vs a deepening shower\n",
                   best, E[ibest], resN[N-1], E[N-1], 100*(resN[N-1]-best)/best);
        delete c;
    }

    // --- graph 3: TRUTH energy resolution (full-module dE/dx) ---
    // NOT the papers' observable — this is the intrinsic sampling resolution of
    // the whole 29-plate stack, a truth quantity nobody measured. Kept because
    // it is a clean characterization of the stack itself; do not compare it to
    // either paper's energy number (or to the 3x3-array design goal).
    {
        auto gr = new TGraphErrors(N, E, resE, nullptr, resEerr);
        gr->SetTitle("SIMPLE intrinsic sampling resolution (truth dE/dx);E_{beam} (GeV);#sigma_{E}/E (%)");
        gr->SetMarkerStyle(21);
        auto fit = new TF1("fe", "sqrt([0]*[0]/x + [1]*[1])", E[0], E[N-1]);
        fit->SetParName(0, "a");
        fit->SetParName(1, "b");
        fit->SetParameters(15, 2);
        gr->Fit(fit, "Q");
        gStyle->SetOptFit(1);
        auto c = new TCanvas("ce", "", 800, 600);
        gr->Draw("AP");
        drawFormula("#sigma_{E}/E = a / #sqrt{E} #oplus b   [a: %#sqrt{GeV}, b: %]");
        c->SaveAs(Form("%s/plots/sigma_E_vs_E.png", dir));
        printf("energy (TRUTH, full-stack dE/dx -- not a paper observable):"
               " %.1f%%/sqrt(E) (+) %.2f%%\n",
               fit->GetParameter(0), fabs(fit->GetParameter(1)));
        delete c;
    }

    // --- graph 4: the two side by side — SHOWER-MAX SLICE vs WHOLE MODULE ---
    // This is the plot that answers "what does the shower-max readout cost me
    // relative to reading out the entire module?".
    //   blue  = what THIS detector measures: light from the 15 mm DSB1 window
    //           at shower max (the papers' observable, and the one that turns up)
    //   red   = the whole 29-plate stack's sampled energy (dE/dx truth), which
    //           falls monotonically like a normal calorimeter and never turns up
    // CAVEAT: the red curve is TRUTH, not a light readout — it is the ceiling a
    // full-module readout could approach, not a simulated measurement. Building
    // an actual full-module light readout means E-type capillaries (WLS running
    // the FULL module length instead of 15 mm at shower max), which is a
    // geometry change + rerun, not an analysis change.
    // (No combined/overlay plot is produced on purpose — each observable gets
    //  its own standalone figure so nothing is visually conflated. The relative
    //  size of the two is stated numerically below instead.)
    printf("\nfor reference (NOT plotted together): at %.0f GeV the shower-max\n"
           "  light readout is %.1fx the whole-module dE/dx floor (%.2f%% vs %.2f%%)\n",
           E[N-1], resN[N-1]/resE[N-1], resN[N-1], resE[N-1]);

    // --- graph 5: longitudinal shower profile, all energies overlaid ---
    // <E deposited> per LYSO layer. The DSB1 window (layers 8-10) is marked:
    // this is what the timing fibre samples, and why the sampled fraction
    // degrades at high E as shower max walks deeper (2401.01747 Fig. 7).
    {
        auto c = new TCanvas("cp", "", 850, 620);
        c->SetLogy();
        auto mg = new TMultiGraph();
        auto lg = new TLegend(0.75, 0.60, 0.89, 0.89);
        lg->SetBorderSize(0); lg->SetFillStyle(0);
        const int COL[6] = {kBlack, kAzure+2, kGreen+2, kRed+1, kMagenta+1, kOrange+7};
        for (int i = 0; i < N; ++i) {
            double L[NL];
            for (int j = 0; j < NL; ++j) L[j] = j;
            auto g = new TGraph(NL, L, prof[i]);
            g->SetLineColor(COL[i]); g->SetMarkerColor(COL[i]);
            g->SetMarkerStyle(20); g->SetMarkerSize(0.6);
            mg->Add(g, "LP");
            lg->AddEntry(g, Form("%.0f GeV", E[i]), "lp");
        }
        mg->SetTitle("longitudinal shower profile (fiducial);LYSO layer;#LTE_{layer}#GT (GeV)");
        mg->Draw("A");
        // shade the DSB1 window, layers 8-10
        auto box = new TBox(7.5, mg->GetYaxis()->GetXmin(), 10.5, mg->GetYaxis()->GetXmax());
        box->SetFillColorAlpha(kOrange, 0.15); box->Draw();
        lg->Draw();
        c->SaveAs(Form("%s/plots/shower_profile.png", dir));
        delete c;
    }

    // --- graph 6: Npe vs eDep in the WLS window — the "calorimeter's view" ---
    // If the detected light is a faithful sample of the window deposit, all
    // energies fall on ONE line; its slope is the light yield (pe/GeV) and is
    // the conversion from measured photons back to deposited energy.
    {
        auto c = new TCanvas("cw", "", 850, 620);
        auto lg = new TLegend(0.15, 0.60, 0.34, 0.89);
        lg->SetBorderSize(0); lg->SetFillStyle(0);
        const int COL[6] = {kBlack, kAzure+2, kGreen+2, kRed+1, kMagenta+1, kOrange+7};
        double maxX = 0, maxY = 0;
        for (int i = 0; i < N; ++i) if (pwin[i]) {
            maxX = TMath::Max(maxX, meanWin[i]*2.0);
            maxY = TMath::Max(maxY, meanNpe[i]*2.0);
        }
        auto frame = gPad->DrawFrame(0, 0, maxX, maxY,
            "detected light vs total LYSO deposit (all 29 layers);"
            "E_{LYSO}, all layers (GeV);N_{pe}");
        for (int i = 0; i < N; ++i) if (pwin[i]) {
            pwin[i]->SetLineColor(COL[i]); pwin[i]->SetMarkerColor(COL[i]);
            pwin[i]->SetMarkerStyle(20);   pwin[i]->SetMarkerSize(0.5);
            pwin[i]->Draw("same");
            lg->AddEntry(pwin[i], Form("%.0f GeV", E[i]), "lp");
        }
        // one global line through the 6 per-energy means -> the pe/GeV handle
        auto gm = new TGraph(N, meanWin, meanNpe);
        auto lin = new TF1("lin", "[0]*x", 0, maxX);
        gm->Fit(lin, "Q");
        lin->SetLineColor(kGray+2); lin->SetLineStyle(2); lin->Draw("same");
        lg->AddEntry(lin, Form("%.0f pe/GeV", lin->GetParameter(0)), "l");
        lg->Draw();
        c->SaveAs(Form("%s/plots/npe_vs_Elyso.png", dir));
        printf("\nlight yield handle: Npe = %.1f pe per GeV of TOTAL LYSO deposit"
               " (all 29 layers)\n  (at LIGHT_SCALE=1e-2; multiply by 100 for true light."
               "  Invert to read E from Npe.)\n", lin->GetParameter(0));
        // Per-energy slope: if these drift, one global pe/GeV does NOT recover
        // the energy and a per-energy (i.e. depth-dependent) calibration is
        // needed — the light samples shower max, not the whole stack.
        printf("  per-energy Npe/E_LYSO:");
        for (int i = 0; i < N; ++i)
            if (meanWin[i] > 0) printf("  %.0fGeV:%.0f", E[i], meanNpe[i]/meanWin[i]);
        printf("\n");
        delete c;
    }

    printf("\nplots: %s/plots/  (sigma_t_vs_E.png, sigma_E_Npe_vs_E.png,"
           " sigma_E_vs_E.png,\n        shower_profile.png, npe_vs_Elyso.png, fits/*.png)\n"
           "(sigma_t is thinned by RADSIMPLE_LYSO_SCALE; sigma_E/E is not.)\n", dir);
}
