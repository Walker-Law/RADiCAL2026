// wrap_scan.C — compare every wrap configuration against the no-wrap control.
// Usage (from RADiCALsimWrap/):   root -l -b -q analysis/wrap_scan.C
//                                 root -l -b -q 'analysis/wrap_scan.C("results")'
//
// Reads results/<config>/E<N>GeV.root for every config folder present, and for
// each one measures the three things a wrap can change:
//
//   Npe          mean detected light      -- does the wrap buy photons?
//   sigma_t      sigma(dT)/2              -- does that light help or hurt timing?
//   sigma(Npe)/Npe   energy resolution    -- does it help the energy measurement?
//
// Everything is quoted as a percentage change against the "none" config, which
// is byte-identical geometry to RADiCALsimSIMPLE. All configs are run with the
// same random seeds (see run_wrap_scan.sh), so the shower fluctuations are
// common mode and the differences below are the wrap, not the beam.
//
// PLOTS (each figure holds exactly ONE observable; configs are the series):
//   plots/npe_vs_E.png            mean detected light
//   plots/npe_gain_vs_E.png       the same, as % change vs no wrap
//   plots/sigma_t_vs_E.png        timing resolution
//   plots/sigma_E_Npe_vs_E.png    energy resolution from detected light
//   plots/fits/dT_<config>_E<N>GeV.png   the dT histogram behind each sigma_t

// Is this histogram multi-modal? Counts local maxima that rise above `frac` of
// the global peak, on a lightly smoothed copy so bin noise does not register.
//
// WHY THIS GUARD EXISTS: before 2026-07-29 the timing histogram was the
// ALL-light dT, which at thinned light is a comb of ~5 discrete spikes (a 1%
// prompt-Cherenkov population arriving ~3.7 ns before the WLS bulk wins the
// first-photon race at random, per corner). A Gaussian core fit across that
// comb returned confident-looking nonsense — 43 ps at one energy, 867 ps at
// the next. Fitting dTwls fixes the physics; this guard makes sure that if a
// distribution is EVER multi-modal again, the number is flagged, not trusted.
int countModes(TH1* h, double frac = 0.35) {
    if (!h || h->GetEntries() < 100) return 0;
    TH1* s = (TH1*)h->Clone("modeScan");
    s->SetDirectory(nullptr);
    s->Rebin(8);
    s->Smooth(2);
    const double pk = s->GetMaximum();
    int modes = 0;
    for (int i = 2; i < s->GetNbinsX(); ++i) {
        const double y = s->GetBinContent(i);
        if (y > frac*pk && y >= s->GetBinContent(i-1) && y > s->GetBinContent(i+1))
            ++modes;
    }
    delete s;
    return modes;
}

// Two-pass Gaussian core fit: fit over mean +- 3*RMS, then refit within
// mu +- 2*sigma so leakage tails do not inflate the width. png = "" -> no file.
// MIN_FIT_ENTRIES: below this a Gaussian core fit is meaningless. Kept low
// enough that a small smoke-test run still exercises this code path; anything
// under ~100 entries is flagged as statistically unreliable in the table.
const int MIN_FIT_ENTRIES = 10;

void coreFit(TH1* h, const char* title, const char* png,
             double& mu, double& sg, double& sgErr) {
    mu = sg = sgErr = 0;
    if (!h || h->GetEntries() < MIN_FIT_ENTRIES) return;
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

    if (png && png[0]) {
        auto c = new TCanvas("cfit", "", 700, 500);
        h->SetTitle(title);
        h->GetXaxis()->SetRangeUser(mu - 6*sg, mu + 6*sg);
        h->Draw("hist");
        g->SetLineColor(kRed); g->Draw("same");
        c->SaveAs(png);
        delete c;
    }
}

// Rebuild a histogram from the UNBINNED ntuple column, focused on its own peak.
// (The stored H1s have fixed wide bins sized for any energy, which quantizes the
// fit at low energy — the same trap fixed in RADiCALsimSIMPLE's scan.C.)
TH1D* fineHist(TTree* t, const char* col, const char* name) {
    double lo = t->GetMinimum(col), hi = t->GetMaximum(col);
    if (!(hi > lo)) return nullptr;
    TH1D* h = new TH1D(name, "", 100, lo, hi);
    t->Draw(Form("%s>>%s", col, name), "", "goff");
    double m = h->GetMean(), r = h->GetRMS();
    if (r > 0) {
        h->SetBins(100, m - 5*r, m + 5*r);
        t->Draw(Form("%s>>%s", col, name), "", "goff");
    }
    h->SetDirectory(nullptr);
    return h;
}

void wrap_scan(const char* dir = "results") {
    gStyle->SetOptStat(0);
    gSystem->mkdir(Form("%s/plots/fits", dir), true);

    // ---- discover which configs were actually run -------------------------
    std::vector<std::string> cfg;
    void* dp = gSystem->OpenDirectory(dir);
    if (!dp) { printf("no such directory: %s  (run run_wrap_scan.sh first)\n", dir); return; }
    const char* ent;
    while ((ent = gSystem->GetDirEntry(dp))) {
        std::string n = ent;
        if (n == "." || n == ".." || n == "plots") continue;
        if (gSystem->AccessPathName(Form("%s/%s/sweep.mac", dir, n.c_str()))) continue;
        cfg.push_back(n);
    }
    gSystem->FreeDirectory(dp);
    if (cfg.empty()) { printf("no config folders under %s/\n", dir); return; }
    std::sort(cfg.begin(), cfg.end());
    // the control goes first so every table reads "baseline, then the rest"
    for (size_t i = 0; i < cfg.size(); ++i)
        if (cfg[i] == "none") { std::swap(cfg[0], cfg[i]); break; }
    const bool haveBase = (cfg[0] == "none");

    // ---- discover which energies exist, across ALL configs ----------------
    // Must scan every config, not just the first: the staged control usually
    // comes from RADiCALsimSIMPLE's 6-energy run.mac while run_wrap_scan.sh
    // defaults to 3 energies, so the two do NOT have the same file list. Take
    // the union; energies a given config lacks simply show "-" in the table
    // and are skipped in the plots.
    const double ECAND[] = {5, 10, 25, 50, 100, 120};
    std::vector<double> E;
    for (double e : ECAND)
        for (size_t ic = 0; ic < cfg.size(); ++ic)
            if (!gSystem->AccessPathName(Form("%s/%s/E%.0fGeV.root", dir, cfg[ic].c_str(), e))) {
                E.push_back(e); break;
            }
    if (E.empty()) { printf("no E<N>GeV.root files under %s/*/\n", dir); return; }

    const int NC = cfg.size(), NE = E.size();
    printf("configs: %d   energies:", NC);
    for (int i = 0; i < NE; ++i) printf(" %.0f", E[i]);
    printf(" GeV\n");

    // [config][energy]
    std::vector<std::vector<double>> npe(NC, std::vector<double>(NE, 0)),
                                     sgt(NC, std::vector<double>(NE, 0)),
                                     sgtE(NC, std::vector<double>(NE, 0)),
                                     res(NC, std::vector<double>(NE, 0)),
                                     eff(NC, std::vector<double>(NE, 0));
    // Distinguish "this config never ran at this energy" (no file) from
    // "it ran but had too few events to fit" — they mean very different things
    // and printing both as a blank row hides a real problem.
    std::vector<std::vector<int>> nev(NC, std::vector<int>(NE, 0));   // 0 = no file
    std::vector<std::vector<int>> modes(NC, std::vector<int>(NE, 0)); // >1 = unfittable
    std::vector<std::vector<bool>> usedWLS(NC, std::vector<bool>(NE, false));

    for (int ic = 0; ic < NC; ++ic) {
        for (int ie = 0; ie < NE; ++ie) {
            TString fn = Form("%s/%s/E%.0fGeV.root", dir, cfg[ic].c_str(), E[ie]);
            // Not every config has every energy (see the union note above), so
            // a missing file is normal, not an error — leave zeros and move on.
            if (gSystem->AccessPathName(fn)) continue;
            TFile f(fn);
            if (f.IsZombie()) { printf("  [!] unreadable: %s\n", fn.Data()); continue; }
            TTree* t = (TTree*)f.Get("ev");
            if (!t) { printf("  [!] no ntuple in %s\n", fn.Data()); continue; }
            nev[ic][ie] = (int)t->GetEntries();

            double mu, sg, sgErr;

            // Timing: sigma_t = sigma(dT)/2. Prefer the WLS-only histogram
            // (2026-07-29 fix) — the all-light "dT" is multi-modal at thinned
            // light and cannot be fitted. Fall back to "dT" for files written
            // before the fix, where it is the only thing available.
            TH1* d = (TH1*)f.Get("dTwls");
            const bool wlsTiming = (d != nullptr);
            if (!d) d = (TH1*)f.Get("dT");
            usedWLS[ic][ie] = wlsTiming;
            coreFit(d, Form("#DeltaT%s, %s at %.0f GeV;#DeltaT (ns);events",
                            wlsTiming ? " (WLS only)" : " (ALL light - unfittable)",
                            cfg[ic].c_str(), E[ie]),
                    Form("%s/plots/fits/dT_%s_E%.0fGeV.png", dir, cfg[ic].c_str(), E[ie]),
                    mu, sg, sgErr);
            modes[ic][ie] = countModes(d);
            sgt[ic][ie]  = 1000 * sg    / 2;      // ns -> ps, /2 for single-ended
            sgtE[ic][ie] = 1000 * sgErr / 2;
            if (d && t->GetEntries() > 0)
                eff[ic][ie] = 100.0 * d->GetEntries() / t->GetEntries();

            // light: mean Npe and its relative spread (the energy observable)
            TH1D* nh = fineHist(t, "Npe", "NpeFine");
            if (nh) {
                coreFit(nh, "", "", mu, sg, sgErr);
                // A fitted mean <= 0 means the fit failed (it happens on very
                // low statistics, where the Gaussian runs away). Treat it as
                // no data rather than reporting a negative photon count.
                if (mu > 0) {
                    npe[ic][ie] = mu;
                    res[ic][ie] = 100 * sg / mu;
                }
                delete nh;
            }
            f.Close();
        }
    }

    // ---- the table, one block per energy ---------------------------------
    for (int ie = 0; ie < NE; ++ie) {
        printf("\n=== %.0f GeV ===\n", E[ie]);
        printf("%-16s %9s %8s   %11s %8s   %10s %7s\n",
               "config", "Npe", "dNpe%", "sigma_t(ps)", "dsig%", "Npe res%", "eff%");
        for (int ic = 0; ic < NC; ++ic) {
            if (!(npe[ic][ie] > 0) && !(sgt[ic][ie] > 0)) {
                const char* why = (nev[ic][ie] == 0)
                    ? "(not run at this energy)"
                    : "(ran, but too few events to fit)";
                printf("%-16s %9s %8s   %11s %8s   %10s %7s   %s\n",
                       cfg[ic].c_str(), "-", "-", "-", "-", "-", "-", why);
                continue;
            }
            TString dN = "--", dS = "--";
            if (haveBase && ic > 0) {
                if (npe[0][ie] > 0 && npe[ic][ie] > 0)
                    dN = Form("%+.1f", 100*(npe[ic][ie]-npe[0][ie])/npe[0][ie]);
                if (sgt[0][ie] > 0 && sgt[ic][ie] > 0)
                    dS = Form("%+.1f", 100*(sgt[ic][ie]-sgt[0][ie])/sgt[0][ie]);
            }
            TString warn = "";
            if (nev[ic][ie] < 100) warn += "  [LOW STATS, do not trust]";
            if (modes[ic][ie] > 1)
                warn += Form("  [sigma_t INVALID: %d-modal, Gaussian fit meaningless]",
                             modes[ic][ie]);
            if (!usedWLS[ic][ie]) warn += "  [pre-2026-07-29 file: all-light dT]";
            if (eff[ic][ie] < 99.0) warn += "  [sigma_t biased: dim events dropped]";
            printf("%-16s %9.0f %8s   %6.1f+-%-4.1f %8s   %10.2f %7.1f%s\n",
                   cfg[ic].c_str(), npe[ic][ie], dN.Data(),
                   sgt[ic][ie], sgtE[ic][ie], dS.Data(),
                   res[ic][ie], eff[ic][ie], warn.Data());
        }
    }
    if (!haveBase)
        printf("\n[!] no 'none' config found — percentages are omitted. Run the\n"
               "    control with: RADWRAP_CONFIGS=none bash run_wrap_scan.sh\n");

    // ---- plots: one observable per figure, configs as the series ----------
    const int COL[] = {kBlack, kAzure+2, kGreen+2, kRed+1, kMagenta+1,
                       kOrange+7, kCyan+2, kGray+2};
    const int MRK[] = {20, 21, 22, 23, 29, 33, 34, 47};

    auto makePlot = [&](const std::vector<std::vector<double>>& v,
                        const std::vector<std::vector<double>>* err,
                        const char* title, const char* png, bool asGain) {
        auto c = new TCanvas("c", "", 850, 620);
        if (NE > 1) c->SetLogx();
        auto mg = new TMultiGraph();
        auto lg = new TLegend(0.62, 0.60, 0.89, 0.89);
        lg->SetBorderSize(0); lg->SetFillStyle(0);
        for (int ic = 0; ic < NC; ++ic) {
            if (asGain && ic == 0) continue;            // baseline is 0 by definition
            std::vector<double> x, y, ey;
            for (int ie = 0; ie < NE; ++ie) {
                double val = v[ic][ie];
                if (asGain) {
                    if (!(v[0][ie] > 0)) continue;
                    val = 100 * (v[ic][ie] - v[0][ie]) / v[0][ie];
                } else if (!(val > 0)) continue;
                x.push_back(E[ie]); y.push_back(val);
                ey.push_back(err ? (*err)[ic][ie] : 0.);
            }
            if (x.empty()) continue;
            auto g = new TGraphErrors(x.size(), &x[0], &y[0], nullptr, &ey[0]);
            g->SetTitle(cfg[ic].c_str());
            g->SetMarkerStyle(MRK[ic % 8]); g->SetMarkerColor(COL[ic % 8]);
            g->SetLineColor(COL[ic % 8]);   g->SetMarkerSize(1.3);
            mg->Add(g, "LP");
            lg->AddEntry(g, cfg[ic].c_str(), "lp");
        }
        mg->SetTitle(title);
        mg->Draw("A");
        mg->GetXaxis()->SetMoreLogLabels(); mg->GetXaxis()->SetNoExponent();
        if (asGain) {
            auto z = new TLine(mg->GetXaxis()->GetXmin(), 0,
                               mg->GetXaxis()->GetXmax(), 0);
            z->SetLineStyle(2); z->SetLineColor(kGray+2); z->Draw();
        }
        lg->Draw();
        c->SaveAs(Form("%s/plots/%s", dir, png));
        delete c;
    };

    makePlot(npe, nullptr, "detected light;E_{beam} (GeV);mean N_{pe}",
             "npe_vs_E.png", false);
    if (haveBase)
        makePlot(npe, nullptr,
                 "light gain from the wrap;E_{beam} (GeV);#DeltaN_{pe} vs no wrap (%)",
                 "npe_gain_vs_E.png", true);
    makePlot(sgt, &sgtE, "timing resolution;E_{beam} (GeV);#sigma_{t} (ps)",
             "sigma_t_vs_E.png", false);
    makePlot(res, nullptr,
             "energy resolution from detected light;E_{beam} (GeV);#sigma_{E}/E (%)",
             "sigma_E_Npe_vs_E.png", false);

    // ---- the one-line answer ---------------------------------------------
    // Pick the energy where the MOST configs can actually be compared against
    // the control — not just the middle of the list, which can easily be an
    // energy only the (6-energy) staged control was run at.
    if (haveBase && NE > 0) {
        int best = -1, bestN = 0;
        for (int ie = 0; ie < NE; ++ie) {
            if (!(npe[0][ie] > 0) || !(sgt[0][ie] > 0)) continue;   // control missing
            int n = 0;
            for (int ic = 1; ic < NC; ++ic)
                if (npe[ic][ie] > 0 && sgt[ic][ie] > 0) ++n;
            if (n >= bestN) { bestN = n; best = ie; }   // >= prefers higher energy
        }
        printf("\n---------------------------------------------------------------\n");
        if (best < 0 || bestN == 0) {
            printf("NO VERDICT: no energy has both the control and a wrap config.\n");
            printf("  Run the wrap configs at an energy the control also covers,\n");
            printf("  or stage a control that covers the energies you ran.\n");
        } else {
            printf("VERDICT at %.0f GeV (vs no wrap):\n", E[best]);
            for (int ic = 1; ic < NC; ++ic) {
                if (!(npe[ic][best] > 0) || !(sgt[ic][best] > 0)) continue;
                printf("  %-16s light %+6.1f%%   sigma_t %+6.1f%%   energy res %+6.1f%%%s\n",
                       cfg[ic].c_str(),
                       100*(npe[ic][best]-npe[0][best])/npe[0][best],
                       100*(sgt[ic][best]-sgt[0][best])/sgt[0][best],
                       (res[0][best] > 0) ? 100*(res[ic][best]-res[0][best])/res[0][best] : 0.,
                       (nev[ic][best] < 100 || nev[0][best] < 100) ? "   [LOW STATS]" : "");
            }
            printf("  (negative sigma_t / energy res = BETTER; negative light = worse)\n");
        }
        printf("---------------------------------------------------------------\n");
    }

    printf("\nplots: %s/plots/\n"
           "NOTE: sigma_t here is measured at thinned light "
           "(RADSIMPLE_LIGHT_SCALE, default 1e-2), so its ABSOLUTE value is ~10x\n"
           "      the true-light number. The RELATIVE comparison between configs "
           "is the result; the absolute ps value is not.\n", dir);
}
