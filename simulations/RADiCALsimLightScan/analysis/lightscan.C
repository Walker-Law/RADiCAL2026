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
            const char* tvar = t->GetBranch("dTwls") ? "dTwls" : "dT";
            TString tcut = FID + Form(" && %s>-999", tvar);
            TH1D* h = new TH1D("dTf", "", 400, -3, 3);
            t->Draw(Form("%s>>dTf", tvar), tcut, "goff");
            double m = h->GetMean(), r = h->GetRMS();
            if (r > 0) { h->SetBins(300, m-5*r, m+5*r);
                         t->Draw(Form("%s>>dTf", tvar), tcut, "goff"); }
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
    printf("=== A/B separation:  sigma_t^2 = A^2/f + B^2 ===\n");
    printf("%-8s %14s %14s %10s %22s\n",
           "E(GeV)", "A (ps.sqrt f)", "B (ps)", "chi2/ndf",
           "TRUE LIGHT sigma_t (ps)");
    auto c = new TCanvas("cl", "", 850, 620);
    auto mg = new TMultiGraph();
    auto lg = new TLegend(0.15, 0.68, 0.45, 0.88);
    lg->SetBorderSize(0); lg->SetFillStyle(0);
    const int COL[] = {kAzure+2, kRed+1, kGreen+2, kMagenta+1, kOrange+7, kBlack};

    for (int ie = 0; ie < NE; ++ie) {
        std::vector<double> x, y, ey;
        for (int jf = 0; jf < NF; ++jf) {
            if (!(sigT[ie][jf] > 0)) continue;
            x.push_back(1.0 / F[jf]);
            y.push_back(sigT[ie][jf] * sigT[ie][jf]);
            ey.push_back(2 * sigT[ie][jf] * sigTerr[ie][jf]);   // d(s^2) = 2 s ds
        }
        if (x.size() < 2) { printf("%-8.0f  (need >=2 rungs)\n", E[ie]); continue; }
        auto g = new TGraphErrors(x.size(), &x[0], &y[0], nullptr, &ey[0]);
        auto lin = new TF1(Form("lin%d", ie), "[0]*x + [1]", 0, x.back()*1.1);
        lin->SetParameters(y[0]/std::max(1e-9,x[0]), 0.);
        g->Fit(lin, "Q");
        double A2 = lin->GetParameter(0),  B2 = lin->GetParameter(1);
        double eA2 = lin->GetParError(0), eB2 = lin->GetParError(1);
        double A  = A2 > 0 ? sqrt(A2) : 0;
        // A negative intercept is consistent with B=0 within errors; report 0
        // rather than an imaginary floor, but say so.
        double B  = B2 > 0 ? sqrt(B2) : 0;
        double ndf = lin->GetNDF();
        // TRUE LIGHT (f=1): sigma_t = sqrt(A^2 + B^2), with both parameter
        // errors propagated. Reported as x +- e so the goal question is
        // answerable at a glance, not eyeballed off a central value.
        double s1  = sqrt(std::max(1e-9, A2 + B2));
        double es1 = sqrt(eA2*eA2 + eB2*eB2) / (2*s1);
        trueLight[ie] = s1; trueLightErr[ie] = es1;
        printf("%-8.0f %14.1f %14.1f %10.2f %13.1f +- %-5.1f%s\n",
               E[ie], A, B, ndf > 0 ? lin->GetChisquare()/ndf : 0., s1, es1,
               B2 <= 0 ? "  (B^2<0: consistent with no floor)" : "");
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

    printf("\n--- how to read this ---\n"
           "B          the LIGHT-INDEPENDENT floor: the part more light cannot fix.\n"
           "TRUE LIGHT sigma_t at f=1, i.e. LYSO's full 33200 ph/MeV with no\n"
           "           thinning anywhere. THIS is the number to compare to 10 ps,\n"
           "           and its error bar decides whether the comparison is\n"
           "           conclusive. chi2/ndf >> 1 means the A^2/f + B^2 model does\n"
           "           not describe the data and neither number should be trusted.\n"
           "\nCAVEAT that must travel with any number from here: SIMPLE models NO\n"
           "electronics, so this is the LIGHT-side budget only. A real device adds\n"
           "SiPM SPTR (~60 ps single-photon), amplifier noise-over-slope and DRS4\n"
           "timebase on top — which is why real test-beam data sits near 500 ps.\n"
           "Read the result as: 'can light transport alone support <10 ps?'\n");
    printf("\nplots: %s/\n", PLOTS.Data());
}
