// scan_resolution.C — build CERN test-beam resolution-vs-energy curves from the
// per-energy scan files in build/scan/.
//
//   root -l -b -q analysis/scan_resolution.C
//
// Produces (build/plots/):
//   energy_resolution_curve.png  — sigma/E vs E, fit  sqrt(a^2/E + b^2)
//   timing_resolution_curve.png  — sigma_t vs E,  fit sqrt(a^2/E + b^2)
//   shower_long_overlay.png      — longitudinal profiles, all energies
// and prints a summary table.

TF1* coreFit(TH1* h, double nsig=2.0, int iters=4) {
  double mu=h->GetMean(), sg=h->GetRMS();
  TF1* g=new TF1(Form("cf_%s_%p",h->GetName(),(void*)h),"gaus",mu-nsig*sg,mu+nsig*sg);
  for(int i=0;i<iters;i++){ g->SetRange(mu-nsig*sg,mu+nsig*sg); h->Fit(g,"RQL0");
    mu=g->GetParameter(1); sg=g->GetParameter(2); if(sg<=0)break; }
  return g;
}

// Robust fractional resolution (sigma/mean, %) with error, for the ENERGY/PHOTON
// spectra. At low light these are strongly non-Gaussian (Poisson-like, skewed),
// and the iterative gaussian core-fit degenerates — that produced the 5 GeV
// point at 42 +/- 209%. When the gauss fit's relative error on sigma exceeds
// 25% (fit unreliable), fall back to the well-defined RMS/mean with the analytic
// large-N error sigma_rel/sqrt(2N). Returns res%, err% by reference.
void robustRes(TH1* h, double& res, double& err) {
  double N=h->GetEntries();
  double rmsRes = (h->GetMean()>0) ? 100.*h->GetRMS()/h->GetMean() : -1;
  double rmsErr = (N>1) ? rmsRes/std::sqrt(2.*N) : 1e9;
  TF1* g=coreFit(h,2.0,4);
  double mu=g->GetParameter(1), sg=g->GetParameter(2), se=g->GetParError(2);
  bool fitOK = (mu>0 && sg>0 && se>0 && se/sg < 0.25);
  if(fitOK){ res=100.*sg/mu; err=100.*se/mu; }
  else     { res=rmsRes;     err=rmsErr; }   // RMS fallback (honest, well-defined)
}

void scan_resolution(const char* dir="build/scan", const char* prefix="radical") {
  gStyle->SetOptStat(0); gStyle->SetOptFit(0);
  gStyle->SetPadGridX(1); gStyle->SetPadGridY(1);
  // PAPER-MATCHED ENERGY POINTS (arXiv:2401.01747): the beam test used six steps,
  // "25, 50, 75, 100, 125, and 150 GeV", and the quoted timing fit
  // sigma_t = 256 ps/sqrt(E) (+) 17.5 ps is valid over 25 <= E <= 150 GeV.
  // Running the sim on the SAME grid means our fit is over the same range as the
  // paper's rather than extrapolating from lower energies.
  const int N=6; double E[N]={25,50,75,100,125,150};
  const char* out="build/plots";

  double eResAll[N],eResErrAll[N], tResAll[N],tResErrAll[N], EAll[N];
  double tResS[N],tResSErr[N], ES[N]; int nGoodS=0;   // scint-only timing (if present)
  TGraphErrors* gTimingScint=nullptr;
  // PAPER-MATCHED timing estimator: H1[32] DeltaT_CFD_4c_Scint = per-event mean
  // over the 4 instrumented capillaries of the 5% CFD (downstream - upstream),
  // i.e. the paper's "difference of the average timing of the downstream SiPM
  // relative to the MCP and the average timing of the upstream SiPM relative to
  // the MCP". The per-corner histograms (H1[6]/[24]/[25]) lack that sqrt(4)
  // averaging and overstate sigma_t by ~2x, so THIS is the number to compare.
  double tRes4c[N],tRes4cErr[N], E4c[N]; int nGood4c=0;
  TGraphErrors* gTiming4c=nullptr;
  // Same estimator but ELECTRONICS-INCLUSIVE: H1[34] = H1[32] + the DRS4
  // uncalibrated-timebase residual (+ per-channel amplifier jitter when
  // RADICAL_ELEC_JITTER_PS was set at run time). The paper's 17.5 ps constant
  // term is a with-electronics number, so THIS curve is the apples-to-apples
  // headline; the noiseless H1[32] curve shows what the light alone supports.
  double tRes4d[N],tRes4dErr[N], E4d[N]; int nGood4d=0;
  double tRes4a[N],tRes4aErr[N], E4a[N]; int nGood4a=0;   // ALL-light DRS4 (H1: DeltaT_CFD_4c_DRS4)
  TGraphErrors* gTiming4d=nullptr;
  double tResW[N],tResWErr[N], EW[N]; int nGoodW=0;   // WLS-only timing (if present)
  TGraphErrors* gTimingWls=nullptr;
  double tResHG[N],tResHGErr[N], EHG[N]; int nGoodHG=0;  // high-gain timing (dual-gain)
  TGraphErrors* gTimingHG=nullptr;
  double tResHGE[N],tResHGEErr[N], EHGE[N]; int nGoodHGE=0;  // energy-binned high-gain
  TGraphErrors* gTimingHGE=nullptr;
  double eLG[N],eLGmean[N]; int nGoodLG=0;               // low-gain energy res + mean
  double smRes[N],smResErr[N], ESM[N]; int nGoodSM=0; // shower-max slice res (dE/dx, if present)
  double peRes[N],peResErr[N], EPE[N]; int nGoodPE=0; // shower-max res (photon-COUNT based, if present)
  TCanvas* cL=new TCanvas("cL","long",800,600);
  TLegend* leg=new TLegend(0.62,0.55,0.88,0.88);
  int cols[N]={kRed+1,kOrange+1,kSpring+4,kAzure+2,kBlue+1,kMagenta+1};
  // Per-energy histogram + Gaussian-core-fit overlay, saved to disk so the fit
  // behind each resolution-curve point is inspectable after the fact (not just
  // the aggregate curve). Answers "do you have those histograms" with a file.
  gSystem->mkdir(Form("%s/fits", out), true);
  TCanvas* cFit=new TCanvas("cFit","fit",800,600);
  // Same idea for timing: save the histogram + Gaussian-core-fit overlay behind
  // each timing-curve point. Histogram/fit are native ns; displayed numbers
  // apply the (DW-UP)/2 corner trick (ns->ps, sigma/2) to match the summary table.
  auto saveTimingFit = [&](TH1* h, TF1* g, const char* tag, double Ei){
    double muPs=g->GetParameter(1)*1000, sgPs=g->GetParameter(2)*500;
    double muN=g->GetParameter(1), sgN=g->GetParameter(2);
    cFit->cd(); cFit->Clear();
    h->GetXaxis()->SetRangeUser(muN-6*sgN, muN+6*sgN);
    h->SetLineColor(kAzure+2); h->SetLineWidth(2);
    h->SetTitle(Form("%s, E_{beam}=%.0f GeV;#Delta T (ns);Corners", tag, Ei));
    h->Draw("hist");
    g->SetLineColor(kRed+1); g->SetLineWidth(2); g->Draw("same");
    TLatex tl; tl.SetNDC(); tl.SetTextSize(0.035);
    tl.DrawLatex(0.15,0.85, Form("#DeltaT mean = %.1f ps", muPs));
    tl.DrawLatex(0.15,0.80, Form("#sigma_{t} = #sigma(#DeltaT)/2 = %.1f ps", sgPs));
    cFit->SaveAs(Form("%s/fits/timing_fit_%s_E%.0fGeV.png", out, tag, Ei));
  };

  printf("\n  E(GeV)   mu_E(GeV)  sigma_E   sigma/E(%%)   DeltaT(ps)  sigma_t(ps)\n");
  printf("  -------------------------------------------------------------------\n");
  int nGood=0;
  for(int i=0;i<N;i++){
    TFile* f=TFile::Open(Form("%s/%s_E%.0fGeV.root",dir,prefix,E[i]));
    if(!f || f->IsZombie()){
      printf("  %5.0f    -- skipped, file not found --\n",E[i]);
      continue;
    }
    // --- energy ---
    TH1D* hE=(TH1D*)f->Get("ECombined");
    { TF1* pre=coreFit(hE,2.0,3); double s=pre->GetParameter(2);
      double bw=hE->GetBinWidth(1); int rb=TMath::Max(1,(int)std::round((s/5.)/bw));
      if(rb>1)hE->Rebin(rb); delete pre; }
    TF1* gE=coreFit(hE,2.0,4);
    double muE=gE->GetParameter(1), sgE=gE->GetParameter(2), sgEerr=gE->GetParError(2);
    double eResI=100*sgE/muE, eResErrI=100*sgEerr/muE;
    // Save the histogram + fit overlay for THIS energy point (the actual object
    // the sigma/E numerator comes from), zoomed to the fit window.
    { cFit->cd(); cFit->Clear();
      hE->GetXaxis()->SetRangeUser(muE-6*sgE, muE+6*sgE);
      hE->SetLineColor(kAzure+2); hE->SetLineWidth(2);
      hE->SetTitle(Form("ECombined, E_{beam}=%.0f GeV;E_{reco} (GeV);Events", E[i]));
      hE->Draw("hist");
      gE->SetLineColor(kRed+1); gE->SetLineWidth(2); gE->Draw("same");
      TLatex tl; tl.SetNDC(); tl.SetTextSize(0.035);
      tl.DrawLatex(0.15,0.85, Form("#mu = %.3f GeV", muE));
      tl.DrawLatex(0.15,0.80, Form("#sigma = %.4f GeV  (%.2f%%)", sgE, eResI));
      cFit->SaveAs(Form("%s/fits/energy_fit_E%.0fGeV.png", out, E[i])); }
    // --- timing ---
    TH1D* hT=(TH1D*)f->Get("DeltaT");
    TF1* gT=coreFit(hT,2.5,4);
    saveTimingFit(hT, gT, "DeltaT", E[i]);
    // (DW−UP)/2 corner trick: σ_t = σ(ΔT)/2 (dividing by 2 gives physical timing resolution)
    double muT=gT->GetParameter(1)*1000, sgT=gT->GetParameter(2)*500, sgTerr=gT->GetParError(2)*500;
    // scint-only timing (new files only): same corner trick, Cherenkov excluded
    double sgTS=-1;
    // --- PAPER-MATCHED: 4-capillary-averaged, scint-only 5% CFD (DW - UP) ---
    // sigma_t = sigma(H1[32])/2 (the (DW-UP)/2 corner trick), ns -> ps.
    TH1D* h4c=(TH1D*)f->Get("DeltaT_CFD_4c_Scint");
    if(h4c && h4c->GetEntries()>50){
      TF1* g4c=coreFit(h4c,2.5,4);
      E4c[nGood4c]=E[i];
      tRes4c[nGood4c]=g4c->GetParameter(2)*500;        // ns->ps, and /2
      tRes4cErr[nGood4c]=g4c->GetParError(2)*500;
      saveTimingFit(h4c, g4c, "DeltaT_CFD_4c_Scint", E[i]);
      nGood4c++;
    }
    // Electronics-inclusive twin (DRS4 timebase + optional amplifier jitter)
    TH1D* h4d=(TH1D*)f->Get("DeltaT_CFD_4c_Scint_DRS4");
    if(h4d && h4d->GetEntries()>50){
      TF1* g4d=coreFit(h4d,2.5,4);
      E4d[nGood4d]=E[i];
      tRes4d[nGood4d]=g4d->GetParameter(2)*500;        // ns->ps, corner trick /2
      tRes4dErr[nGood4d]=g4d->GetParError(2)*500;
      saveTimingFit(h4d, g4d, "DeltaT_CFD_4c_Scint_DRS4", E[i]);
      nGood4d++;
    }
    // ALL-light electronics-inclusive twin: every photon the SiPM detects
    // (Cherenkov + self-scint + OpWLS, PDE-weighted) — no origin gating, as a
    // real SiPM cannot distinguish photon provenance. Realistic ONLY when the
    // light composition is realistic, i.e. all sources coherently thinned
    // (LYSO_SCINT_SCALE = SCINT_YIELD = QUARTZ_CHER_KEEP).
    TH1D* h4a=(TH1D*)f->Get("DeltaT_CFD_4c_DRS4");
    if(h4a && h4a->GetEntries()>50){
      TF1* g4a=coreFit(h4a,2.5,4);
      E4a[nGood4a]=E[i];
      tRes4a[nGood4a]=g4a->GetParameter(2)*500;        // ns->ps, corner trick /2
      tRes4aErr[nGood4a]=g4a->GetParError(2)*500;
      saveTimingFit(h4a, g4a, "DeltaT_CFD_4c_DRS4", E[i]);
      nGood4a++;
    }
    TH1D* hTS=(TH1D*)f->Get("DeltaT_Scint");
    if(hTS && hTS->GetEntries()>50){
      TF1* gTS=coreFit(hTS,2.5,4);
      saveTimingFit(hTS, gTS, "DeltaT_Scint", E[i]);
      sgTS=gTS->GetParameter(2)*500;
      ES[nGoodS]=E[i]; tResS[nGoodS]=sgTS; tResSErr[nGoodS]=gTS->GetParError(2)*500;
      nGoodS++;
    }
    // shower-max slice resolution — dE/dx version (pure calorimetric, NO photon
    // statistics; structurally cannot reproduce the paper's light-yield-limited
    // Fig 17 curve, kept for reference/diagnostic only).
    TH1D* hSM=(TH1D*)f->Get("EShowerMax");
    if(hSM && hSM->GetEntries()>50){
      TF1* gSM=coreFit(hSM,2.0,4);
      double muSM=gSM->GetParameter(1), sgSM=gSM->GetParameter(2);
      if(muSM>0){
        ESM[nGoodSM]=E[i]; smRes[nGoodSM]=100.*sgSM/muSM;
        smResErr[nGoodSM]=100.*gSM->GetParError(2)/muSM;
        nGoodSM++;
      }
    }
    // shower-max resolution — PHOTON-COUNT version (the real Fig 17 analog).
    // PhotonsWLS is inherently shower-max-localized: only LYSO light that
    // reaches the DSB1 fiber's fixed z-window gets WLS-shifted, so this carries
    // the same photon-counting noise that dominates the paper's real SiPM-sum
    // estimator. sigma/mean(N_pe) ~ sigma_E/E since N_pe is proportional to
    // collected light -> collected energy for a linear chain.
    TH1D* hPE=(TH1D*)f->Get("PhotonsWLS");
    if(hPE && hPE->GetEntries()>50){
      double rPE, ePE; robustRes(hPE, rPE, ePE);   // RMS-fallback for skewed spectra
      if(rPE>0){
        EPE[nGoodPE]=E[i]; peRes[nGoodPE]=rPE;
        peResErr[nGoodPE]=ePE;
        nGoodPE++;
      }
    }
    // WLS-only timing (realistic LYSO->DSB1 chain; needs LYSO-optical run)
    double sgTW=-1;
    TH1D* hTW=(TH1D*)f->Get("DeltaT_WLS");
    if(hTW && hTW->GetEntries()>50){
      TF1* gTW=coreFit(hTW,2.5,4);
      saveTimingFit(hTW, gTW, "DeltaT_WLS", E[i]);
      sgTW=gTW->GetParameter(2)*500;
      EW[nGoodW]=E[i]; tResW[nGoodW]=sgTW; tResWErr[nGoodW]=gTW->GetParError(2)*500;
      nGoodW++;
    }
    // Dual-gain: HIGH-GAIN timing (fixed-threshold leading edge; dual-gain runs)
    double sgTHG=-1;
    TH1D* hHG=(TH1D*)f->Get("DeltaT_HighGain");
    if(hHG && hHG->GetEntries()>50){
      TF1* gHG=coreFit(hHG,2.5,4);
      saveTimingFit(hHG, gHG, "DeltaT_HighGain", E[i]);
      sgTHG=gHG->GetParameter(2)*500;
      EHG[nGoodHG]=E[i]; tResHG[nGoodHG]=sgTHG; tResHGErr[nGoodHG]=gHG->GetParError(2)*500;
      nGoodHG++;
    }
    // Energy-BINNED high-gain timing — the paper's time-walk removal: select the
    // high-detected-energy events (top 40%, ~ "bins 6-8") where the fixed-
    // threshold pulse is large and its crossing time is amplitude-stable.
    double sgTHGE=-1;
    TH2D* h2hg=(TH2D*)f->Get("DeltaT_HG_vs_Elg");
    if(h2hg && h2hg->GetEntries()>100){
      TH1D* px=h2hg->ProjectionX(Form("pxE%d",i));
      double tot=px->Integral(), cum=0; int xcut=1;
      for(int b=1;b<=px->GetNbinsX();b++){ cum+=px->GetBinContent(b); if(cum>=0.60*tot){xcut=b;break;} }
      TH1D* py=h2hg->ProjectionY(Form("pyE%d",i), xcut, h2hg->GetNbinsX());
      if(py->GetEntries()>50){
        TF1* gE=coreFit(py,2.5,4);
        sgTHGE=gE->GetParameter(2)*500;   // (DW-UP)/2 corner trick, ns->ps
        EHGE[nGoodHGE]=E[i]; tResHGE[nGoodHGE]=sgTHGE;
        tResHGEErr[nGoodHGE]=gE->GetParError(2)*500; nGoodHGE++;
      }
    }
    // Dual-gain: LOW-GAIN energy resolution (SiPM fired-pixel count, saturated)
    double eresLG=-1;
    TH1D* hLG=(TH1D*)f->Get("EnergyLowGain");
    if(hLG && hLG->GetEntries()>50){
      double rLG, eLGe; robustRes(hLG, rLG, eLGe);   // RMS-fallback for skewed spectra
      if(rLG>0 && hLG->GetMean()>0){ eresLG=rLG;
        eLG[nGoodLG]=eresLG; eLGmean[nGoodLG]=hLG->GetMean(); nGoodLG++; }
    }
    printf("  %5.0f    %7.3f   %6.3f    %6.2f       %6.1f      %6.2f      %s%s%s%s%s\n",
           E[i],muE,sgE,eResI,muT,sgT,
           sgTS>0?Form("(scint: %.2f) ",sgTS):"",
           sgTW>0?Form("(WLS: %.2f) ",sgTW):"",
           sgTHG>0?Form("(HG-t: %.2f) ",sgTHG):"",
           sgTHGE>0?Form("(HG-Ebin: %.2f) ",sgTHGE):"",
           eresLG>0?Form("(LG-E: %.1f%%)",eresLG):"");
    // --- longitudinal profile overlay (normalized to unit area) ---
    TH1D* hL=(TH1D*)f->Get("ShowerProfile"); hL=(TH1D*)hL->Clone(Form("L%d",i));
    if(hL->Integral()>0) hL->Scale(1.0/hL->Integral());
    hL->SetLineColor(cols[i]); hL->SetLineWidth(2); hL->SetLineStyle(3);
    hL->SetTitle("Longitudinal shower profile vs energy;LYSO layer;normalized <E>");
    cL->cd(); hL->Draw(nGood==0?"hist":"hist same");
    hL->SetMaximum(0.12);
    leg->AddEntry(hL,Form("%.0f GeV",E[i]),"l");
    // Longo / gamma-distribution fit: dE/dt ∝ t^(α-1)·exp(−βt)
    // Bin centers: 0.5..28.5 (29 bins, [0,29]).  Peak at t_max = (α-1)/β ≈ [1]/[2].
    TF1* fL=new TF1(Form("fL%d",i),"[0]*TMath::Power(x,[1])*TMath::Exp(-[2]*x)",0.5,28.5);
    // Method-of-moments seed from the histogram's OWN mean/variance. For a gamma
    // distribution mean=α/β and var=α/β², so β=mean/var and α=mean²/var. A fixed
    // seed (old code) let 25 GeV converge to a spurious high-α/high-β minimum;
    // seeding from moments puts every energy on the same physical branch.
    double hMean=hL->GetMean(), hVar=hL->GetRMS()*hL->GetRMS();
    double b0=(hVar>1e-6)? hMean/hVar : 0.5;          // beta
    double a0=(hVar>1e-6)? hMean*hMean/hVar : 5.0;    // alpha
    if(a0<1.05) a0=1.05;                              // keep α-1>0 for the power law
    double tmax0=(a0-1.0)/b0;
    double peak0=TMath::Power(tmax0,a0-1.0)*TMath::Exp(-(a0-1.0));
    double norm0=(peak0>0)? hL->GetMaximum()/peak0 : hL->GetMaximum();
    fL->SetParameters(norm0, a0-1.0, b0);             // [1]=α-1, [2]=β
    fL->SetParLimits(0,1e-9,100.); fL->SetParLimits(1,0.1,20.); fL->SetParLimits(2,0.01,5.);
    hL->Fit(fL,"RQ0M");  // Q=quiet, 0=don't draw, M=improve (escape local minima)
    fL->SetLineColor(cols[i]); fL->SetLineStyle(1); fL->SetLineWidth(2);
    fL->Draw("same");
    printf("  Longo fit E=%.0f GeV: alpha=%.2f  beta=%.3f  t_max=%.1f layers\n",
           E[i], fL->GetParameter(1)+1, fL->GetParameter(2),
           fL->GetParameter(1)/fL->GetParameter(2));
    // record this energy as good, then advance the compacted index
    EAll[nGood]=E[i]; eResAll[nGood]=eResI; eResErrAll[nGood]=eResErrI;
    tResAll[nGood]=sgT; tResErrAll[nGood]=sgTerr;
    nGood++;
  }
  double zero[N]={0};
  // Dashed-line legend entry for the Longo fit curves
  TLine* lDash=new TLine(); lDash->SetLineStyle(1); lDash->SetLineWidth(2); lDash->SetLineColor(kGray+1);
  leg->AddEntry(lDash,"Longo fit: t^{#alpha-1}e^{-#beta t}","l");
  leg->Draw(); cL->SaveAs(Form("%s/shower_long_overlay.png",out));

  // ---------- energy resolution curve ----------
  TCanvas* c1=new TCanvas("c1","eres",800,600);
  TGraphErrors* gr=new TGraphErrors(nGood,EAll,eResAll,zero,eResErrAll);
  gr->SetName("EnergyResolution");
  gr->SetTitle("Energy resolution;E_{beam} (GeV);#sigma/E (%)");
  gr->SetMarkerStyle(20); gr->SetMarkerColor(kBlue+1); gr->SetLineColor(kBlue+1);
  gr->SetMarkerSize(1.3);
  TF1* fr=new TF1("fr","sqrt([0]*[0]/x+[1]*[1])",4,130);
  fr->SetParameters(10,1); fr->SetParNames("stoch(%)","const(%)");
  fr->SetLineColor(kRed); gr->Fit(fr,"RQ");
  gr->Draw("AP"); fr->Draw("same");
  TLatex t; t.SetNDC(); t.SetTextSize(0.040);
  t.DrawLatex(0.45,0.82,Form("#sigma/E = %.1f%%/#sqrt{E} #oplus %.2f%%",
              fabs(fr->GetParameter(0)),fabs(fr->GetParameter(1))));
  c1->SaveAs(Form("%s/energy_resolution_curve.png",out));

  // ---------- timing resolution curve ----------
  TCanvas* c2=new TCanvas("c2","tres",800,600);
  TGraphErrors* gt=new TGraphErrors(nGood,EAll,tResAll,zero,tResErrAll);
  gt->SetName("TimingResolution");
  gt->SetTitle("Timing resolution (downstream #minus upstream);E_{beam} (GeV);#sigma_{t} (ps)");
  gt->SetMarkerStyle(21); gt->SetMarkerColor(kGreen+2); gt->SetLineColor(kGreen+2);
  gt->SetMarkerSize(1.3);
  TF1* ft=new TF1("ft","sqrt([0]*[0]/x+[1]*[1])",4,130);
  ft->SetParameters(20,5); ft->SetParNames("stoch","const");
  ft->SetLineColor(kGreen+2); gt->Fit(ft,"RQ");
  // Scint-only graph + fit (new files only), prepared before drawing so the
  // axes can be ranged over BOTH graphs — otherwise the lower scint-only
  // points fall outside the all-photon auto-range and vanish from the plot.
  TF1* ftS=nullptr;
  if(nGoodS>=3){
    double zeroS[N]={0};
    TGraphErrors* gtS=new TGraphErrors(nGoodS,ES,tResS,zeroS,tResSErr);
    gtS->SetName("TimingResolutionScint");
    gtS->SetMarkerStyle(20); gtS->SetMarkerColor(kAzure+2); gtS->SetLineColor(kAzure+2);
    gtS->SetMarkerSize(1.3);
    ftS=new TF1("ftS","sqrt([0]*[0]/x+[1]*[1])",4,130);
    ftS->SetParameters(20,5); ftS->SetLineColor(kAzure+2); ftS->SetLineStyle(2);
    gtS->Fit(ftS,"RQ");
    gTimingScint=gtS;   // persisted alongside the other curves below
  }
  // PAPER-MATCHED graph + fit: 4-capillary-averaged scint-only CFD (H1[32]).
  // This is the curve to compare directly against 256 ps/sqrt(E) (+) 17.5 ps.
  TF1* ft4c=nullptr;
  if(nGood4c>=3){
    double zero4c[N]={0};
    TGraphErrors* gt4c=new TGraphErrors(nGood4c,E4c,tRes4c,zero4c,tRes4cErr);
    gt4c->SetName("TimingResolution_4cScint_paperMatched");
    gt4c->SetMarkerStyle(29); gt4c->SetMarkerColor(kRed+1); gt4c->SetLineColor(kRed+1);
    gt4c->SetMarkerSize(1.9);
    ft4c=new TF1("ft4c","sqrt([0]*[0]/x+[1]*[1])",20,160);
    ft4c->SetParameters(256,17.5); ft4c->SetLineColor(kRed+1); ft4c->SetLineWidth(2);
    gt4c->Fit(ft4c,"RQ");
    gTiming4c=gt4c;
  }
  // Electronics-inclusive paper-matched curve (H1[34]) — the true headline.
  TF1* ft4d=nullptr;
  if(nGood4d>=3){
    double zero4d[N]={0};
    TGraphErrors* gt4d=new TGraphErrors(nGood4d,E4d,tRes4d,zero4d,tRes4dErr);
    gt4d->SetName("TimingResolution_4cScintDRS4_paperMatched");
    gt4d->SetMarkerStyle(34); gt4d->SetMarkerColor(kBlack); gt4d->SetLineColor(kBlack);
    gt4d->SetMarkerSize(1.6);
    ft4d=new TF1("ft4d","sqrt([0]*[0]/x+[1]*[1])",20,160);
    ft4d->SetParameters(256,17.5); ft4d->SetLineColor(kBlack); ft4d->SetLineWidth(2);
    gt4d->Fit(ft4d,"RQ");
    gTiming4d=gt4d;
  }
  // ALL-light electronics-inclusive curve (H1: DeltaT_CFD_4c_DRS4) — the
  // headline once composition is coherent (see comment at the fill above).
  TF1* ft4a=nullptr; TGraphErrors* gTiming4a=nullptr;
  if(nGood4a>=3){
    double zero4a[N]={0};
    TGraphErrors* gt4a=new TGraphErrors(nGood4a,E4a,tRes4a,zero4a,tRes4aErr);
    gt4a->SetName("TimingResolution_4cAllDRS4");
    gt4a->SetMarkerStyle(21); gt4a->SetMarkerColor(kAzure+2); gt4a->SetLineColor(kAzure+2);
    gt4a->SetMarkerSize(1.3);
    ft4a=new TF1("ft4a","sqrt([0]*[0]/x+[1]*[1])",20,160);
    ft4a->SetParameters(256,17.5); ft4a->SetLineColor(kAzure+2); ft4a->SetLineWidth(2);
    gt4a->Fit(ft4a,"RQ");
    gTiming4a=gt4a;
  }
  // WLS-only graph + fit (LYSO-optical runs only)
  TF1* ftW=nullptr;
  if(nGoodW>=3){
    double zeroW[N]={0};
    TGraphErrors* gtW=new TGraphErrors(nGoodW,EW,tResW,zeroW,tResWErr);
    gtW->SetName("TimingResolutionWLS");
    gtW->SetMarkerStyle(22); gtW->SetMarkerColor(kMagenta+1); gtW->SetLineColor(kMagenta+1);
    gtW->SetMarkerSize(1.4);
    ftW=new TF1("ftW","sqrt([0]*[0]/x+[1]*[1])",4,130);
    ftW->SetParameters(100,15); ftW->SetLineColor(kMagenta+1); ftW->SetLineStyle(3);
    gtW->Fit(ftW,"RQ");
    gTimingWls=gtW;
  }
  // High-gain timing graph + fit (dual-gain runs only): fixed-threshold leading
  // edge should give a SHARPER timing reference than the 5% CFD "all light".
  TF1* ftHG=nullptr;
  if(nGoodHG>=3){
    double zeroHG[N]={0};
    TGraphErrors* gtHG=new TGraphErrors(nGoodHG,EHG,tResHG,zeroHG,tResHGErr);
    gtHG->SetName("TimingResolutionHighGain");
    gtHG->SetMarkerStyle(33); gtHG->SetMarkerColor(kOrange+7); gtHG->SetLineColor(kOrange+7);
    gtHG->SetMarkerSize(1.7);
    ftHG=new TF1("ftHG","sqrt([0]*[0]/x+[1]*[1])",4,130);
    ftHG->SetParameters(100,15); ftHG->SetLineColor(kOrange+7); ftHG->SetLineStyle(5);
    gtHG->Fit(ftHG,"RQ");
    gTimingHG=gtHG;
  }
  // Energy-binned high-gain graph + fit (paper's time-walk-corrected method)
  TF1* ftHGE=nullptr;
  if(nGoodHGE>=3){
    double zeroHGE[N]={0};
    TGraphErrors* gtHGE=new TGraphErrors(nGoodHGE,EHGE,tResHGE,zeroHGE,tResHGEErr);
    gtHGE->SetName("TimingResolutionHighGainEbinned");
    gtHGE->SetMarkerStyle(29); gtHGE->SetMarkerColor(kOrange+2); gtHGE->SetLineColor(kOrange+2);
    gtHGE->SetMarkerSize(1.8);
    // Fit from 15 GeV up: below that the E-binned estimator is still strongly
    // walk/statistics limited and the falling low-E points drag the 2-parameter
    // fit's constant term to an unphysical 0 (railed floor). The constant term
    // is defined by the high-E asymptote, which E>=15 GeV isolates cleanly.
    ftHGE=new TF1("ftHGE","sqrt([0]*[0]/x+[1]*[1])",15,130);
    ftHGE->SetParameters(100,15); ftHGE->SetParLimits(1,1.,100.);
    ftHGE->SetLineColor(kOrange+2); ftHGE->SetLineStyle(1);
    gtHGE->Fit(ftHGE,"RQ");
    gTimingHGE=gtHGE;
  }
  // Paper's measured fit (arXiv:2401.01747 abstract): sigma_t = 256/sqrt(E) (+) 17.5 ps
  TF1* fpaperT=new TF1("fpaperT","sqrt(256.*256./x+17.5*17.5)",4,160);
  fpaperT->SetLineColor(kGray+2); fpaperT->SetLineStyle(2); fpaperT->SetLineWidth(2);

  TMultiGraph* mgT=new TMultiGraph();
  mgT->Add(gt,"P");
  if(gTimingScint) mgT->Add(gTimingScint,"P");
  if(gTimingWls)   mgT->Add(gTimingWls,"P");
  if(gTimingHG)    mgT->Add(gTimingHG,"P");
  if(gTimingHGE)   mgT->Add(gTimingHGE,"P");
  if(gTiming4c)    mgT->Add(gTiming4c,"P");   // paper-matched estimator (light only)
  if(gTiming4d)    mgT->Add(gTiming4d,"P");   // paper-matched + electronics
  if(gTiming4a)    mgT->Add(gTiming4a,"P");   // ALL-light + electronics
  mgT->SetTitle("Timing resolution (downstream #minus upstream);E_{beam} (GeV);#sigma_{t} (ps)");
  mgT->Draw("A");
  mgT->SetMinimum(0.);
  ft->Draw("same");
  if(ftS) ftS->Draw("same");
  if(ftW) ftW->Draw("same");
  if(ftHG) ftHG->Draw("same");
  if(ft4c) ft4c->Draw("same");
  if(ft4d) ft4d->Draw("same");
  fpaperT->Draw("same");
  if(ft4d){   // TRUE headline: paper-matched estimator WITH electronics
    t.SetTextColor(kBlack);
    t.DrawLatex(0.30,0.965,Form("PAPER-MATCHED+electronics: %.1f ps/#sqrt{E} #oplus %.1f ps",
                fabs(ft4d->GetParameter(0)),fabs(ft4d->GetParameter(1))));
  }
  if(ft4c){   // light-only reference
    t.SetTextColor(kRed+1);
    t.DrawLatex(0.36,0.93,Form("light-only (4-cap mean): %.1f ps/#sqrt{E} #oplus %.1f ps",
                fabs(ft4c->GetParameter(0)),fabs(ft4c->GetParameter(1))));
    t.SetTextColor(kBlack);
  }
  t.SetTextColor(kGreen+2);
  t.DrawLatex(0.40,0.86,Form("all light: #sigma_{t} = %.1f ps/#sqrt{E} #oplus %.1f ps",
              fabs(ft->GetParameter(0)),fabs(ft->GetParameter(1))));
  t.SetTextColor(kBlack);
  if(ftS){
    t.SetTextColor(kAzure+2);
    t.DrawLatex(0.40,0.79,Form("scint-only (5%% CFD): %.1f ps/#sqrt{E} #oplus %.1f ps",
                fabs(ftS->GetParameter(0)),fabs(ftS->GetParameter(1))));
    t.SetTextColor(kBlack);
  }
  if(ftW){
    t.SetTextColor(kMagenta+1);
    t.DrawLatex(0.40,0.65,Form("WLS chain: %.1f ps/#sqrt{E} #oplus %.1f ps",
                fabs(ftW->GetParameter(0)),fabs(ftW->GetParameter(1))));
    t.SetTextColor(kBlack);
  }
  if(ftHG){
    t.SetTextColor(kOrange+7);
    t.DrawLatex(0.40,0.58,Form("high-gain (fixed thr): %.1f ps/#sqrt{E} #oplus %.1f ps",
                fabs(ftHG->GetParameter(0)),fabs(ftHG->GetParameter(1))));
    t.SetTextColor(kBlack);
  }
  if(ftHGE){
    t.SetTextColor(kOrange+2);
    t.DrawLatex(0.40,0.51,Form("high-gain (E-binned): %.1f ps/#sqrt{E} #oplus %.1f ps",
                fabs(ftHGE->GetParameter(0)),fabs(ftHGE->GetParameter(1))));
    t.SetTextColor(kBlack);
  }
  t.SetTextColor(kGray+2);
  t.DrawLatex(0.40,0.72,"paper (arXiv:2401.01747): 256 ps/#sqrt{E} #oplus 17.5 ps");
  t.SetTextColor(kBlack);
  c2->SaveAs(Form("%s/timing_resolution_curve.png",out));

  // ---------- shower-max resolution vs paper Fig 17 (right) ----------
  // Two estimators: dE/dx (no photon stats, diagnostic only) and photon-COUNT
  // (PhotonsWLS — the real apples-to-apples analog of the paper's SiPM-sum).
  TGraphErrors* gSM=nullptr;
  TGraphErrors* gPE=nullptr;
  TF1* fsm=nullptr;
  TF1* fpe=nullptr;
  if(nGoodSM>=3 || nGoodPE>=3){
    TCanvas* c3=new TCanvas("c3","smres",800,600);
    TMultiGraph* mgSM=new TMultiGraph();
    if(nGoodSM>=3){
      double zeroSM[N]={0};
      gSM=new TGraphErrors(nGoodSM,ESM,smRes,zeroSM,smResErr);
      gSM->SetName("ShowerMaxResolution_dEdx");
      gSM->SetMarkerStyle(20); gSM->SetMarkerColor(kBlue+1); gSM->SetLineColor(kBlue+1);
      gSM->SetMarkerSize(1.3);
      fsm=new TF1("fsm","sqrt([0]*[0]+[1]*[1]/x+[2]*[2]/(x*x))",4,130);
      fsm->SetParameters(9,50,30); fsm->SetLineColor(kBlue+1);
      gSM->Fit(fsm,"RQ");
      mgSM->Add(gSM,"P");
    }
    if(nGoodPE>=3){
      double zeroPE[N]={0};
      gPE=new TGraphErrors(nGoodPE,EPE,peRes,zeroPE,peResErr);
      gPE->SetName("ShowerMaxResolution_PhotonCount");
      gPE->SetMarkerStyle(21); gPE->SetMarkerColor(kMagenta+1); gPE->SetLineColor(kMagenta+1);
      gPE->SetMarkerSize(1.3);
      fpe=new TF1("fpe","sqrt([0]*[0]+[1]*[1]/x+[2]*[2]/(x*x))",4,130);
      fpe->SetParameters(9,50,30); fpe->SetLineColor(kMagenta+1); fpe->SetLineStyle(2);
      gPE->Fit(fpe,"RQ");
      mgSM->Add(gPE,"P");
    }
    mgSM->SetTitle("Shower-max energy resolution;E_{beam} (GeV);#sigma/mean (%)");
    mgSM->Draw("A");
    if(fsm) fsm->Draw("same");
    if(fpe) fpe->Draw("same");
    // paper Fig 17 fit for reference
    TF1* fpap=new TF1("fpap","sqrt(9.31*9.31+52.04*52.04/x+31.62*31.62/(x*x))",4,160);
    fpap->SetLineColor(kGray+2); fpap->SetLineStyle(3); fpap->Draw("same");
    TLatex tt; tt.SetNDC(); tt.SetTextSize(0.032);
    double yy=0.86;
    if(fsm){ tt.SetTextColor(kBlue+1);
      tt.DrawLatex(0.35,yy,Form("sim (dE/dx, no photon stats): %.2f #oplus %.1f/#sqrt{E} #oplus %.1f/E",
                   fabs(fsm->GetParameter(0)),fabs(fsm->GetParameter(1)),fabs(fsm->GetParameter(2)))); yy-=0.06; }
    if(fpe){ tt.SetTextColor(kMagenta+1);
      tt.DrawLatex(0.35,yy,Form("sim (photon count, real analog): %.2f #oplus %.1f/#sqrt{E} #oplus %.1f/E",
                   fabs(fpe->GetParameter(0)),fabs(fpe->GetParameter(1)),fabs(fpe->GetParameter(2)))); yy-=0.06; }
    tt.SetTextColor(kGray+2);
    tt.DrawLatex(0.35,yy,"paper Fig 17: 9.31 #oplus 52.04/#sqrt{E} #oplus 31.62/E");
    tt.SetTextColor(kBlack);
    c3->SaveAs(Form("%s/showermax_resolution.png",out));
    if(fsm) printf("\n  Shower-max res (dE/dx):        %.2f%% (+) %.1f%%/sqrt(E) (+) %.1f%%/E   [paper: 9.31 (+) 52.04 (+) 31.62]\n",
           fabs(fsm->GetParameter(0)),fabs(fsm->GetParameter(1)),fabs(fsm->GetParameter(2)));
    if(fpe) printf("  Shower-max res (photon count): %.2f%% (+) %.1f%%/sqrt(E) (+) %.1f%%/E   [paper: 9.31 (+) 52.04 (+) 31.62]\n",
           fabs(fpe->GetParameter(0)),fabs(fpe->GetParameter(1)),fabs(fpe->GetParameter(2)));
  }

  // ---------- persist curves as ROOT objects (refreshed every scan) ----------
  // gr/gt carry their fitted TF1 in their function list, so the fits are saved too.
  TFile* fo = new TFile(Form("%s/resolution_curves.root",dir),"RECREATE");
  gr->Write();
  gt->Write();
  if(gTimingScint) gTimingScint->Write();
  if(gTiming4c)    gTiming4c->Write();   // paper-matched estimator (light only)
  if(gTiming4d)    gTiming4d->Write();   // paper-matched + electronics
  if(gTimingWls)   gTimingWls->Write();
  if(gTimingHG)    gTimingHG->Write();
  if(gTimingHGE)   gTimingHGE->Write();
  if(gSM) gSM->Write();
  if(gPE) gPE->Write();
  // also store the raw scan points as a tidy TTree for quick inspection
  TTree* tr = new TTree("scan","resolution scan points");
  double bE,bSE,bST; tr->Branch("E",&bE); tr->Branch("sigmaE_pct",&bSE); tr->Branch("sigmaT_ps",&bST);
  for(int i=0;i<nGood;i++){ bE=EAll[i]; bSE=eResAll[i]; bST=tResAll[i]; tr->Fill(); }
  tr->Write();
  fo->Close();

  printf("\n  Energy res:  %.1f%%/sqrt(E) (+) %.2f%%\n",fabs(fr->GetParameter(0)),fabs(fr->GetParameter(1)));
  printf("  Timing res:  %.1f ps/sqrt(E) (+) %.1f ps\n",fabs(ft->GetParameter(0)),fabs(ft->GetParameter(1)));
  if(ft4d) printf("  *** PAPER-MATCHED + ELECTRONICS (4-cap mean + DRS4 + amp jitter):\n"
                  "        %.1f ps/sqrt(E) (+) %.1f ps   [paper: 256 ps/sqrt(E) (+) 17.5 ps]\n",
                  fabs(ft4d->GetParameter(0)),fabs(ft4d->GetParameter(1)));
  if(ft4c) printf("  *** PAPER-MATCHED light-only (4-capillary mean, scint, DW-UP):\n"
                  "        %.1f ps/sqrt(E) (+) %.1f ps   [paper: 256 ps/sqrt(E) (+) 17.5 ps]\n",
                  fabs(ft4c->GetParameter(0)),fabs(ft4c->GetParameter(1)));
  if(ftS) printf("  Timing res (scint-only, per-corner, no sqrt4 averaging): %.1f ps/sqrt(E) (+) %.1f ps   [paper: 256 ps/sqrt(E) (+) 17.5 ps]\n",
                 fabs(ftS->GetParameter(0)),fabs(ftS->GetParameter(1)));
  if(ftW) printf("  Timing res (WLS chain, LYSO->DSB1 re-emission): %.1f ps/sqrt(E) (+) %.1f ps   [paper: 256 ps/sqrt(E) (+) 17.5 ps]\n",
                 fabs(ftW->GetParameter(0)),fabs(ftW->GetParameter(1)));
  if(ftHG) printf("  Timing res (DUAL-GAIN high-gain, fixed threshold): %.1f ps/sqrt(E) (+) %.1f ps\n",
                 fabs(ftHG->GetParameter(0)),fabs(ftHG->GetParameter(1)));
  if(ftHGE) printf("  Timing res (high-gain, ENERGY-BINNED / time-walk corrected): %.1f ps/sqrt(E) (+) %.1f ps   [paper method]\n",
                 fabs(ftHGE->GetParameter(0)),fabs(ftHGE->GetParameter(1)));
  // Dual-gain low-gain energy: resolution + SiPM-saturation linearity check
  if(nGoodLG>=3){
    printf("  DUAL-GAIN low-gain energy (SiPM fired-pixel sum):\n");
    for(int i=0;i<nGoodLG;i++)
      printf("     mean N_fired=%.0f   sigma/mean=%.1f%%\n", eLGmean[i], eLG[i]);
    // linearity: mean N_fired per GeV should be flat if unsaturated, fall if saturating
    printf("     (falling N_fired/GeV at high E = SiPM pixel saturation setting in)\n");
  }
  printf("  Saved 3 curve PNGs to %s/  and  %s/resolution_curves.root\n\n",out,dir);
}
