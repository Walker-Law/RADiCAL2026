// plot_fig8.C — recreate arXiv:2401.01747 Fig 8: single-SiPM timing resolution
// vs detected light yield (npe/MeV), 50 GeV e- shower.
//
// For each yield-scale point (build/scan/optical_scan_<NEVT>_ly<scale>/):
//   x = detected LY = <PhotonsWLS> / <TotalCornerWLS in MeV>   (npe / MeV)
//   y = sigma_t     = gaussian-core width of H1[38] DeltaT_SingleDown (ps)
// then plots log-log with the paper's law sigma_t = 485 ps / sqrt(LY) overlaid.
//
//   root -l -b -q 'analysis/plot_fig8.C()'          (default NEVT=2000)
//   root -l -b -q 'analysis/plot_fig8.C(5000)'

TF1* core(TH1* h){
  double mu=h->GetMean(), sg=h->GetRMS();
  TF1* g=new TF1(Form("c%p",(void*)h),"gaus",mu-2*sg,mu+2*sg);
  for(int i=0;i<4;i++){ g->SetRange(mu-2*sg,mu+2*sg); h->Fit(g,"RQL0");
    mu=g->GetParameter(1); sg=g->GetParameter(2); if(sg<=0)break; }
  return g;
}

void plot_fig8(int NEVT=2000){
  gStyle->SetOptStat(0);
  const char* scales[]={"1e-4","3e-4","1e-3","3e-3","1e-2","2e-2"};
  const int NS=6;
  std::vector<double> LY, sigT, sigTerr;

  printf("\n  scale      LY(npe/MeV)   sigma_t(ps)\n");
  printf("  ------------------------------------------\n");
  for(int i=0;i<NS;i++){
    TString f=Form("build/scan/optical_scan_%d_ly%s/optical_E50GeV.root",NEVT,scales[i]);
    TFile* fp=TFile::Open(f);
    if(!fp||fp->IsZombie()){ printf("  %-8s  -- file missing --\n",scales[i]); continue; }
    TH1D* hN =(TH1D*)fp->Get("PhotonsWLS");       // detected WLS photons/event
    TH1D* hE =(TH1D*)fp->Get("TotalCornerWLS");   // fiber energy (MeV)/event
    TH1D* hT =(TH1D*)fp->Get("DeltaT_SingleDown");// single-ended downstream (ns)
    if(!hN||!hE||!hT||hT->GetEntries()<50){ printf("  %-8s  -- no data --\n",scales[i]); fp->Close(); continue; }
    double ly = (hE->GetMean()>0)? hN->GetMean()/hE->GetMean() : -1;
    TF1* g=core(hT);
    double st  = g->GetParameter(2)*1000.;        // ns -> ps
    double ste = g->GetParError(2)*1000.;
    if(ly>0 && st>0){
      LY.push_back(ly); sigT.push_back(st); sigTerr.push_back(ste);
      printf("  %-8s  %10.1f   %8.1f\n",scales[i],ly,st);
    }
    fp->Close();
  }
  if(LY.size()<3){ printf("\n  Need >=3 points — run run_fig8_sweep.sh first.\n"); return; }

  TCanvas* c=new TCanvas("c","fig8",800,600);
  c->SetLogx(); c->SetLogy(); c->SetGrid();
  std::vector<double> ex(LY.size(),0.);
  TGraphErrors* g=new TGraphErrors(LY.size(),LY.data(),sigT.data(),ex.data(),sigTerr.data());
  g->SetName("Fig8_SingleSiPM");
  g->SetTitle("Fig 8 recreation: single-SiPM timing vs detected light yield;LY (npe/MeV);#sigma_{t} (ps)");
  g->SetMarkerStyle(20); g->SetMarkerSize(1.3); g->SetMarkerColor(kBlue+1); g->SetLineColor(kBlue+1);
  g->Draw("AP");
  g->GetYaxis()->SetRangeUser(5,1000);
  // paper's law: sigma_t = 485 ps / sqrt(LY)
  TF1* fp=new TF1("fp","485./sqrt(x)",0.5,2000.);
  fp->SetLineColor(kRed); fp->SetLineStyle(2); fp->Draw("same");
  // our own fit sigma_t = a/sqrt(LY)
  TF1* fo=new TF1("fo","[0]/sqrt(x)",0.5,2000.);
  fo->SetParameter(0,485); fo->SetLineColor(kBlue+1);
  g->Fit(fo,"RQ");
  fo->Draw("same");
  TLatex t; t.SetNDC(); t.SetTextSize(0.038);
  t.SetTextColor(kBlue+1);
  t.DrawLatex(0.42,0.82,Form("sim: #sigma_{t} = %.0f ps/#sqrt{LY}",fabs(fo->GetParameter(0))));
  t.SetTextColor(kRed);
  t.DrawLatex(0.42,0.75,"paper Fig 8: 485 ps/#sqrt{LY}");
  t.SetTextColor(kBlack);
  gSystem->mkdir("build/plots",true);
  c->SaveAs("build/plots/fig8_timing_vs_LY.png");
  printf("\n  sim law: sigma_t = %.0f ps/sqrt(LY)   [paper Fig 8: 485 ps/sqrt(LY)]\n",
         fabs(fo->GetParameter(0)));
  printf("  Saved build/plots/fig8_timing_vs_LY.png\n\n");
}
