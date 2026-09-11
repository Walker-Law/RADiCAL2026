// tb26.C — the three requested comparisons for the August 2026 T10 beam test:
//   1. light yield per material          (mean detected photons vs beam energy)
//   2. energy resolution per material    (sigma/mean of the 4-corner photon sum)
//   3. timing resolution per material    (two estimators, see below)
// Usage (from Aug26TestBeam/):   root -l -b -q analysis/tb26.C
//
// Reads build/rootfiles/<material>/E<N>GeV.root for material in {luag, dsb1,
// ej199} and N in {1,3,5,7,9,11}; whatever exists is used, the rest is skipped.
// Plots go to build/plots/, one PNG per comparison with every material overlaid,
// plus the fitted histogram behind every number under build/plots/fits/.
//
// TIMING, two numbers per point, both from the per-corner 5% quantile times:
//   sigma(dTpair)    the REALIZABLE one: diagonal corner pairs differenced and
//                    averaged, no external reference; equals the single-corner
//                    resolution directly (EventAction.hh explains the algebra).
//   sigma(tUpMean)   the 4-corner mean against a PERFECT time reference — the
//                    best any MCP-referenced measurement could do. Upper bound.
//
// Every number is a two-pass Gaussian CORE fit with ADAPTIVE binning (~8
// events per bin, clamped to [20,300]) — a fixed-bin fit on low statistics has
// faked results in this project three times (ROADMAP Discovery 9, 13, 16).
// Points with fewer than 50 fiducial events are skipped; a fitted relative
// error above 30% is printed with a warning and drawn hollow.
//
// Fiducial cut, same as RADiCALsimSIMPLE: beam within 3.5 mm of the axis and
// more than 1.5 mm from any of the five capillary holes. No containment veto
// (there is no lead glass in this setup).

#include <vector>
#include <string>
#include <cmath>

static const char* kHoleDist =
    "min(min(sqrt(x*x+y*y),min(sqrt((x-3.5)*(x-3.5)+(y-3.5)*(y-3.5)),"
    "sqrt((x+3.5)*(x+3.5)+(y+3.5)*(y+3.5)))),"
    "min(sqrt((x-3.5)*(x-3.5)+(y+3.5)*(y+3.5)),sqrt((x+3.5)*(x+3.5)+(y-3.5)*(y-3.5))))";

// Two-pass Gaussian core fit on an adaptively-binned histogram built from a
// tree expression. Returns false if there is too little to fit.
static bool coreFit(TTree* t, const char* expr, const TString& cut,
                    const char* title, const char* png,
                    double& mu, double& sg, double& sgErr) {
    mu = sg = sgErr = 0;
    TH1D probe("probe", "", 1000, t->GetMinimum(expr), t->GetMaximum(expr) + 1e-9);
    t->Draw(Form("%s>>probe", expr), cut, "goff");
    const long n = (long)probe.GetEntries();
    if (n < 50) return false;
    const double m = probe.GetMean(), r = probe.GetRMS();
    if (r <= 0) return false;
    const int nb = std::min(300, std::max(20, (int)(n/8)));
    TH1D h("h", title, nb, m - 5*r, m + 5*r);
    t->Draw(Form("%s>>h", expr), cut, "goff");
    h.Fit("gaus", "Q0", "", m - 3*r, m + 3*r);
    TF1* g = h.GetFunction("gaus");
    if (!g) return false;
    mu = g->GetParameter(1); sg = g->GetParameter(2);
    if (sg <= 0) return false;
    h.Fit("gaus", "Q0", "", mu - 2*sg, mu + 2*sg);
    g = h.GetFunction("gaus");
    if (!g) return false;
    mu = g->GetParameter(1); sg = g->GetParameter(2); sgErr = g->GetParError(2);
    TCanvas c("cfit", "", 700, 500);
    h.GetXaxis()->SetRangeUser(mu - 6*sg, mu + 6*sg);
    h.Draw("hist"); g->SetLineColor(kRed); g->Draw("same");
    c.SaveAs(png);
    return true;
}

struct TbPoint { double E, ly, lyErr, resN, resNErr, sT, sTErr, sU, sUErr;
               double fCher, fDirect, fWls, fFil; long nFid; };

void tb26(const char* base = "build/rootfiles", double rMax = 3.5) {
    gStyle->SetOptStat(0);
    const std::vector<std::string> MATS = {"luag", "dsb1", "ej199"};
    const std::vector<double> ENERGIES = {1, 3, 5, 7, 9, 11};
    const int COL[3] = {kGreen+2, kOrange+7, kAzure+1};

    TString PLOTS = base; PLOTS.ReplaceAll("rootfiles", "plots");
    if (PLOTS == base) PLOTS += "/plots";
    gSystem->mkdir(Form("%s/fits", PLOTS.Data()), true);

    const TString FID = Form("%s>1.5 && sqrt(x*x+y*y)<%g", kHoleDist, rMax);
    printf("fiducial: r<%g mm, >1.5 mm from any hole; no containment veto\n", rMax);

    std::vector<std::vector<TbPoint>> all(MATS.size());
    for (size_t im = 0; im < MATS.size(); ++im) {
        const std::string& mat = MATS[im];
        bool any = false;
        for (double E : ENERGIES) {
            TString fn = Form("%s/%s/E%.0fGeV.root", base, mat.c_str(), E);
            if (gSystem->AccessPathName(fn)) continue;
            TFile f(fn);
            TTree* t = (TTree*)f.Get("ev");
            if (!t) continue;
            if (!any) { printf("\n=== %s ===\n%-6s %8s %14s %14s %16s %16s   %s\n", mat.c_str(),
                               "E(GeV)", "fiducial", "<Npe>", "sigma_E/E (%)",
                               "sigma_t pair(ps)", "sigma_t ideal(ps)",
                               "light: Cher/direct/shifted/filament"); any = true; }
            Point p{}; p.E = E;
            p.nFid = t->GetEntries(FID);
            double mu, sg, se;
            const TString tag = Form("%s_E%.0f", mat.c_str(), E);
            // light yield + measured energy resolution from the same fit
            if (coreFit(t, "Npe", FID, Form("N_{pe} %s %.0f GeV;detected photons;events", mat.c_str(), E),
                        Form("%s/fits/Npe_%s.png", PLOTS.Data(), tag.Data()), mu, sg, se)) {
                p.ly = mu; p.lyErr = sg / std::sqrt((double)std::max(1L, p.nFid));
                p.resN = 100*sg/mu; p.resNErr = p.resN * (se/sg);
            }
            if (coreFit(t, "dTpair", FID + " && dTpair>-999",
                        Form("#DeltaT_{pair} %s %.0f GeV;#DeltaT_{pair} (ns);events", mat.c_str(), E),
                        Form("%s/fits/dTpair_%s.png", PLOTS.Data(), tag.Data()), mu, sg, se)) {
                p.sT = 1000*sg; p.sTErr = 1000*se;
            }
            if (coreFit(t, "tUpMean", FID + " && tUpMean>-999",
                        Form("t_{up,mean} %s %.0f GeV;t (ns);events", mat.c_str(), E),
                        Form("%s/fits/tUpMean_%s.png", PLOTS.Data(), tag.Data()), mu, sg, se)) {
                p.sU = 1000*sg; p.sUErr = 1000*se;
            }
            // light composition (mean fractions over fiducial events)
            auto frac = [&](const char* col) {
                TH1D hh("hh", "", 1, -1e9, 1e9);
                t->Draw(Form("%s/Npe>>hh", col), FID + " && Npe>0", "goff");
                double m = 0; long n = 0;
                TTreeFormula fx("fx", Form("%s/Npe", col), t); TTreeFormula fc("fc", FID + " && Npe>0", t);
                for (long i = 0; i < t->GetEntries(); ++i) { t->GetEntry(i);
                    if (fc.EvalInstance() != 0) { m += fx.EvalInstance(); ++n; } }
                return n ? m/n : 0.;
            };
            p.fCher = frac("NpeCher"); p.fDirect = frac("NpeDirect");
            p.fWls = frac("NpeWLS");   p.fFil = frac("NpeFil");

            const char* warn = "";
            if (p.sT > 0 && p.sTErr/p.sT > 0.30) warn = "  [!] >30% error on timing";
            printf("%-6.0f %8ld %7.0f +- %-5.0f %6.2f +- %-5.2f %7.1f +- %-6.1f %7.1f +- %-6.1f   %.2f/%.2f/%.2f/%.2f%s\n",
                   E, p.nFid, p.ly, p.lyErr, p.resN, p.resNErr, p.sT, p.sTErr, p.sU, p.sUErr,
                   p.fCher, p.fDirect, p.fWls, p.fFil, warn);
            if (p.nFid < 50) printf("       (fewer than 50 fiducial events — point skipped in plots)\n");
            else all[im].push_back(p);
        }
    }

    // ---- three overlay plots -------------------------------------------------
    auto makePlot = [&](const char* name, const char* ytitle, int which) {
        TCanvas c(name, "", 800, 600);
        c.SetLogx(false);
        TMultiGraph mg; TLegend lg(0.55, 0.65, 0.88, 0.88); lg.SetBorderSize(0);
        bool anyG = false;
        for (size_t im = 0; im < MATS.size(); ++im) {
            std::vector<double> x, y, ey, x2, y2, ey2;
            for (const Point& p : all[im]) {
                double v = 0, e = 0;
                if (which == 0) { v = p.ly;   e = p.lyErr; }
                if (which == 1) { v = p.resN; e = p.resNErr; }
                if (which == 2) { v = p.sT;   e = p.sTErr; }
                if (v > 0) { x.push_back(p.E); y.push_back(v); ey.push_back(e); }
                if (which == 2 && p.sU > 0) { x2.push_back(p.E); y2.push_back(p.sU); ey2.push_back(p.sUErr); }
            }
            if (x.empty()) continue;
            anyG = true;
            auto g = new TGraphErrors((int)x.size(), &x[0], &y[0], nullptr, &ey[0]);
            g->SetMarkerStyle(20); g->SetMarkerColor(COL[im]); g->SetLineColor(COL[im]);
            mg.Add(g, "LP");
            lg.AddEntry(g, Form("%s%s", MATS[im].c_str(), which == 2 ? " (pair, realizable)" : ""), "lp");
            if (which == 2 && !x2.empty()) {
                auto g2 = new TGraphErrors((int)x2.size(), &x2[0], &y2[0], nullptr, &ey2[0]);
                g2->SetMarkerStyle(24); g2->SetMarkerColor(COL[im]); g2->SetLineColor(COL[im]);
                g2->SetLineStyle(2);
                mg.Add(g2, "LP");
                lg.AddEntry(g2, Form("%s (ideal reference)", MATS[im].c_str()), "lp");
            }
        }
        if (!anyG) return;
        mg.SetTitle(Form(";beam energy (GeV);%s", ytitle));
        mg.Draw("A"); lg.Draw();
        c.SaveAs(Form("%s/tb26_%s.png", PLOTS.Data(), name));
    };
    makePlot("lightyield_vs_E", "detected photons per event (4 upstream corners)", 0);
    makePlot("energyres_vs_E",  "#sigma(N_{pe})/N_{pe} (%)", 1);
    makePlot("timing_vs_E",     "#sigma_{t} (ps)", 2);

    // ---- the stochastic/constant fits, per material --------------------------
    printf("\n=== fits: sigma_E/E = a/sqrt(E) (+) b,  sigma_t = a/sqrt(E) (+) b ===\n");
    for (size_t im = 0; im < MATS.size(); ++im) {
        if (all[im].size() < 3) continue;
        std::vector<double> x, yE, eE, yT, eT;
        for (const Point& p : all[im]) {
            if (p.resN > 0) { x.push_back(p.E); yE.push_back(p.resN); eE.push_back(p.resNErr); }
        }
        TF1 fE("fE", "sqrt([0]*[0]/x+[1]*[1])", 0.5, 12);
        fE.SetParameters(20, 5);
        TGraphErrors gE((int)x.size(), &x[0], &yE[0], nullptr, &eE[0]);
        gE.Fit(&fE, "Q");
        printf("%-6s energy: %.1f%%/sqrt(E) (+) %.2f%%   (chi2/ndf %.2f)\n", MATS[im].c_str(),
               std::fabs(fE.GetParameter(0)), std::fabs(fE.GetParameter(1)),
               fE.GetNDF() > 0 ? fE.GetChisquare()/fE.GetNDF() : 0.);
        x.clear();
        for (const Point& p : all[im]) {
            if (p.sT > 0 && p.sTErr/p.sT <= 0.30) { x.push_back(p.E); yT.push_back(p.sT); eT.push_back(p.sTErr); }
        }
        if (x.size() >= 3) {
            TF1 fT("fT", "sqrt([0]*[0]/x+[1]*[1])", 0.5, 12);
            fT.SetParameters(100, 30);
            TGraphErrors gT((int)x.size(), &x[0], &yT[0], nullptr, &eT[0]);
            gT.Fit(&fT, "Q");
            printf("%-6s timing: %.0f ps/sqrt(E) (+) %.1f ps   (chi2/ndf %.2f)\n", MATS[im].c_str(),
                   std::fabs(fT.GetParameter(0)), std::fabs(fT.GetParameter(1)),
                   fT.GetNDF() > 0 ? fT.GetChisquare()/fT.GetNDF() : 0.);
        }
    }
    printf("\nplots: %s/tb26_{lightyield,energyres,timing}_vs_E.png  (fits under %s/fits/)\n",
           PLOTS.Data(), PLOTS.Data());
    printf("sigma_t(pair) needs no factor of 2; sigma_t(ideal) is an upper bound on any\n"
           "externally-referenced measurement. Both are LIGHT-side numbers: no electronics.\n");
}
