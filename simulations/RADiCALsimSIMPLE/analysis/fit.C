// fit.C — read one radsimple_output.root and print the timing and energy
// resolution. Usage:  root -l -b -q 'analysis/fit.C("build/radsimple_output.root")'
//
// Timing:  fit a Gaussian to the dT distribution; the single-ended timing
//          resolution is sigma_t = sigma(dT)/2.
// Energy:  fit a Gaussian to the LYSO-energy distribution; report sigma/mean.
void fit(const char* file = "build/radsimple_output.root") {
    TFile f(file);
    if (f.IsZombie()) { printf("cannot open %s\n", file); return; }

    // WLS-only timing (2026-07-29 fix); all-light "dT" is multi-modal at
    // thinned light and unfittable — see scan.C for the full explanation.
    // dTcfd = the 5% CFD (2026-08-02); older files carry first-photon
    // dTwls/dT, which are NOT comparable to CFD numbers.
    TH1* d = (TH1*)f.Get("dTcfd");
    if (!d) { d = (TH1*)f.Get("dTwls");
              printf("  [!] no dTcfd (old file) -- first-photon fallback, not comparable to CFD\n"); }
    if (!d) { d = (TH1*)f.Get("dT");
              printf("  [!] oldest schema: all-light first photon, unreliable\n"); }
    TH1* e = (TH1*)f.Get("Elyso");
    TH1* n = (TH1*)f.Get("Npe");
    printf("\n%-14s %s\n", "events", "");
    printf("%-14s %.0f\n", "events", d->GetEntries());

    // --- timing: Gaussian fit around the dT peak ---
    d->GetXaxis()->SetRangeUser(d->GetMean()-5*d->GetRMS(), d->GetMean()+5*d->GetRMS());
    d->Fit("gaus", "Q");
    TF1* g = d->GetFunction("gaus");
    double sdt = g ? g->GetParameter(2) : d->GetRMS();     // ns
    printf("%-14s dT = %.3f ns,  sigma(dT) = %.1f ps\n", "timing", g?g->GetParameter(1):d->GetMean(), 1000*sdt);
    printf("%-14s sigma_t = sigma(dT)/2 = %.1f ps\n", "", 1000*sdt/2);

    // --- energy: Gaussian fit ---
    e->Fit("gaus", "Q");
    TF1* ge = e->GetFunction("gaus");
    if (ge && ge->GetParameter(1) > 0)
        printf("%-14s E_LYSO = %.2f GeV,  sigma/E = %.2f %%\n", "energy",
               ge->GetParameter(1), 100*ge->GetParameter(2)/ge->GetParameter(1));

    printf("%-14s %.0f detected photons/event\n", "light", n->GetMean());
    printf("\n(reminder: at RADSIMPLE_LYSO_SCALE=f, sigma_t is ~1/sqrt(f) worse than\n"
           " true light; extrapolate by multiplying by sqrt(f).)\n\n");
}
