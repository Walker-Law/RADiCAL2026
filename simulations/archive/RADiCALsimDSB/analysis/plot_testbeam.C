// plot_testbeam.C — produce CERN test-beam style canvases from one run file.
//
//   root -l -b -q 'analysis/plot_testbeam.C("build/radical_output.root", 120)'
//
// Generates (into build/plots/):
//   energy_resolution_E<NN>.png   — sampled-energy spectrum + iterative core Gaussian fit (sigma/E)
//   timing_resolution_E<NN>.png   — downstream-minus-upstream DeltaT + Gaussian fit (sigma_t)
//   shower_long_E<NN>.png         — longitudinal shower profile
//   shower_lat_E<NN>.png          — lateral X-Y shower profile
//
// Energy estimator = TotalLYSO (LYSO sampling signal). sigma/E is calibration-
// independent, so sampled GeV is shown directly.

// Iterative Gaussian "core" fit: refit within +/- nsig*sigma a few times so
// non-Gaussian tails (leakage) don't bias the resolution — standard TB practice.
TF1* coreFit(TH1* h, double nsig=2.0, int iters=4, Color_t col=kRed) {
  double mu = h->GetMean(), sg = h->GetRMS();
  TF1* g = new TF1(Form("core_%s",h->GetName()), "gaus", mu-nsig*sg, mu+nsig*sg);
  for (int i=0;i<iters;i++) {
    g->SetRange(mu-nsig*sg, mu+nsig*sg);
    h->Fit(g, "RQL0");
    mu = g->GetParameter(1); sg = g->GetParameter(2);
    if (sg<=0) break;
  }
  g->SetLineColor(col); g->SetLineWidth(2); g->SetNpx(500);
  return g;
}

void plot_testbeam(const char* file="build/radical_output.root", double Ebeam=120) {
  gStyle->SetOptStat(0); gStyle->SetOptFit(0);
  gStyle->SetPadGridX(1); gStyle->SetPadGridY(1);
  TFile* f = TFile::Open(file);
  if (!f || f->IsZombie()) { printf("cannot open %s\n", file); return; }
  TString tag = Form("E%.0f", Ebeam);
  const char* out = "build/plots";

  // ---------- 1. ENERGY RESOLUTION ----------
  // Estimator = tail-catcher-corrected energy (E_LYSO + f_s*E_PbGlass).
  TH1D* hE = (TH1D*)f->Get("ECombined");
  // Adaptive rebin: target bin width ~ coreSigma/5 so the peak is well sampled
  // at any beam energy (the native 5 MeV bins wash out the high-E peak).
  { TF1* pre = coreFit(hE, 2.0, 3); double s = pre->GetParameter(2);
    double bw = hE->GetBinWidth(1); int rb = TMath::Max(1, (int)std::round((s/5.0)/bw));
    if (rb>1) hE->Rebin(rb); delete pre; }
  hE->GetXaxis()->SetRangeUser(0, hE->GetMean()+6*hE->GetRMS());
  TCanvas* c1 = new TCanvas("c1","energy",800,600);
  hE->SetLineColor(kBlack); hE->SetLineWidth(2);
  hE->SetTitle(Form("Energy response (LYSO + Pb-glass), %.0f GeV e^{-};E_{reco} (GeV);Events", Ebeam));
  hE->Draw("hist");
  TF1* gE = coreFit(hE, 2.0, 4, kRed); gE->Draw("same");
  double muE=gE->GetParameter(1), sgE=gE->GetParameter(2), resE=100*sgE/muE;
  TLatex tl; tl.SetNDC(); tl.SetTextSize(0.040);
  tl.DrawLatex(0.58,0.85, Form("#mu = %.3f GeV", muE));
  tl.DrawLatex(0.58,0.79, Form("#sigma = %.3f GeV", sgE));
  tl.DrawLatex(0.58,0.73, Form("#sigma/E = %.2f%%", resE));
  tl.DrawLatex(0.58,0.67, Form("entries = %.0f", hE->GetEntries()));
  c1->SaveAs(Form("%s/energy_resolution_%s.png", out, tag.Data()));

  // ---------- 2. TIMING RESOLUTION ----------
  TH1D* hT = (TH1D*)f->Get("DeltaT");
  hT->GetXaxis()->SetRangeUser(hT->GetMean()-8*hT->GetRMS(), hT->GetMean()+8*hT->GetRMS());
  TCanvas* c2 = new TCanvas("c2","timing",800,600);
  hT->SetLineColor(kBlack); hT->SetLineWidth(2);
  hT->SetTitle(Form("Timing response (downstream #minus upstream), %.0f GeV;#DeltaT (ns);Hits", Ebeam));
  hT->Draw("hist");
  TF1* gT = coreFit(hT, 2.5, 4, kBlue+1); gT->Draw("same");
  double muT=gT->GetParameter(1), sgT=gT->GetParameter(2);
  tl.DrawLatex(0.58,0.85, Form("#mu = %.1f ps", muT*1000));
  tl.DrawLatex(0.58,0.79, Form("#sigma_{t} = %.2f ps", sgT*1000));
  tl.DrawLatex(0.58,0.73, Form("hits = %.0f", hT->GetEntries()));
  c2->SaveAs(Form("%s/timing_resolution_%s.png", out, tag.Data()));

  // ---------- 3. LONGITUDINAL SHOWER PROFILE ----------
  TH1D* hL = (TH1D*)f->Get("ShowerProfile");
  TCanvas* c3 = new TCanvas("c3","long",800,600);
  hL->Scale(1.0/hE->GetEntries());   // mean energy per layer per event
  hL->SetLineColor(kAzure+2); hL->SetLineWidth(2); hL->SetFillColorAlpha(kAzure+2,0.25);
  hL->SetTitle(Form("Longitudinal shower profile, %.0f GeV;LYSO layer;<E> per layer (MeV/event)", Ebeam));
  hL->Draw("hist");
  c3->SaveAs(Form("%s/shower_long_%s.png", out, tag.Data()));

  // ---------- 4. LATERAL SHOWER PROFILE ----------
  TH2D* hXY = (TH2D*)f->Get("LateralProfile");
  TCanvas* c4 = new TCanvas("c4","lat",750,650);
  c4->SetRightMargin(0.15);
  hXY->SetTitle(Form("Lateral shower profile, %.0f GeV;x (mm);y (mm)", Ebeam));
  hXY->Draw("colz");
  c4->SaveAs(Form("%s/shower_lat_%s.png", out, tag.Data()));

  printf("\n=== %.0f GeV summary ===\n", Ebeam);
  printf("Energy:  mu=%.3f GeV  sigma=%.3f GeV  sigma/E=%.2f%%\n", muE, sgE, resE);
  printf("Timing:  mu=%.1f ps   sigma_t=%.2f ps\n", muT*1000, sgT*1000);
  printf("Saved 4 PNGs to %s/\n", out);
  f->Close();
}
