// compare_graphs.C — graphical data-vs-sim timing comparison.
//   root -l -b -q analysis/compare_graphs.C
// Makes (build/plots/datacomp/):
//   sigma_vs_E.png        — sigma_t vs E: data, sim waveform-CFD, sim first-photon
//   deltaT_overlay_*.png  — normalized sim DeltaT_CFD vs data sigma band
TF1* coreFit(TH1* h, double ns=2.0) {
  double mu=h->GetMean(), sg=h->GetRMS();
  TF1* g=new TF1(Form("cf%s",h->GetName()),"gaus",mu-ns*sg,mu+ns*sg);
  for(int i=0;i<4;i++){ g->SetRange(mu-ns*sg,mu+ns*sg); h->Fit(g,"RQL0");
    mu=g->GetParameter(1); sg=g->GetParameter(2); if(sg<=0)break; }
  return g;
}
void compare_graphs() {
  gStyle->SetOptStat(0); gStyle->SetPadGridX(1); gStyle->SetPadGridY(1);
  const char* out="build/plots/datacomp";
  const int N=2; double E[N]={25,150};
  double dataS[N]={614,476}, dataErr[N]={15,10};   // ps (5% CFD; err ~ run spread)
  double simC[N], simCe[N], simF[N], simFe[N];
  for (int i=0;i<N;i++){
    TFile* f=TFile::Open(Form("build/datacomp/radical_E%.0fGeV.root",E[i]));
    TF1* gC=coreFit((TH1D*)f->Get("DeltaT_CFD"));
    TF1* gF=coreFit((TH1D*)f->Get("DeltaT"));
    simC[i]=gC->GetParameter(2)*1000; simCe[i]=gC->GetParError(2)*1000;
    simF[i]=gF->GetParameter(2)*1000; simFe[i]=gF->GetParError(2)*1000;
    f->Close();
  }
  TCanvas* c=new TCanvas("c","",850,620);
  c->SetLogy();
  double ze[N]={0,0};
  TGraphErrors* gD=new TGraphErrors(N,E,dataS,ze,dataErr);
  gD->SetMarkerStyle(20); gD->SetMarkerSize(1.5); gD->SetMarkerColor(kBlack); gD->SetLineColor(kBlack);
  TGraphErrors* gC=new TGraphErrors(N,E,simC,ze,simCe);
  gC->SetMarkerStyle(21); gC->SetMarkerSize(1.4); gC->SetMarkerColor(kRed+1); gC->SetLineColor(kRed+1);
  TGraphErrors* gF=new TGraphErrors(N,E,simF,ze,simFe);
  gF->SetMarkerStyle(24); gF->SetMarkerSize(1.4); gF->SetMarkerColor(kAzure+2); gF->SetLineColor(kAzure+2);
  gD->SetTitle("Timing resolution: test-beam data vs simulation;E_{beam} (GeV);#sigma_{t} (ps)");
  gD->GetYaxis()->SetRangeUser(50,900);
  gD->GetXaxis()->SetLimits(0,170);
  gD->Draw("AP"); gC->Draw("P same"); gF->Draw("P same");
  TLegend* lg=new TLegend(0.40,0.42,0.87,0.62);
  lg->AddEntry(gD,"DATA (5% CFD, core fit)","pe");
  lg->AddEntry(gC,"SIM waveform-emulated 5% CFD","pe");
  lg->AddEntry(gF,"SIM first-photon (idealized floor)","pe");
  lg->Draw();
  c->SaveAs(Form("%s/sigma_vs_E.png",out));
  printf("sim CFD: %.0f ps (25), %.0f ps (150)\n", simC[0], simC[1]);
}
