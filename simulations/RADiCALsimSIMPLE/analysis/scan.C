// scan.C — build sigma_t vs E from the run.mac sweep. Usage (from RADiCALsimSIMPLE/):
//   root -l -b -q analysis/scan.C
//
// Reads build/E5GeV.root ... E120GeV.root (must match run.mac's energy list),
// Gaussian-fits each file's dT histogram (same as fit.C), takes
// sigma_t = sigma(dT)/2 with its fit error, and fits the standard calorimeter
// form sigma_t(E) = a/sqrt(E) (+) b. Saves build/plots/sigma_t_vs_E.png.
//
// REMINDER (see README "Knobs"): at RADSIMPLE_LYSO_SCALE = f (default 1e-2)
// the fitted 'a' is the THINNED value; the true-light 'a' is smaller by
// roughly sqrt(f), same idea as the full sim's scale ladder.
void scan() {
    // must match the energies (and order) in run.mac
    const int N = 6;
    const double E[N]      = {5, 10, 25, 50, 100, 120};
    const char*  files[N]  = {"build/E5GeV.root", "build/E10GeV.root",
                              "build/E25GeV.root", "build/E50GeV.root",
                              "build/E100GeV.root", "build/E120GeV.root"};

    double sigT[N], sigTerr[N];
    printf("%-8s %10s %10s\n", "E(GeV)", "sigma_t(ps)", "err(ps)");
    for (int i = 0; i < N; ++i) {
        TFile f(files[i]);
        if (f.IsZombie()) { printf("  missing %s\n", files[i]); sigT[i]=sigTerr[i]=0; continue; }
        TH1* d = (TH1*)f.Get("dT");
        d->GetXaxis()->SetRangeUser(d->GetMean()-5*d->GetRMS(), d->GetMean()+5*d->GetRMS());
        d->Fit("gaus", "Q0");                    // Q0: quiet, don't draw yet
        TF1* g = d->GetFunction("gaus");
        sigT[i]    = 1000 * g->GetParameter(2) / 2;   // ps, sigma_t = sigma(dT)/2
        sigTerr[i] = 1000 * g->GetParError(2)  / 2;
        printf("%-8.0f %10.1f %10.1f\n", E[i], sigT[i], sigTerr[i]);
        f.Close();
    }

    auto gr = new TGraphErrors(N, E, sigT, nullptr, sigTerr);
    gr->SetTitle("SIMPLE timing resolution;E_{beam} (GeV);#sigma_{t} (ps)");
    gr->SetMarkerStyle(20);

    auto fitf = new TF1("fitf", "sqrt([0]*[0]/x + [1]*[1])", E[0], E[N-1]);
    fitf->SetParameters(300, 30);                 // seed: a ~ few hundred, b ~ tens (ps)
    gr->Fit(fitf, "Q");

    gStyle->SetOptFit(1);
    auto c = new TCanvas("c", "sigma_t vs E", 800, 600);
    gr->Draw("AP");
    printf("\nfit: a = %.1f +- %.1f ps*sqrt(GeV),  b = %.1f +- %.1f ps\n",
           fitf->GetParameter(0), fitf->GetParError(0),
           fitf->GetParameter(1), fitf->GetParError(1));
    printf("(thinned at RADSIMPLE_LYSO_SCALE; true-light 'a' ~ this x sqrt(f))\n");

    gSystem->mkdir("build/plots", true);
    c->SaveAs("build/plots/sigma_t_vs_E.png");
}
