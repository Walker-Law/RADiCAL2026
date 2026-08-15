// lightscan.C — separate photon-counting from the light-independent floor.
// Usage (from RADiCALsimLightScan/):  root -l -b -q analysis/lightscan.C
//
// THE FIT. Timing resolution at light scale f decomposes as
//
//     sigma_t^2(f) = A^2 / f + B^2
//
// which is LINEAR in x = 1/f: slope A^2, intercept B^2. So plotting
// sigma_t^2 against 1/f and fitting a straight line reads off both terms
// directly. A is photon counting (vanishes as light -> infinity); B is the
// floor that survives no matter how much light there is — fibre transit-time
// spread, path-length dispersion, geometry. B is the number that decides
// whether <10 ps is reachable in principle.
//
// WHY THIS EXISTS: extrapolating a single thinned run by sqrt(f) implicitly
// assumes B = 0. The DSB sim's ladder found B = 43.5 ps hiding under exactly
// that assumption (../RADiCALsimLadder/README.md). One point cannot
// distinguish "the floor is zero" from "the floor is small at this f".
//
// CAVEAT worth remembering when reading the result: B here is the floor of
// the SIMPLE model, which has NO electronics. A real device adds SiPM SPTR,
// amplifier noise-over-slope and DRS4 timebase on top of B. So B is a LOWER
// BOUND on the achievable device resolution, not a prediction of it.

// Two-pass Gaussian core fit (same estimator as RADiCALsimSIMPLE/scan.C, so
// the f=1e-2 rung here is directly comparable to the production numbers).
void coreFit(TH1* h, double& mu, double& sg, double& sgErr) {
    mu = sg = sgErr = 0;
    if (!h || h->GetEntries() < 50) return;
    double m = h->GetMean(), r = h->GetRMS();
    if (r <= 0) return;
    h->Fit("gaus", "Q0", "", m - 3*r, m + 3*r);
    TF1* g = h->GetFunction("gaus");
    if (!g) return;
    mu = g->GetParameter(1); sg = g->GetParameter(2);
    if (sg <= 0) return;
    h->Fit("gaus", "Q0", "", mu - 2*sg, mu + 2*sg);
    g = h->GetFunction("gaus");
    if (!g) return;
    mu = g->GetParameter(1); sg = g->GetParameter(2); sgErr = g->GetParError(2);
}

// Same beam fiducial as RADiCALsimSIMPLE/scan.C — required, not cosmetic:
// without it, events whose beam went down a capillary hole or missed the
// 14 mm tile contaminate every width. See scan.C for the full justification.
const char* kHoleDist =
    "min(min(sqrt(x*x+y*y),"
    "min(sqrt((x-3.5)*(x-3.5)+(y-3.5)*(y-3.5)),sqrt((x+3.5)*(x+3.5)+(y+3.5)*(y+3.5)))),"
    "min(sqrt((x-3.5)*(x-3.5)+(y+3.5)*(y+3.5)),sqrt((x+3.5)*(x+3.5)+(y-3.5)*(y-3.5))))";

void lightscan(const char* dir = "build/rootfiles", double rMax = 3.5) {
    gStyle->SetOptStat(0);
    TString PLOTS = dir; PLOTS.ReplaceAll("rootfiles", "plots");
    if (PLOTS == dir) PLOTS += "/plots";
    gSystem->mkdir(PLOTS + "/fits", true);

    const TString FID = Form("%s>1.5 && sqrt(x*x+y*y)<%g", kHoleDist, rMax);

    // ---- discover the rungs actually present -----------------------------
    std::vector<double> F;
    std::vector<std::string> Fdir;
    void* dp = gSystem->OpenDirectory(dir);
    if (!dp) { printf("no such directory: %s (run run_lightscan.sh first)\n", dir); return; }
    const char* ent;
    while ((ent = gSystem->GetDirEntry(dp))) {
        std::string n = ent;
        if (n.size() < 2 || n[0] != 'f') continue;
        F.push_back(atof(n.c_str() + 1));
        Fdir.push_back(n);
    }
    gSystem->FreeDirectory(dp);
    if (F.empty()) { printf("no f<scale>/ rung folders under %s/\n", dir); return; }
    // sort rungs by f
    for (size_t i = 0; i < F.size(); ++i)
        for (size_t j = i+1; j < F.size(); ++j)
            if (F[j] < F[i]) { std::swap(F[i],F[j]); std::swap(Fdir[i],Fdir[j]); }

    // ---- discover energies present in the first rung ----------------------
    const double ECAND[] = {5, 10, 25, 50, 100, 120};
    std::vector<double> E;
    for (double e : ECAND)
        if (!gSystem->AccessPathName(Form("%s/%s/E%.0fGeV.root", dir, Fdir[0].c_str(), e)))
            E.push_back(e);
    if (E.empty()) { printf("no E<N>GeV.root in %s/%s/\n", dir, Fdir[0].c_str()); return; }

    const int NF = F.size(), NE = E.size();
    printf("rungs:");    for (int i=0;i<NF;++i) printf(" %g", F[i]);
    printf("\nenergies:"); for (int i=0;i<NE;++i) printf(" %.0f", E[i]);
    printf(" GeV\nfiducial: r<%g mm, >1.5 mm from any hole\n\n", rMax);

    // [energy][rung]
    std::vector<std::vector<double>> sigT   (NE, std::vector<double>(NF, 0)),
                                     sigTerr(NE, std::vector<double>(NF, 0)),
                                     npe    (NE, std::vector<double>(NF, 0));
    // per-energy true-light extrapolation, filled by the fit below
    std::vector<double> trueLight(NE, 0), trueLightErr(NE, 0);

    for (int ie = 0; ie < NE; ++ie) {
        printf("=== %.0f GeV ===\n", E[ie]);
        printf("%-10s %10s %14s %12s\n", "f", "<Npe>", "sigma_t (ps)", "eff%");
        for (int jf = 0; jf < NF; ++jf) {
            TString fn = Form("%s/%s/E%.0fGeV.root", dir, Fdir[jf].c_str(), E[ie]);
            if (gSystem->AccessPathName(fn)) { printf("%-10g   (missing)\n", F[jf]); continue; }
            TFile f(fn);
            if (f.IsZombie()) { printf("%-10g   (unreadable)\n", F[jf]); continue; }
            TTree* t = (TTree*)f.Get("ev");
            if (!t) { printf("%-10g   (no ntuple)\n", F[jf]); continue; }

            // WLS-only timing — the all-light dT is multi-modal at low light
            // (a ~1% prompt-Cherenkov population wins the first-photon race at
            // random). See RADiCALsimSIMPLE/include/EventAction.hh.
            const char* tvar = t->GetBranch("dTcfd") ? "dTcfd"
                             : t->GetBranch("dTwls") ? "dTwls" : "dT";
            TString tcut = FID + Form(" && %s>-999", tvar);
            TH1D* h = new TH1D("dTf", "", 400, -3, 3);
            t->Draw(Form("%s>>dTf", tvar), tcut, "goff");
            double m = h->GetMean(), r = h->GetRMS();
            if (r > 0) {
                // Bin count scales with statistics (~8 events/bin), bounded
                // [20,300]. A FIXED 300 bins here once produced a nonsense fit
                // (22.5+-42.1 ps) on an 82-event emergency rung: nearly every
                // bin was empty/single-occupancy, and the two-pass Gaussian
                // core fit diverged on that sparse, spiky histogram instead of
                // failing loudly. This is a companion to the chi2/ndf guard
                // below -- that guard catches a bad MODEL, this prevents a bad
                // HISTOGRAM from masquerading as a precise measurement.
                int nb = (int)std::max(20.0, std::min(300.0, h->GetEntries()/8.0));
                h->SetBins(nb, m-5*r, m+5*r);
                t->Draw(Form("%s>>dTf", tvar), tcut, "goff");
            }
            h->SetDirectory(nullptr);
            double mu, sg, sgErr;
            coreFit(h, mu, sg, sgErr);
            sigT[ie][jf] = 1000 * sg / 2;      // ns->ps, /2 for the corner trick
            sigTerr[ie][jf] = 1000 * sgErr / 2;
            delete h;

            t->Draw("Npe>>hn(200,0,0)", FID, "goff");
            npe[ie][jf] = ((TH1*)gDirectory->Get("hn"))->GetMean();
            double inFid = t->Draw("Npe", FID, "goff");
            double nT    = t->Draw(tvar, tcut, "goff");
            printf("%-10g %10.0f %8.1f +- %-4.1f %10.1f%%\n",
                   F[jf], npe[ie][jf], sigT[ie][jf], sigTerr[ie][jf],
                   inFid > 0 ? 100.0*nT/inFid : 0.);
            f.Close();
        }
        printf("\n");
    }

    // ---- the fit: sigma_t^2 vs 1/f, per energy ---------------------------
    // f=1 is TRUE LIGHT: RADSIMPLE_LIGHT_SCALE multiplies LYSO's datasheet
    // 33200 ph/MeV, and StackingAction thins Cherenkov by the same factor, so
    // f=1 means "no thinning anywhere". sigma_t(f=1) = sqrt(A^2 + B^2) is
    // therefore the extrapolation to the light the real device actually has —
    // reported WITH its error, because that error is what decides whether the
    // <10 ps goal is met, missed, or simply unresolved.
    //
    // THIS MODEL ASSUMES MEAN-AVERAGING PHOTOSTATISTICS (sigma ~ 1/sqrt(N)).
    // dTwls is a FIRST-PHOTON (minimum-of-N) estimator, which is an ORDER
    // STATISTIC, not a mean -- and minima are not obliged to obey 1/sqrt(N).
    // A chi2/ndf cutoff below therefore isn't just fit hygiene here; it is
    // the test of whether that assumption even applies to this observable.
    // If it fails, log-log slope + a bare data table are printed instead of
    // a fabricated true-light number (see CHI2_BAD block below).
    const double CHI2_BAD = 5.0;
    printf("=== A/B separation:  sigma_t^2 = A^2/f + B^2 ===\n");
    printf("%-8s %14s %14s %10s %22s\n",
           "E(GeV)", "A (ps.sqrt f)", "B (ps)", "chi2/ndf",
           "TRUE LIGHT sigma_t (ps)");
    auto c = new TCanvas("cl", "", 850, 620);
    // log-log: 1/f spans ~3 decades and sigma_t^2 spans ~3 more — on linear
    // axes all but the lowest-f rung collapse into the origin corner and the
    // plot looks like "2 points" (2026-08-08).
    c->SetLogx(); c->SetLogy();
    auto mg = new TMultiGraph();
    auto lg = new TLegend(0.15, 0.68, 0.45, 0.88);
    lg->SetBorderSize(0); lg->SetFillStyle(0);
    const int COL[] = {kAzure+2, kRed+1, kGreen+2, kMagenta+1, kOrange+7, kBlack};
    std::vector<bool> fitOk(NE, false);

    for (int ie = 0; ie < NE; ++ie) {
        std::vector<double> x, y, ey;
        for (int jf = 0; jf < NF; ++jf) {
            if (!(sigT[ie][jf] > 0)) continue;
            x.push_back(1.0 / F[jf]);
            y.push_back(sigT[ie][jf] * sigT[ie][jf]);
            ey.push_back(2 * sigT[ie][jf] * sigTerr[ie][jf]);   // d(s^2) = 2 s ds
        }
        if (x.size() < 2) { printf("%-8.0f  (need >=2 rungs)\n", E[ie]); continue; }

        // Power-law characterization: sigma_t ~ f^(-p), via log-log least
        // squares over ALL rungs at this energy. Naive photon-COUNTING
        // (mean-averaging) predicts p=0.5; dTwls is a FIRST-PHOTON (minimum
        // of N) estimator, an ORDER statistic, which is not obliged to obey
        // that -- p can genuinely exceed 0.5. This is reported UNCONDITIONALLY
        // (unlike the A/B fit above) because it doesn't assume a floor exists;
        // it just describes how sigma_t actually scales with the data at hand.
        {
            std::vector<double> lx, ly;
            for (int jf = 0; jf < NF; ++jf)
                if (sigT[ie][jf] > 0) { lx.push_back(log(F[jf])); ly.push_back(log(sigT[ie][jf])); }
            double mx=0,my=0; for (size_t i=0;i<lx.size();++i){mx+=lx[i];my+=ly[i];}
            mx/=lx.size(); my/=ly.size();
            double num=0, den=0;
            for (size_t i=0;i<lx.size();++i){ num+=(lx[i]-mx)*(ly[i]-my); den+=(lx[i]-mx)*(lx[i]-mx); }
            double p = -num/den;
            printf("         power-law fit: sigma_t ~ f^-%.2f  (0.5 = naive photon-counting;"
                   " first-photon/minimum-of-N estimators can exceed 0.5)\n", p);
        }

        auto g = new TGraphErrors(x.size(), &x[0], &y[0], nullptr, &ey[0]);
        auto lin = new TF1(Form("lin%d", ie), "[0]*x + [1]", 0, x.back()*1.1);
        lin->SetParameters(y[0]/std::max(1e-9,x[0]), 0.);
        g->Fit(lin, "Q");
        double A2 = lin->GetParameter(0),  B2 = lin->GetParameter(1);
        double eA2 = lin->GetParError(0), eB2 = lin->GetParError(1);
        double A  = A2 > 0 ? sqrt(A2) : 0;
        double B  = B2 > 0 ? sqrt(B2) : 0;
        double ndf = lin->GetNDF();
        double chi2ndf = ndf > 0 ? lin->GetChisquare()/ndf : 0.;
        fitOk[ie] = (chi2ndf < CHI2_BAD) && (B2 > 0 || fabs(B2) < 0.2*A2);

        if (!fitOk[ie]) {
            // Do NOT compute/print a true-light extrapolation from a fit this
            // bad -- sqrt(A^2+B^2) with a wildly negative B^2 is a coordinate
            // artifact (A^2+B^2 near zero), not a physical floor estimate.
            printf("%-8.0f %14s %14s %10.1f %22s\n", E[ie], "--", "--", chi2ndf,
                   "MODEL INVALID (see below)");
            trueLight[ie] = trueLightErr[ie] = 0;
        } else {
            double s1  = sqrt(std::max(1e-9, A2 + B2));
            double es1 = sqrt(eA2*eA2 + eB2*eB2) / (2*s1);
            trueLight[ie] = s1; trueLightErr[ie] = es1;
            printf("%-8.0f %14.1f %14.1f %10.2f %13.1f +- %-5.1f%s\n",
                   E[ie], A, B, chi2ndf, s1, es1,
                   B2 <= 0 ? "  (B^2<0: consistent with no floor)" : "");
        }
        g->SetMarkerStyle(20 + ie); g->SetMarkerColor(COL[ie % 6]);
        g->SetLineColor(COL[ie % 6]); lin->SetLineColor(COL[ie % 6]);
        mg->Add(g, "P");
        lg->AddEntry(g, Form("%.0f GeV", E[ie]), "lp");
    }

    mg->SetTitle("photostatistics ladder: #sigma_{t}^{2} = A^{2}/f + B^{2};"
                 "1/f  (1 = true light);#sigma_{t}^{2} (ps^{2})");
    mg->Draw("A");
    lg->Draw();
    c->SaveAs(Form("%s/ladder_sigmaT2_vs_invf.png", PLOTS.Data()));
    delete c;

    // ---- THE TURNOVER PLOT: sigma_t*sqrt(f) vs f ---------------------------
    // If sigma_t^2 = A^2/f + B^2 held with B^2>=0, sigma_t*sqrt(f) = sqrt(A^2
    // + B^2*f) could only stay flat or RISE as f grows -- it can never fall.
    // So this quantity falling is the direct, model-independent signature
    // that the mean-averaging assumption is wrong (see file header); and a
    // MINIMUM followed by a rise -- a true turnover -- is what finding the
    // real floor looks like, whatever its functional form turns out to be.
    // Printed AND plotted so the turnover is visible without doing the
    // arithmetic by hand.
    printf("\n=== turnover check: sigma_t*sqrt(f) (falling = no floor yet"
           " visible; rising = floor coming into view) ===\n");
    auto c2 = new TCanvas("c2", "", 850, 620);
    c2->SetLogx();
    auto mg2 = new TMultiGraph();
    auto lg2 = new TLegend(0.62, 0.68, 0.89, 0.89);
    lg2->SetBorderSize(0); lg2->SetFillStyle(0);
    const double REL_ERR_BAD = 0.3;   // >30% relative error on sigma_t: too
                                       // noisy to trust for a rise/fall call
                                       // (this is what a 200-event emergency
                                       // rung looked like before the binning
                                       // fix above -- guard stays regardless)
    for (int ie = 0; ie < NE; ++ie) {
        printf("  %.0f GeV:", E[ie]);
        std::vector<double> x, y, ey;
        // CONSECUTIVE rises, not total (bug fixed 2026-08-09): the old code
        // counted every rise anywhere in the sequence, so a zigzag like
        // dn,UP,dn,UP -- pure statistical scatter around a FLAT sigma_t*sqrt(f)
        // -- scored 2 "rises" and triggered the turnover verdict, which then
        // let the floor fit run and print B = 18.8 +- 30.7 ps (163% error) off
        // a 5-rung 25 GeV ladder that had not turned over at all. A real
        // turnover is a MINIMUM FOLLOWED BY A SUSTAINED RISE, so only an
        // unbroken run of rises counts. (D9's lesson, third recurrence.)
        double prev = -1; int nRises = 0, runRises = 0, nFalls = 0;
        for (int jf = 0; jf < NF; ++jf) {
            if (!(sigT[ie][jf] > 0)) continue;
            double v = sigT[ie][jf] * sqrt(F[jf]);
            bool unreliable = (sigTerr[ie][jf] / sigT[ie][jf]) > REL_ERR_BAD;
            const char* arrow = "";
            if (unreliable) {
                arrow = " [UNRELIABLE >30% rel.err -- excluded from verdict]";
            } else if (prev >= 0) {
                if (v > prev) { arrow = " UP"; ++runRises;
                                if (runRises > nRises) nRises = runRises; }
                else          { arrow = " dn"; ++nFalls; runRises = 0; }
            }
            printf("  f=%g:%.2f%s", F[jf], v, arrow);
            x.push_back(F[jf]); y.push_back(v);
            ey.push_back(sqrt(F[jf]) * sigTerr[ie][jf]);
            if (!unreliable) prev = v;   // don't chain off a garbage point
        }
        printf("  ->  %s\n", nRises >= 2 ? "TURNOVER SEEN (>=2 CONSECUTIVE rises)"
                            : nRises == 1 ? "possible turnover (1 rise) -- need one more rung to confirm"
                            : "still falling / flat -- no floor visible in this range yet");
        auto g2 = new TGraphErrors(x.size(), &x[0], &y[0], nullptr, &ey[0]);
        g2->SetMarkerStyle(20 + ie); g2->SetMarkerColor(COL[ie % 6]);
        g2->SetLineColor(COL[ie % 6]);
        mg2->Add(g2, "LP");
        lg2->AddEntry(g2, Form("%.0f GeV", E[ie]), "lp");

        // ---- THE FLOOR, measured -- only when the turnover is actually seen.
        // sigma_t(f) = sqrt(C*f^-q + B^2): the power term is the empirical
        // order-statistic scaling (q free -- NOT assumed 1, see file header),
        // and B is the light-independent floor. This is only fittable once
        // rungs on BOTH sides of the minimum exist; before that the fit is
        // degenerate (measured: chi2/ndf 8-13 with 5 rungs, 2.6 with 6). Both
        // guards from elsewhere in this file apply: >=2 confirmed rises AND
        // chi2/ndf < CHI2_BAD, else nothing is reported.
        if (nRises >= 2) {
            std::vector<double> fx, fy, fe;
            for (int jf = 0; jf < NF; ++jf) {
                if (!(sigT[ie][jf] > 0)) continue;
                if (sigTerr[ie][jf] / sigT[ie][jf] > REL_ERR_BAD) continue;
                fx.push_back(F[jf]); fy.push_back(sigT[ie][jf]); fe.push_back(sigTerr[ie][jf]);
            }
            if (fx.size() >= 5) {   // 3 free params; demand >=2 dof
                auto gf = new TGraphErrors(fx.size(), &fx[0], &fy[0], nullptr, &fe[0]);
                auto ffit = new TF1(Form("floor%d", ie),
                                    "sqrt([0]*pow(x,-[1])+[2]*[2])", fx.front(), fx.back());
                ffit->SetParameters(1.0, 0.6, 5.0);
                ffit->SetParLimits(2, 0.0, 100.0);
                gf->Fit(ffit, "Q");
                double B = ffit->GetParameter(2), eB = ffit->GetParError(2);
                double q = ffit->GetParameter(1), eq = ffit->GetParError(1);
                double c2n = ffit->GetNDF() > 0 ? ffit->GetChisquare()/ffit->GetNDF() : 999;
                if (c2n < CHI2_BAD) {
                    printf("      FLOOR (light-independent): B = %.2f +- %.2f ps"
                           "   [q = %.2f +- %.2f, chi2/ndf = %.2f]\n"
                           "      -> light-side true-light sigma_t at %.0f GeV = B"
                           " (power term ~0 at f=1); compare to the 10 ps goal,\n"
                           "         REMEMBERING electronics (SPTR, DRS4) add on top.\n",
                           B, eB, q, eq, c2n, E[ie]);
                } else {
                    printf("      floor fit attempted but chi2/ndf = %.1f fails the"
                           " %.0f cut -- not reported\n", c2n, CHI2_BAD);
                }
            }
        }
    }
    mg2->SetTitle("turnover check (rising = floor visible);f (log scale, 1 = true light);#sigma_{t}#upoint#sqrt{f}  (ps#sqrt{GeV#lower[-0.3]{ }}units)");
    mg2->Draw("A");
    lg2->Draw();
    c2->SaveAs(Form("%s/turnover_sigmaT_sqrtf_vs_f.png", PLOTS.Data()));
    delete c2;

    // ---- does the light itself scale as it should? ------------------------
    // Sanity check on the whole premise: Npe must be LINEAR in f. If it is
    // not, the sim is not simply "the same detector with less light" and the
    // A^2/f model does not apply, so the extrapolation would be invalid.
    // Also reports the true-light yield, which is the number to compare
    // against the papers / DSB's ~50-90 pe/MeV prediction.
    printf("\n=== light scaling check (Npe must be linear in f) ===\n");
    printf("%-8s %16s %14s %18s\n",
           "E(GeV)", "Npe/f (should", "max deviation", "TRUE-LIGHT Npe");
    printf("%-8s %16s %14s %18s\n", "", "be constant)", "from constant", "(f=1)");
    for (int ie = 0; ie < NE; ++ie) {
        double ref = 0; int nref = 0;
        for (int jf = 0; jf < NF; ++jf)
            if (npe[ie][jf] > 0) { ref += npe[ie][jf]/F[jf]; ++nref; }
        if (!nref) continue;
        ref /= nref;
        double worst = 0;
        for (int jf = 0; jf < NF; ++jf)
            if (npe[ie][jf] > 0)
                worst = std::max(worst, fabs(npe[ie][jf]/F[jf] - ref)/ref);
        printf("%-8.0f %16.0f %13.1f%% %18.0f%s\n",
               E[ie], ref, 100*worst, ref,
               worst > 0.10 ? "   <-- NONLINEAR, extrapolation suspect" : "");
    }

    bool anyBad = false;
    for (int ie = 0; ie < NE; ++ie) if (!fitOk[ie]) anyBad = true;

    printf("\n--- how to read this ---\n"
           "B          the LIGHT-INDEPENDENT floor: the part more light cannot fix.\n"
           "TRUE LIGHT sigma_t at f=1, i.e. LYSO's full 33200 ph/MeV with no\n"
           "           thinning anywhere. THIS is the number to compare to 10 ps.\n"
           "MODEL INVALID: chi2/ndf failed the %.0f cut, so no true-light number\n"
           "           is reported -- printing one anyway would be a fabrication.\n",
           CHI2_BAD);
    if (anyBad)
        printf("\n[!] THE TWO-TERM MODEL FAILED HERE. Diagnostic: sigma_t*sqrt(f) should\n"
               "be flat-or-RISING with f if sigma_t^2 = A^2/f + B^2 with B^2>=0 -- any\n"
               "floor only ever ADDS as f grows. If it instead FALLS as f increases\n"
               "(check the printed sigma_t values by hand: sigma_t*sqrt(f) at each f),\n"
               "the mean-averaging assumption behind A^2/f is simply wrong for this\n"
               "observable. dTwls is a FIRST-PHOTON (minimum-of-N) estimator -- an\n"
               "ORDER statistic, not a mean -- and minima are not obliged to obey\n"
               "1/sqrt(N). The power-law slope printed above each energy (p) is the\n"
               "actual local scaling; p > 0.5 here means the light-side timing is\n"
               "improving FASTER than naive counting predicts, which is encouraging\n"
               "for the <10 ps goal, but a power law has no floor built in, so it\n"
               "must NOT be extrapolated to f=1 either -- it would be optimistic in\n"
               "the same way the A^2/f+B^2 fit was invalid. NEXT STEP: add a rung\n"
               "between f=0.03 and f=0.3 -- if a real floor exists, sigma_t*sqrt(f)\n"
               "should visibly turn over and start RISING there (120 GeV already\n"
               "shows a hint of this between f=0.01 and f=0.03); that turnover, not\n"
               "an extrapolation past it, is what would pin down B honestly.\n");
    printf("\nCAVEAT that must travel with any number from here: SIMPLE models NO\n"
           "electronics, so this is the LIGHT-side budget only. A real device adds\n"
           "SiPM SPTR (~60 ps single-photon), amplifier noise-over-slope and DRS4\n"
           "timebase on top — which is why real test-beam data sits near 500 ps.\n"
           "Read the result as: 'can light transport alone support <10 ps?'\n");
    printf("\nplots: %s/\n", PLOTS.Data());
}
