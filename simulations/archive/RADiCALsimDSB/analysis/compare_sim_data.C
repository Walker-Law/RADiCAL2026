// compare_sim_data.C — overlay the sim's DATA-MATCHED estimators against the
// CERN May-2023 test-beam result (RADiCAL/ repo) and the RADiCAL paper.
//
//   root -l -b -q 'analysis/compare_sim_data.C("build/scan")'
//   root -l -b -q 'analysis/compare_sim_data.C("build/scan/optical_scan_20000")'
//
// Requires an OPTICAL scan produced by a binary built after the H1[31-34]
// estimators were added (EventAction.cc / RunAction.cc). Reads per-energy files
// <dir>/<prefix>_E<E>GeV.root and the data-matched histograms:
//   H1[34] DeltaT_CFD_4c_Scint_DRS4 -> sim sigma_t  (scint + datasheet DRS4)   [HEADLINE]
//   H1[32] DeltaT_CFD_4c_Scint      -> sim sigma_t  (scint, ideal DRS4)        [light floor]
//   H1[31] DeltaT_CFD_4c            -> sim sigma_t  (all light, ideal DRS4)    [reference]
//   H1[33] Npe_Scint_veto           -> sim sigma/E  (fiber light, sum_lg analog)
// All timing uses the (DW-UP)/2 corner trick built into the estimator: sigma_t = sigma(dT)/2.
//
// Reference curves are hardcoded (like the paper curve in scan_resolution.C):
//   DATA  RADiCAL/ repo, MCP-free Method A, 5% CFD (PLAN.md executive summary,
//         analyzeResolution.C): sigma_t = 181/sqrt(E) (+) 34.9 ps  (~37 ps @150).
//         sigma/E ~ 11-19% across 25-150 GeV (fiber-light sum_lg).
//   PAPER arXiv:2401.01747: sigma_t = 256/sqrt(E) (+) 17.5 ps.
// Edit kDataA/kDataB below if the data pipeline is re-run with updated numbers.

static const double kDataA = 181.0, kDataB = 34.9;   // data sigma_t fit (ps)
static const double kPapA  = 256.0, kPapB  = 17.5;   // paper sigma_t fit (ps)
static const double kDataEresLo = 11.0, kDataEresHi = 19.0;  // data sigma/E band (%)

// Iterative Gaussian core fit (identical convention to scan_resolution.C and the
// test-beam analyzeResolution.C: +/- nsig core, refit iters times).
TF1* coreFit(TH1* h, double nsig = 2.0, int iters = 4) {
  double mu = h->GetMean(), sg = h->GetRMS();
  TF1* g = new TF1(Form("cf_%s_%p", h->GetName(), (void*)h), "gaus",
                   mu - nsig*sg, mu + nsig*sg);
  for (int i = 0; i < iters; i++) {
    g->SetRange(mu - nsig*sg, mu + nsig*sg);
    h->Fit(g, "RQL0");
    mu = g->GetParameter(1); sg = g->GetParameter(2);
    if (sg <= 0) break;
  }
  return g;
}

// helper: core-fit a timing hist, return sigma_t (ps) = sigma(dT[ns])*1000/2
static bool sigmaT(TFile* f, const char* hn, double& st, double& ste) {
  TH1* h = (TH1*)f->Get(hn);
  if (!h || h->GetEntries() < 50) return false;
  TF1* g = coreFit(h, 2.5, 4);
  st  = g->GetParameter(2) * 500;   // *1000 ns->ps, /2 corner trick
  ste = g->GetParError(2)  * 500;
  return st > 0;
}

void compare_sim_data(const char* dir = "build/scan", const char* prefix = "optical") {
  gStyle->SetOptStat(0); gStyle->SetOptFit(0);
  gStyle->SetPadGridX(1); gStyle->SetPadGridY(1);
  const int N = 8; double E[N] = {5, 10, 20, 25, 50, 100, 120, 150};
  const char* out = "build/plots";

  double Eh[N], tH[N], tHe[N]; int nH = 0;   // ALL light + DRS4 (faithful headline; needs realistic yield)
  double Ed[N], tD[N], tDe[N]; int nD = 0;   // scint + datasheet DRS4 (WLS-regime proxy)
  double Es[N], tS[N], tSe[N]; int nS = 0;   // scint, ideal DRS4 (light floor)
  double Ea[N], tA[N], tAe[N]; int nA = 0;   // all light, ideal DRS4 (reference)
  double Ee[N], eR[N], eRe[N]; int nE = 0;   // fiber-light sigma/E

  printf("\n  E(GeV)  all+DRS4(ps)  scint+DRS4(ps)  scint(ps)  all(ps)   sigma/E light(%%)\n");
  printf("  --------------------------------------------------------------------------\n");

  for (int i = 0; i < N; i++) {
    TFile* f = TFile::Open(Form("%s/%s_E%.0fGeV.root", dir, prefix, E[i]));
    if (!f || f->IsZombie()) { printf("  %5.0f   -- file not found --\n", E[i]); continue; }

    double sH=-1,sHe=0, sD=-1,sDe=0, sS=-1,sSe=0, sA=-1,sAe=0;
    if (sigmaT(f, "DeltaT_CFD_4c_DRS4",       sH, sHe)) { Eh[nH]=E[i]; tH[nH]=sH; tHe[nH]=sHe; nH++; }
    if (sigmaT(f, "DeltaT_CFD_4c_Scint_DRS4", sD, sDe)) { Ed[nD]=E[i]; tD[nD]=sD; tDe[nD]=sDe; nD++; }
    if (sigmaT(f, "DeltaT_CFD_4c_Scint",      sS, sSe)) { Es[nS]=E[i]; tS[nS]=sS; tSe[nS]=sSe; nS++; }
    if (sigmaT(f, "DeltaT_CFD_4c",            sA, sAe)) { Ea[nA]=E[i]; tA[nA]=sA; tAe[nA]=sAe; nA++; }

    double eres = -1;
    if (TH1* h = (TH1*)f->Get("Npe_Scint_veto")) {
      if (h->GetEntries() > 50) {
        TF1* pre = coreFit(h, 2.0, 3); double s = pre->GetParameter(2);
        double bw = h->GetBinWidth(1); int rb = TMath::Max(1, (int)std::round((s/5.)/bw));
        int nb = h->GetNbinsX();
        while (rb > 1 && nb % rb != 0) --rb;   // snap to exact divisor (no Rebin warning)
        if (rb > 1) h->Rebin(rb); delete pre;
        TF1* g = coreFit(h, 2.0, 4);
        double mu = g->GetParameter(1), sg = g->GetParameter(2);
        if (mu > 0) { eres = 100*sg/mu; Ee[nE]=E[i]; eR[nE]=eres; eRe[nE]=100*g->GetParError(2)/mu; nE++; }
      }
    }
    printf("  %5.0f    %8s      %8s      %8s %8s        %8s\n", E[i],
           sH>0?Form("%.1f",sH):"--", sD>0?Form("%.1f",sD):"--",
           sS>0?Form("%.1f",sS):"--", sA>0?Form("%.1f",sA):"--",
           eres>0?Form("%.2f",eres):"--");
    f->Close();
  }

  if (nH == 0 && nD == 0 && nS == 0 && nA == 0) {
    printf("\n  No data-matched histograms found in %s.\n"
           "  Run an OPTICAL scan with a binary built after the H1[31-35] change:\n"
           "    RADICAL_OPTICAL=1 bash run_scan.sh <NEVT> 1\n\n", dir);
    return;
  }

  // ================= TIMING: sim vs data vs paper =================
  TCanvas* c1 = new TCanvas("c1", "timing", 850, 650);
  double z[N] = {0};
  TMultiGraph* mg = new TMultiGraph();

  TGraphErrors *gH=nullptr, *gD=nullptr, *gS=nullptr, *gA=nullptr;
  if (nH) { gH = new TGraphErrors(nH, Eh, tH, z, tHe); gH->SetName("sim_all_drs4");
    gH->SetMarkerStyle(20); gH->SetMarkerSize(1.5); gH->SetMarkerColor(kRed+1); gH->SetLineColor(kRed+1); mg->Add(gH,"P"); }
  if (nD) { gD = new TGraphErrors(nD, Ed, tD, z, tDe); gD->SetName("sim_scint_drs4");
    gD->SetMarkerStyle(22); gD->SetMarkerSize(1.4); gD->SetMarkerColor(kMagenta+1); gD->SetLineColor(kMagenta+1); mg->Add(gD,"P"); }
  if (nS) { gS = new TGraphErrors(nS, Es, tS, z, tSe); gS->SetName("sim_scint");
    gS->SetMarkerStyle(24); gS->SetMarkerSize(1.3); gS->SetMarkerColor(kAzure+2); gS->SetLineColor(kAzure+2); mg->Add(gS,"P"); }
  if (nA) { gA = new TGraphErrors(nA, Ea, tA, z, tAe); gA->SetName("sim_all");
    gA->SetMarkerStyle(25); gA->SetMarkerSize(1.2); gA->SetMarkerColor(kGreen+2); gA->SetLineColor(kGreen+2); mg->Add(gA,"P"); }

  mg->SetTitle("Timing resolution: sim (data-matched) vs test beam;E_{beam} (GeV);#sigma_{t} (ps)");
  mg->Draw("A"); mg->SetMinimum(0.);
  // Fixed y-range: unstable all-light core fits at low E (Cherenkov-broadened,
  // non-Gaussian at scaled yield) otherwise stretch the axis and squash the
  // 20-40 ps region where the actual comparison lives.
  mg->SetMaximum(120.);

  TF1* fData = new TF1("fData","sqrt([0]*[0]/x+[1]*[1])",4,160);
  fData->SetParameters(kDataA,kDataB); fData->SetLineColor(kBlack); fData->SetLineWidth(2); fData->Draw("same");
  TF1* fPap = new TF1("fPap","sqrt([0]*[0]/x+[1]*[1])",4,160);
  fPap->SetParameters(kPapA,kPapB); fPap->SetLineColor(kGray+2); fPap->SetLineStyle(2); fPap->SetLineWidth(2); fPap->Draw("same");

  TF1 *fH=nullptr, *fD=nullptr, *fS=nullptr;
  if (nH >= 3) { fH = new TF1("fH","sqrt([0]*[0]/x+[1]*[1])",4,160);
    fH->SetParameters(60,30); fH->SetLineColor(kRed+1); fH->SetLineStyle(1); gH->Fit(fH,"RQ"); fH->Draw("same"); }
  if (nD >= 3) { fD = new TF1("fD","sqrt([0]*[0]/x+[1]*[1])",4,160);
    fD->SetParameters(60,30); fD->SetLineColor(kMagenta+1); fD->SetLineStyle(7); gD->Fit(fD,"RQ"); fD->Draw("same"); }
  if (nS >= 3) { fS = new TF1("fS","sqrt([0]*[0]/x+[1]*[1])",4,160);
    fS->SetParameters(60,15); fS->SetLineColor(kAzure+2); fS->SetLineStyle(3); gS->Fit(fS,"RQ"); fS->Draw("same"); }

  TLegend* L = new TLegend(0.36,0.58,0.88,0.88); L->SetFillStyle(0); L->SetBorderSize(0);
  if (gH) L->AddEntry(gH,"sim: ALL light + DRS4 (faithful; needs realistic yield)","p");
  if (gD) L->AddEntry(gD,"sim: scint + DRS4 (WLS-regime proxy)","p");
  if (gS) L->AddEntry(gS,"sim: scint, ideal DRS4 (light floor)","p");
  if (gA) L->AddEntry(gA,"sim: all light, ideal DRS4 (ref)","p");
  L->AddEntry(fData,Form("DATA (test beam): %.0f/#sqrt{E} #oplus %.1f ps",kDataA,kDataB),"l");
  L->AddEntry(fPap, Form("paper 2401.01747: %.0f/#sqrt{E} #oplus %.1f ps",kPapA,kPapB),"l");
  L->Draw();
  c1->SaveAs(Form("%s/sim_vs_data_timing.png", out));

  // ================= ENERGY: sim vs data band =================
  if (nE >= 2) {
    TCanvas* c2 = new TCanvas("c2","energy",850,650);
    TGraphErrors* gE = new TGraphErrors(nE, Ee, eR, z, eRe); gE->SetName("sim_eres");
    gE->SetMarkerStyle(21); gE->SetMarkerSize(1.4); gE->SetMarkerColor(kRed+1); gE->SetLineColor(kRed+1);
    gE->SetTitle("Fiber-light energy resolution: sim vs test beam;E_{beam} (GeV);#sigma/E (%)");
    gE->SetMinimum(0.);   // keep the 11-19% DATA band on-canvas (auto-range hid it)
    gE->Draw("AP");
    TBox* band = new TBox(4, kDataEresLo, 160, kDataEresHi);
    band->SetFillColorAlpha(kGray,0.35); band->SetLineColor(kGray+1); band->Draw("same");
    gE->Draw("P same");
    TLegend* L2 = new TLegend(0.42,0.72,0.88,0.88); L2->SetFillStyle(0); L2->SetBorderSize(0);
    L2->AddEntry(gE,"sim: fiber light (scint N_{p.e.}), veto","p");
    L2->AddEntry(band,Form("DATA sum_{lg}: %.0f-%.0f%%",kDataEresLo,kDataEresHi),"f");
    L2->Draw();
    TLatex tc; tc.SetNDC(); tc.SetTextSize(0.028); tc.SetTextColor(kGray+2);
    tc.DrawLatex(0.13,0.02,"sim N_{p.e.} carries LYSO-yield scaling: trust trend, not absolute floor");
    c2->SaveAs(Form("%s/sim_vs_data_energy.png", out));
  }

  // ================= summary =================
  printf("\n  ---- fits (sigma_t = a/sqrt(E) (+) b) ----\n");
  if (fH) printf("  sim ALL light + DRS4:        %.1f/sqrt(E) (+) %.1f ps   <- faithful headline (at realistic yield)\n",
                 fabs(fH->GetParameter(0)), fabs(fH->GetParameter(1)));
  if (fD) printf("  sim scint + datasheet DRS4:  %.1f/sqrt(E) (+) %.1f ps   (WLS-regime proxy)\n",
                 fabs(fD->GetParameter(0)), fabs(fD->GetParameter(1)));
  if (fS) printf("  sim scint, ideal DRS4:       %.1f/sqrt(E) (+) %.1f ps   (light floor)\n",
                 fabs(fS->GetParameter(0)), fabs(fS->GetParameter(1)));
  printf("  DATA (test beam):            %.0f/sqrt(E) (+) %.1f ps   (~37 ps @150)\n", kDataA, kDataB);
  printf("  paper (2401.01747):          %.0f/sqrt(E) (+) %.1f ps\n", kPapA, kPapB);
  printf("  Saved: %s/sim_vs_data_timing.png%s\n\n", out, (nE>=2?"  +  sim_vs_data_energy.png":""));
}
