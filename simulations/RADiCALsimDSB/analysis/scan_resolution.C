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

void scan_resolution(const char* dir="build/scan", const char* prefix="radical") {
  gStyle->SetOptStat(0); gStyle->SetOptFit(0);
  gStyle->SetPadGridX(1); gStyle->SetPadGridY(1);
  const int N=8; double E[N]={5,10,20,25,50,100,120,150};
  const char* out="build/plots";

  double eResAll[N],eResErrAll[N], tResAll[N],tResErrAll[N], EAll[N];
  double tResS[N],tResSErr[N], ES[N]; int nGoodS=0;   // scint-only timing (if present)
  TGraphErrors* gTimingScint=nullptr;
  double smRes[N],smResErr[N], ESM[N]; int nGoodSM=0; // shower-max slice res (if present)
  TCanvas* cL=new TCanvas("cL","long",800,600);
  TLegend* leg=new TLegend(0.62,0.55,0.88,0.88);
  int cols[N]={kRed+1,kOrange+1,kGreen+2,kSpring+4,kAzure+2,kBlue+1,kMagenta+1,kViolet+2};

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
    // --- timing ---
    TH1D* hT=(TH1D*)f->Get("DeltaT");
    TF1* gT=coreFit(hT,2.5,4);
    // (DW−UP)/2 corner trick: σ_t = σ(ΔT)/2 (dividing by 2 gives physical timing resolution)
    double muT=gT->GetParameter(1)*1000, sgT=gT->GetParameter(2)*500, sgTerr=gT->GetParError(2)*500;
    // scint-only timing (new files only): same corner trick, Cherenkov excluded
    double sgTS=-1;
    TH1D* hTS=(TH1D*)f->Get("DeltaT_Scint");
    if(hTS && hTS->GetEntries()>50){
      TF1* gTS=coreFit(hTS,2.5,4);
      sgTS=gTS->GetParameter(2)*500;
      ES[nGoodS]=E[i]; tResS[nGoodS]=sgTS; tResSErr[nGoodS]=gTS->GetParError(2)*500;
      nGoodS++;
    }
    // shower-max slice resolution (paper Fig 17 analog; new files only)
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
    printf("  %5.0f    %7.3f   %6.3f    %6.2f       %6.1f      %6.2f      %s\n",
           E[i],muE,sgE,eResI,muT,sgT,
           sgTS>0?Form("(scint-only: %.2f)",sgTS):"");
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
    double tmax0=2.5*TMath::Log(E[i])+1.0, b0=0.65;
    fL->SetParameters(hL->GetMaximum(), tmax0*b0, b0);
    fL->SetParLimits(0,1e-6,10.); fL->SetParLimits(1,0.1,20.); fL->SetParLimits(2,0.01,5.);
    hL->Fit(fL,"RQ0");  // Q=quiet, 0=don't draw automatically
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
  ft->SetLineColor(kRed); gt->Fit(ft,"RQ");
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
  TMultiGraph* mgT=new TMultiGraph();
  mgT->Add(gt,"P");
  if(gTimingScint) mgT->Add(gTimingScint,"P");
  mgT->SetTitle("Timing resolution (downstream #minus upstream);E_{beam} (GeV);#sigma_{t} (ps)");
  mgT->Draw("A");
  mgT->SetMinimum(0.);
  ft->Draw("same");
  if(ftS) ftS->Draw("same");
  t.DrawLatex(0.40,0.84,Form("all light: #sigma_{t} = %.1f ps/#sqrt{E} #oplus %.1f ps",
              fabs(ft->GetParameter(0)),fabs(ft->GetParameter(1))));
  if(ftS){
    t.SetTextColor(kAzure+2);
    t.DrawLatex(0.40,0.77,Form("scint-only: %.1f ps/#sqrt{E} #oplus %.1f ps",
                fabs(ftS->GetParameter(0)),fabs(ftS->GetParameter(1))));
    t.SetTextColor(kGray+2);
    t.DrawLatex(0.40,0.70,"paper: 256 ps/#sqrt{E} #oplus 17.5 ps");
    t.SetTextColor(kBlack);
  }
  c2->SaveAs(Form("%s/timing_resolution_curve.png",out));

  // ---------- shower-max slice resolution vs paper Fig 17 (right) ----------
  TGraphErrors* gSM=nullptr;
  if(nGoodSM>=3){
    TCanvas* c3=new TCanvas("c3","smres",800,600);
    double zeroSM[N]={0};
    gSM=new TGraphErrors(nGoodSM,ESM,smRes,zeroSM,smResErr);
    gSM->SetName("ShowerMaxResolution");
    gSM->SetTitle("Shower-max slice energy resolution (LYSO layers 8-10);E_{beam} (GeV);#sigma_{E}/E (%)");
    gSM->SetMarkerStyle(20); gSM->SetMarkerColor(kBlue+1); gSM->SetLineColor(kBlue+1);
    gSM->SetMarkerSize(1.3);
    TF1* fsm=new TF1("fsm","sqrt([0]*[0]+[1]*[1]/x+[2]*[2]/(x*x))",4,130);
    fsm->SetParameters(9,50,30); fsm->SetLineColor(kRed);
    gSM->Fit(fsm,"RQ");
    gSM->Draw("AP"); fsm->Draw("same");
    // paper Fig 17 fit for reference
    TF1* fpap=new TF1("fpap","sqrt(9.31*9.31+52.04*52.04/x+31.62*31.62/(x*x))",4,160);
    fpap->SetLineColor(kGray+2); fpap->SetLineStyle(2); fpap->Draw("same");
    TLatex tt; tt.SetNDC(); tt.SetTextSize(0.035);
    tt.DrawLatex(0.35,0.84,Form("sim: %.2f%% #oplus %.1f%%/#sqrt{E} #oplus %.1f%%/E",
                 fabs(fsm->GetParameter(0)),fabs(fsm->GetParameter(1)),fabs(fsm->GetParameter(2))));
    tt.SetTextColor(kGray+2);
    tt.DrawLatex(0.35,0.77,"paper Fig 17: 9.31% #oplus 52.04%/#sqrt{E} #oplus 31.62%/E");
    tt.SetTextColor(kBlack);
    c3->SaveAs(Form("%s/showermax_resolution.png",out));
    printf("\n  Shower-max slice res: %.2f%% (+) %.1f%%/sqrt(E) (+) %.1f%%/E   [paper Fig 17: 9.31 (+) 52.04 (+) 31.62]\n",
           fabs(fsm->GetParameter(0)),fabs(fsm->GetParameter(1)),fabs(fsm->GetParameter(2)));
  }

  // ---------- persist curves as ROOT objects (refreshed every scan) ----------
  // gr/gt carry their fitted TF1 in their function list, so the fits are saved too.
  TFile* fo = new TFile(Form("%s/resolution_curves.root",dir),"RECREATE");
  gr->Write();
  gt->Write();
  if(gTimingScint) gTimingScint->Write();
  if(gSM) gSM->Write();
  // also store the raw scan points as a tidy TTree for quick inspection
  TTree* tr = new TTree("scan","resolution scan points");
  double bE,bSE,bST; tr->Branch("E",&bE); tr->Branch("sigmaE_pct",&bSE); tr->Branch("sigmaT_ps",&bST);
  for(int i=0;i<nGood;i++){ bE=EAll[i]; bSE=eResAll[i]; bST=tResAll[i]; tr->Fill(); }
  tr->Write();
  fo->Close();

  printf("\n  Energy res:  %.1f%%/sqrt(E) (+) %.2f%%\n",fabs(fr->GetParameter(0)),fabs(fr->GetParameter(1)));
  printf("  Timing res:  %.1f ps/sqrt(E) (+) %.1f ps\n",fabs(ft->GetParameter(0)),fabs(ft->GetParameter(1)));
  if(ftS) printf("  Timing res (scint-only, Cherenkov excluded): %.1f ps/sqrt(E) (+) %.1f ps   [paper: 256 ps/sqrt(E) (+) 17.5 ps]\n",
                 fabs(ftS->GetParameter(0)),fabs(ftS->GetParameter(1)));
  printf("  Saved 3 curve PNGs to %s/  and  %s/resolution_curves.root\n\n",out,dir);
}
