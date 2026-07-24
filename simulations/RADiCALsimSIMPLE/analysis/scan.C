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

void scan() {
    // must match the energies (and order) in run.mac
    const int N = 6;
    const double E[N]     = {5, 10, 25, 50, 100, 120};
    const char* files[N]  = {"build/E5GeV.root",  "build/E10GeV.root",
                             "build/E25GeV.root", "build/E50GeV.root",
                             "build/E100GeV.root","build/E120GeV.root"};

    gStyle->SetOptStat(0);
    gSystem->mkdir("build/plots/fits", true);

    double sigT[N], sigTerr[N];      // timing:  sigma_t (ps)
    double resE[N], resEerr[N];      // energy:  sigma_E/E (%)

    printf("%-8s %14s %16s\n", "E(GeV)", "sigma_t (ps)", "sigma_E/E (%)");
    for (int i = 0; i < N; ++i) {
        TFile f(files[i]);
        if (f.IsZombie()) { printf("  missing %s\n", files[i]); sigT[i]=resE[i]=0; sigTerr[i]=resEerr[i]=0; continue; }
        double mu, sg, sgErr;

        // --- timing: dT distribution -> sigma_t = sigma(dT)/2 ---
        TH1* d = (TH1*)f.Get("dT");
        coreFitAndSave(d, Form("#DeltaT at %.0f GeV;#DeltaT = t_{down}-t_{up} (ns);events", E[i]),
                       Form("build/plots/fits/dT_E%.0fGeV.png", E[i]), mu, sg, sgErr);
        sigT[i]    = 1000 * sg    / 2;                       // ns->ps, /2 corner-trick
        sigTerr[i] = 1000 * sgErr / 2;

        // --- energy: Elyso spread -> sigma/mean at this fixed beam energy ---
        TH1* e = (TH1*)f.Get("Elyso");
        double muE, sgE, sgEErr;
        coreFitAndSave(e, Form("E_{LYSO} at %.0f GeV;E_{LYSO} (GeV);events", E[i]),
                       Form("build/plots/fits/Elyso_E%.0fGeV.png", E[i]), muE, sgE, sgEErr);
        resE[i]    = 100 * sgE / muE;
        resEerr[i] = resE[i] * sqrt(pow(sgEErr/sgE, 2));     // sigma error dominates

        printf("%-8.0f %8.1f +- %-5.1f %9.2f +- %-5.2f\n",
               E[i], sigT[i], sigTerr[i], resE[i], resEerr[i]);
        f.Close();
    }

    // --- graph 1: sigma_t vs E,  fit a/sqrt(E) (+) b ---
    {
        auto gr = new TGraphErrors(N, E, sigT, nullptr, sigTerr);
        gr->SetTitle("SIMPLE timing resolution;E_{beam} (GeV);#sigma_{t} (ps)");
        gr->SetMarkerStyle(20);
        auto fit = new TF1("ft", "sqrt([0]*[0]/x + [1]*[1])", E[0], E[N-1]);
        fit->SetParameters(300, 30);
        gr->Fit(fit, "Q");
        gStyle->SetOptFit(1);
        auto c = new TCanvas("ct", "", 800, 600);
        gr->Draw("AP");
        c->SaveAs("build/plots/sigma_t_vs_E.png");
        printf("\ntiming: sigma_t = %.1f/sqrt(E) (+) %.1f ps   (+- %.1f / %.1f)\n",
               fit->GetParameter(0), fabs(fit->GetParameter(1)),
               fit->GetParError(0), fit->GetParError(1));
        delete c;
    }

    // --- graph 2: sigma_E/E vs E,  fit a/sqrt(E) (+) b (in %) ---
    {
        auto gr = new TGraphErrors(N, E, resE, nullptr, resEerr);
        gr->SetTitle("SIMPLE energy resolution;E_{beam} (GeV);#sigma_{E}/E (%)");
        gr->SetMarkerStyle(21);
        auto fit = new TF1("fe", "sqrt([0]*[0]/x + [1]*[1])", E[0], E[N-1]);
        fit->SetParameters(15, 2);
        gr->Fit(fit, "Q");
        auto c = new TCanvas("ce", "", 800, 600);
        gr->Draw("AP");
        c->SaveAs("build/plots/sigma_E_vs_E.png");
        printf("energy: sigma_E/E = %.1f%%/sqrt(E) (+) %.2f%%   (+- %.1f / %.2f)\n",
               fit->GetParameter(0), fabs(fit->GetParameter(1)),
               fit->GetParError(0), fit->GetParError(1));
        delete c;
    }

    printf("\nplots: build/plots/sigma_t_vs_E.png, sigma_E_vs_E.png,"
           " fits/dT_E*.png, fits/Elyso_E*.png\n"
           "(sigma_t is thinned by RADSIMPLE_LYSO_SCALE; sigma_E/E is not.)\n");
}
