// fscan25.C — timing and energy resolution vs light-thinning fraction f, at a
// single fixed energy (default 25 GeV).
//
// Usage (from RADiCALsimSIMPLE/):
//   root -l -b -q analysis/fscan25.C
//
// scan.C sweeps ENERGY at fixed f; lightscan.C sweeps f but only reports
// TIMING. This is the missing combination: fixed energy, f on the x-axis,
// both sigma_t and sigma_E/E plotted together. Same cuts, same two-pass
// Gaussian core fit, and same estimator preference (dTcfd > dTwls > dT) as
// scan.C, so numbers from this macro are directly comparable to scan.C's.
//
// Reads <base>/<subdir>/E<energy>GeV.root for each (f, subdir) pair below —
// edit RUNS to match whatever RADSIMPLE_OUT_SUBDIR values you actually used.

void coreFitAndSave(TH1* h, const char* title, const char* png,
                    double& mu, double& sg, double& sgErr) {
    double m = h->GetMean(), r = h->GetRMS();
    h->Fit("gaus", "Q0", "", m - 3*r, m + 3*r);
    TF1* g = h->GetFunction("gaus");
    mu = g->GetParameter(1); sg = g->GetParameter(2);
    h->Fit("gaus", "Q0", "", mu - 2*sg, mu + 2*sg);
    g = h->GetFunction("gaus");
    mu = g->GetParameter(1); sg = g->GetParameter(2); sgErr = g->GetParError(2);

    auto c = new TCanvas("cfit", "", 700, 500);
    h->SetTitle(title);
    h->GetXaxis()->SetRangeUser(mu - 6*sg, mu + 6*sg);
    h->Draw("hist");
    g->SetLineColor(kRed); g->Draw("same");
    c->SaveAs(png);
    delete c;
}

static const char* kHoleDist =
    "min(min(sqrt(x*x+y*y),min(sqrt((x-3.5)*(x-3.5)+(y-3.5)*(y-3.5)),"
    "sqrt((x+3.5)*(x+3.5)+(y+3.5)*(y+3.5)))),"
    "min(sqrt((x-3.5)*(x-3.5)+(y+3.5)*(y+3.5)),sqrt((x+3.5)*(x+3.5)+(y-3.5)*(y-3.5))))";

void fscan25(double energy = 25, const char* base = "build/rootfiles",
             double rMax = 3.5, double pbFrac = 0.05) {
    // EDIT to match the RADSIMPLE_OUT_SUBDIR values actually used.
    struct Rung { double f; const char* subdir; };
    const std::vector<Rung> RUNS = {
        {0.01, "fscan25_f1e-2"},
        {0.1,  "fscan25_f1e-1"},
        {0.5,  "fscan25_f5e-1"},
    };
    const int N = RUNS.size();

    gStyle->SetOptStat(0);
    TString PLOTS = Form("%s/plots_fscan25", base);
    gSystem->mkdir(Form("%s/fits", PLOTS.Data()), true);

    const TString FIDbase = Form("%s>1.5 && sqrt(x*x+y*y)<%g", kHoleDist, rMax);
    printf("fiducial: %s\n", FIDbase.Data());
    printf("containment veto: ePbGlass < %.0f%% of E_beam\n\n", 100*pbFrac);

    double F[N], sigT[N], sigTerr[N], resN[N], resNerr[N];

    printf("%-8s %14s %14s %7s\n", "f", "sigma_t (ps)", "Npe res (%)", "keep%");
    for (int i = 0; i < N; ++i) {
        F[i] = RUNS[i].f;
        TString fname = Form("%s/%s/E%.0fGeV.root", base, RUNS[i].subdir, energy);
        TFile file(fname);
        if (file.IsZombie()) {
            printf("  missing %s\n", fname.Data());
            sigT[i] = resN[i] = sigTerr[i] = resNerr[i] = 0;
            continue;
        }
        TTree* t = (TTree*)file.Get("ev");
        const char* tvar = t->GetBranch("dTcfd") ? "dTcfd"
                         : t->GetBranch("dTwls") ? "dTwls" : "dT";
        if (strcmp(tvar, "dTcfd") != 0)
            printf("  [!] %s: no dTcfd -- falling back to %s (not comparable"
                   " to CFD results)\n", fname.Data(), tvar);

        const TString FID = FIDbase + Form(" && ePbGlass < %g", pbFrac * energy);
        const TString tcut = FID + Form(" && %s>-999", tvar);

        // --- timing ---
        TH1D* d = new TH1D("dTfine", "", 400, -3, 3);
        t->Draw(Form("%s>>dTfine", tvar), tcut, "goff");
        double mD = d->GetMean(), rD = d->GetRMS();
        const int nbA = std::min(300, std::max(20, (int)(d->GetEntries()/8)));
        if (rD > 0) { d->SetBins(nbA, mD - 5*rD, mD + 5*rD);
                      t->Draw(Form("%s>>dTfine", tvar), tcut, "goff"); }
        d->SetDirectory(nullptr);
        double mu, sg, sgErr;
        coreFitAndSave(d, Form("#DeltaT at f=%g;#DeltaT = t_{down}-t_{up} (ns);events", F[i]),
                       Form("%s/fits/dT_f%g.png", PLOTS.Data(), F[i]), mu, sg, sgErr);
        sigT[i] = 1000 * sg / 2; sigTerr[i] = 1000 * sgErr / 2;

        // --- energy: MEASURED, detected light (Npe), same method as scan.C ---
        TH1D* nh = new TH1D("NpeFine", "", 100, t->GetMinimum("Npe"), t->GetMaximum("Npe"));
        t->Draw("Npe>>NpeFine", FID, "goff");
        double mN = nh->GetMean(), rN = nh->GetRMS();
        const int nbN = std::min(300, std::max(20, (int)(nh->GetEntries()/8)));
        nh->SetBins(nbN, mN - 5*rN, mN + 5*rN);
        t->Draw("Npe>>NpeFine", FID, "goff");
        nh->SetDirectory(nullptr);
        double muN, sgN, sgNErr;
        coreFitAndSave(nh, Form("N_{pe} at f=%g;detected photons;events", F[i]),
                       Form("%s/fits/Npe_f%g.png", PLOTS.Data(), F[i]), muN, sgN, sgNErr);
        resN[i] = 100 * sgN / muN;
        resNerr[i] = resN[i] * (sgNErr / sgN);

        long nFid = t->GetEntries(FID);
        printf("%-8g %6.1f +- %-6.1f %5.2f +- %-6.2f %6.1f%%\n",
               F[i], sigT[i], sigTerr[i], resN[i], resNerr[i],
               100.0 * nFid / t->GetEntries());
        delete d; delete nh;
    }

    // --- plot both vs f, log-x axis (f spans a wide range) ---
    auto c1 = new TCanvas("c1", "", 1400, 600);
    c1->Divide(2, 1);

    c1->cd(1); gPad->SetLogx();
    auto gT = new TGraphErrors(N, F, sigT, nullptr, sigTerr);
    gT->SetTitle(Form("timing resolution vs f, %.0f GeV;f (light fraction, log scale);#sigma_{t} (ps)", energy));
    gT->SetMarkerStyle(20); gT->SetMarkerColor(kBlue+1); gT->SetLineColor(kBlue+1);
    gT->Draw("APL");

    c1->cd(2); gPad->SetLogx();
    auto gE = new TGraphErrors(N, F, resN, nullptr, resNerr);
    gE->SetTitle(Form("energy resolution vs f, %.0f GeV;f (light fraction, log scale);#sigma_{E}/E (%%, measured/detected light)", energy));
    gE->SetMarkerStyle(21); gE->SetMarkerColor(kRed+1); gE->SetLineColor(kRed+1);
    gE->Draw("APL");

    c1->SaveAs(Form("%s/fscan25_timing_and_energy_vs_f.png", PLOTS.Data()));
    printf("\nplot: %s/fscan25_timing_and_energy_vs_f.png\n", PLOTS.Data());
    printf("(energy resolution is a pure dE/dx-adjacent quantity and should be\n"
           " roughly flat vs f -- if it isn't, that's worth a second look.\n"
           " sigma_t should fall steeply as f rises -- more light, less photon\n"
           " counting noise -- until/unless it turns over near the floor.)\n");
}
