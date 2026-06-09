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

void scan_resolution(const char* dir="build/scan") {
  gStyle->SetOptStat(0); gStyle->SetOptFit(0);
  gStyle->SetPadGridX(1); gStyle->SetPadGridY(1);
  const int N=6; double E[N]={5,10,20,50,100,120};
  const char* out="build/plots";

  double eRes[N],eResErr[N], tRes[N],tResErr[N], zero[N]={0};
  TCanvas* cL=new TCanvas("cL","long",800,600);
  TLegend* leg=new TLegend(0.62,0.60,0.88,0.88);
  int cols[N]={kRed+1,kOrange+1,kGreen+2,kAzure+2,kBlue+1,kMagenta+1};

  printf("\n  E(GeV)   mu_E(GeV)  sigma_E   sigma/E(%%)   DeltaT(ps)  sigma_t(ps)\n");
  printf("  -------------------------------------------------------------------\n");
  for(int i=0;i<N;i++){
    TFile* f=TFile::Open(Form("%s/radical_E%.0fGeV.root",dir,E[i]));
    // --- energy ---
    TH1D* hE=(TH1D*)f->Get("ECombined");
    { TF1* pre=coreFit(hE,2.0,3); double s=pre->GetParameter(2);
      double bw=hE->GetBinWidth(1); int rb=TMath::Max(1,(int)std::round((s/5.)/bw));
      if(rb>1)hE->Rebin(rb); delete pre; }
    TF1* gE=coreFit(hE,2.0,4);
    double muE=gE->GetParameter(1), sgE=gE->GetParameter(2), sgEerr=gE->GetParError(2);
    eRes[i]=100*sgE/muE; eResErr[i]=100*sgEerr/muE;
    // --- timing ---
    TH1D* hT=(TH1D*)f->Get("DeltaT");
    TF1* gT=coreFit(hT,2.5,4);
    double muT=gT->GetParameter(1)*1000, sgT=gT->GetParameter(2)*1000, sgTerr=gT->GetParError(2)*1000;
    tRes[i]=sgT; tResErr[i]=sgTerr;
    printf("  %5.0f    %7.3f   %6.3f    %6.2f       %6.1f      %6.2f\n",
           E[i],muE,sgE,eRes[i],muT,sgT);
    // --- longitudinal profile overlay (normalized to unit area) ---
    TH1D* hL=(TH1D*)f->Get("ShowerProfile"); hL=(TH1D*)hL->Clone(Form("L%d",i));
    if(hL->Integral()>0) hL->Scale(1.0/hL->Integral());
    hL->SetLineColor(cols[i]); hL->SetLineWidth(2);
    hL->SetTitle("Longitudinal shower profile vs energy;LYSO layer;normalized <E>");
    cL->cd(); hL->Draw(i==0?"hist":"hist same");
    hL->SetMaximum(0.12);
    leg->AddEntry(hL,Form("%.0f GeV",E[i]),"l");
    // keep file open? clone histos already; close.
  }
  leg->Draw(); cL->SaveAs(Form("%s/shower_long_overlay.png",out));

  // ---------- energy resolution curve ----------
  TCanvas* c1=new TCanvas("c1","eres",800,600);
  TGraphErrors* gr=new TGraphErrors(N,E,eRes,zero,eResErr);
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
  TGraphErrors* gt=new TGraphErrors(N,E,tRes,zero,tResErr);
  gt->SetName("TimingResolution");
  gt->SetTitle("Timing resolution (front #minus back);E_{beam} (GeV);#sigma_{t} (ps)");
  gt->SetMarkerStyle(21); gt->SetMarkerColor(kGreen+2); gt->SetLineColor(kGreen+2);
  gt->SetMarkerSize(1.3);
  TF1* ft=new TF1("ft","sqrt([0]*[0]/x+[1]*[1])",4,130);
  ft->SetParameters(20,5); ft->SetParNames("stoch","const");
  ft->SetLineColor(kRed); gt->Fit(ft,"RQ");
  gt->Draw("AP"); ft->Draw("same");
  t.DrawLatex(0.40,0.82,Form("#sigma_{t} = %.1f ps/#sqrt{E} #oplus %.1f ps",
              fabs(ft->GetParameter(0)),fabs(ft->GetParameter(1))));
  c2->SaveAs(Form("%s/timing_resolution_curve.png",out));

  // ---------- persist curves as ROOT objects (refreshed every scan) ----------
  // gr/gt carry their fitted TF1 in their function list, so the fits are saved too.
  TFile* fo = new TFile(Form("%s/resolution_curves.root",dir),"RECREATE");
  gr->Write();
  gt->Write();
  // also store the raw scan points as a tidy TTree for quick inspection
  TTree* tr = new TTree("scan","resolution scan points");
  double bE,bSE,bST; tr->Branch("E",&bE); tr->Branch("sigmaE_pct",&bSE); tr->Branch("sigmaT_ps",&bST);
  for(int i=0;i<N;i++){ bE=E[i]; bSE=eRes[i]; bST=tRes[i]; tr->Fill(); }
  tr->Write();
  fo->Close();

  printf("\n  Energy res:  %.1f%%/sqrt(E) (+) %.2f%%\n",fabs(fr->GetParameter(0)),fabs(fr->GetParameter(1)));
  printf("  Timing res:  %.1f ps/sqrt(E) (+) %.1f ps\n",fabs(ft->GetParameter(0)),fabs(ft->GetParameter(1)));
  printf("  Saved 3 curve PNGs to %s/  and  build/scan/resolution_curves.root\n\n",out);
}
