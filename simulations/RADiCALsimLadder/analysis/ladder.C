// ladder.C — photostatistics scale-ladder decomposition for the RADiCAL timing
// simulation, on the realistic-composition + realistic-electronics config.
//
//   cd RADiCALsimLadder && root -l -b -q 'analysis/ladder.C(500)'
//
// Reads scan/optical_scan_<nevt>_paperR_lad<f>/ (self-contained copies of the
// merged per-energy ROOT files). For each light factor f it pulls the stored
// all-light + electronics headline fit (TimingResolution_4cAllDRS4), measures
// the ACTUAL light ratio from <PhotonsWLS>, then fits
//
//     a^2(f) = A^2/f + B^2        (linear in x = 1/f)
//
// separating the photostatistic term (A) from the light-independent floor (B),
// and extrapolates to true light (f = 1/base_scale = 100). It ALSO reports two
// diagnostics that turned out to carry the headline: the WLS capture*PDE
// fraction (H1 PhotonsWLS / PhotonsWLSEmitted) and the low-gain energy
// linearity (fired-pixel / E, flat == unsaturated == like the paper's Fig 17).
//
// Writes results/ladder_summary.txt and results/ladder.png.

#pragma GCC diagnostic ignored "-Wformat-security"   // P() forwards our own literals

void ladder(int nevt = 500) {
  const int NF = 4;
  const double fnom[NF] = {0.1, 0.3, 1., 3.};
  const double baseScale = 1e-2;                 // paperR thinning at f=1
  const int EREF = 50;
  const int NE = 6; double Egrid[NE] = {25,50,75,100,125,150};

  FILE* out = fopen("results/ladder_summary.txt", "w");
  auto P = [&](const char* fmt, auto... args){ printf(fmt, args...); fprintf(out, fmt, args...); };

  P("RADiCAL timing scale ladder — realistic composition + electronics (%d evt/point)\n", nevt);
  P("================================================================\n\n");
  P("%-6s %10s %8s %14s %9s %11s %12s\n",
    "f_nom","<PhWLS>","f_meas","a [ps/sqrtE]","b [ps]","capture*PDE","Nfired/E sag");

  double light[NF], a[NF], ae[NF], b[NF];
  TGraphErrors* gST[NF];
  std::vector<double> X, Y, YE;

  for (int i = 0; i < NF; ++i) {
    TString dir = Form("scan/optical_scan_%d_paperR_lad%g", nevt, fnom[i]);
    // light, capture, energy-linearity sag from the per-energy files
    TFile* fE = TFile::Open(Form("%s/optical_E%dGeV.root", dir.Data(), EREF));
    TH1* wl = (TH1*)fE->Get("PhotonsWLS");
    TH1* we = (TH1*)fE->Get("PhotonsWLSEmitted");
    light[i]     = wl->GetMean();
    double cap   = (we && we->GetMean() > 0) ? wl->GetMean() / we->GetMean() : -1;
    fE->Close();
    // energy nonlinearity: (Nfired/E at 25) vs (at 150); >1 means saturating
    auto nfE = [&](int E){ TFile* f = TFile::Open(Form("%s/optical_E%dGeV.root", dir.Data(), E));
                           TH1* h = (TH1*)f->Get("EnergyLowGain"); double v = h->GetMean()/E; f->Close(); return v; };
    double sag = nfE(25) / nfE(150);
    // stored headline fit
    TFile* fC = TFile::Open(Form("%s/resolution_curves.root", dir.Data()));
    TGraphErrors* g = (TGraphErrors*)fC->Get("TimingResolution_4cAllDRS4");
    TF1* ft = (TF1*)g->GetListOfFunctions()->First();
    a[i] = ft->GetParameter(0); ae[i] = ft->GetParError(0); b[i] = std::fabs(ft->GetParameter(1));
    gST[i] = (TGraphErrors*)g->Clone(Form("gST%d", i));
    fC->Close();
    double fm = 0;  // filled after normalization below
    P("%-6g %10.1f %8s %9.1f+-%4.1f %8.2f %10.2f%% %12.2f\n",
      fnom[i], light[i], "-", a[i], ae[i], b[i], 100*cap, sag);
  }

  int i1 = 2;                                    // f=1
  P("\n(f_meas = <PhWLS>/<PhWLS(f=1)>; capture*PDE ~11%% vs ~3%% physical => over-collection;\n");
  P(" Nfired/E sag > 1 => SiPM saturating with energy, UNLIKE the paper's linear Fig 17)\n\n");

  for (int i = 0; i < NF; ++i) {
    double fm = light[i] / light[i1];
    X.push_back(1./fm); Y.push_back(a[i]*a[i]); YE.push_back(2*a[i]*ae[i]);
  }
  TGraphErrors* gr = new TGraphErrors(NF, X.data(), Y.data(), nullptr, YE.data());
  TF1* lin = new TF1("lin", "[0]*x+[1]", 0, 12); gr->Fit(lin, "Q0");
  double A = std::sqrt(std::max(0., lin->GetParameter(0)));
  double B2 = lin->GetParameter(1), B = std::sqrt(std::fabs(B2));
  double fTrue = 1./baseScale, aTrue = std::sqrt(A*A/fTrue + std::max(0., B2));

  P("a^2(f) = A^2/f + B^2 :  A = %.0f  B = %s%.0f  ps/sqrt(E)   chi2/ndf = %.1f/%d\n",
    A, (B2<0?"(imag) ":""), B, lin->GetChisquare(), lin->GetNDF());
  P("EXTRAPOLATION to true light (f=%.0f):  a = %.0f ps/sqrt(E)\n", fTrue, aTrue);
  P("PAPER:                                 a = 256 ps/sqrt(E) (+) 17.5 ps\n");
  P("=> sim ~%.1fx the paper's stochastic term; fit chi2/ndf=%.1f (poor) and B\n",
    aTrue/256., lin->GetChisquare()/std::max(1,lin->GetNDF()));
  P("   dominates => the floor, not photon counting, sets the number.\n");

  // ---- plot: left = sigma_t(E) per f; right = a^2 vs 1/f ----
  gStyle->SetOptStat(0);
  TCanvas* c = new TCanvas("c","ladder",1200,500); c->Divide(2,1);
  c->cd(1); gPad->SetGrid();
  int col[NF] = {kRed+1, kOrange+7, kAzure+2, kGreen+2};
  TMultiGraph* mg = new TMultiGraph();
  for (int i = 0; i < NF; ++i) { gST[i]->SetMarkerStyle(20); gST[i]->SetMarkerColor(col[i]);
    gST[i]->SetLineColor(col[i]); gST[i]->SetMarkerSize(1.2); mg->Add(gST[i],"P"); }
  mg->SetTitle("All-light + electronics #sigma_{t} vs E;E_{beam} (GeV);#sigma_{t} (ps)");
  mg->Draw("A"); mg->GetXaxis()->SetLimits(0,160);
  TLegend* lg = new TLegend(0.55,0.68,0.88,0.88);
  for (int i = 0; i < NF; ++i) lg->AddEntry(gST[i], Form("f = %g (x%g light)", fnom[i], fnom[i]), "p");
  lg->Draw();
  c->cd(2); gPad->SetGrid();
  gr->SetTitle("Photostatistics decomposition;1/f  (inverse light);a^{2}  (ps^{2}/E)");
  gr->SetMarkerStyle(21); gr->SetMarkerSize(1.3); gr->Draw("AP");
  lin->SetLineColor(kBlack); lin->SetRange(0, 11); lin->Draw("same");
  TLatex tx; tx.SetNDC(); tx.SetTextSize(0.038);
  tx.DrawLatex(0.30,0.83, Form("A = %.0f  (photostat)", A));
  tx.DrawLatex(0.30,0.77, Form("B = %.0f  (floor)", B));
  tx.DrawLatex(0.30,0.71, Form("#chi^{2}/ndf = %.1f/%d", lin->GetChisquare(), lin->GetNDF()));
  tx.DrawLatex(0.30,0.63, Form("true-light a #approx %.0f", aTrue));
  tx.DrawLatex(0.30,0.57, "paper a = 256");
  c->SaveAs("results/ladder.png");
  P("\nwrote results/ladder_summary.txt and results/ladder.png\n");
  fclose(out);
}
