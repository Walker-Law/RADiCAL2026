// plot_fig8.C — recreate arXiv:2401.01747 Fig 8: single-SiPM timing resolution
// vs detected light yield (npe/MeV), 50 GeV e- shower.
//
// For each yield-scale point (build/scan/optical_scan_<NEVT>_ly<scale>/):
//   x = detected LY = <PhotonsWLSDown> / <TotalCornerWLS in MeV>   (npe / MeV)
//   y = sigma_t     = robust clipped-RMS width of H1[38] DeltaT_SingleDown (ps)
// then plots log-log with the paper's law sigma_t = 485 ps / sqrt(LY) overlaid
// and fits the sim's own photostatistics-plus-floor law a/sqrt(LY) (+) c.
//
//   root -l -b -q 'analysis/plot_fig8.C()'          (default NEVT=2000)
//   root -l -b -q 'analysis/plot_fig8.C(1000)'

// Robust, MONOTONIC width of the single-SiPM timing distribution: iteratively
// sigma-clipped RMS. The (CFD - fiber time) distribution is a leading-edge core
// plus a long tail (events where late-decaying LYSO photons set the 5% crossing).
// An earlier peak-Gaussian-fit estimator was NON-monotonic at low LY — with few
// photons the peak search latched onto a spurious narrow prompt cluster, giving
// the dimmest point an artificially GOOD sigma_t. Sigma-clipping to +-2.5 sigma
// converges on the core width while discarding the extreme tail, is stable at low
// statistics, and is monotonic in LY (verified: 4017->2203->1724->1556->1457->1439
// ps across the yield sweep). Returns sigma (ns); *err = large-N RMS error.
double clipSigma(TH1* h, double* err){
  double lo=h->GetMean()-5*h->GetRMS(), hi=h->GetMean()+5*h->GetRMS();
  double s=h->GetRMS(), n=h->GetEntries();
  for(int it=0; it<6; it++){
    h->GetXaxis()->SetRangeUser(lo,hi);
    double m=h->GetMean(); s=h->GetRMS(); n=h->GetEffectiveEntries();
    lo=m-2.5*s; hi=m+2.5*s;
  }
  h->GetXaxis()->SetRange(0,0);                 // restore full range
  if(err) *err = (n>1) ? s/std::sqrt(2.*n) : s; // analytic RMS error sigma/sqrt(2N)
  return s;
}

// PROMPT-PEAK sigma — the physical single-SiPM timing resolution.
// The (t_thresh - t_MCP) distribution is a dominant PROMPT peak (events whose
// early light burst clears the threshold immediately) plus a LATE tail near
// ~5 ns (events whose prompt light fell short, so the threshold is only crossed
// later on the slow LYSO-36ns decay). The late tail SHRINKS as light yield rises,
// so it is a low-light-fluctuation population, not a resolution. Mixing the two
// (plain RMS or clipped RMS) reports the population SEPARATION rather than the
// timing width. Here we fit a gaussian to the dominant prompt peak only.
// Returns sigma (ns); *err = fit error; *frac = fraction of events within +-3sigma
// of the prompt peak (report it — it is the honest caveat on this number).
double promptPeakSigma(TH1* hin, double* err, double* frac){
  TH1* h=(TH1*)hin->Clone(Form("pp%p",(void*)hin)); h->SetDirectory(nullptr);
  h->Rebin(10);
  int pk=h->GetMaximumBin();
  double x0=h->GetXaxis()->GetBinCenter(pk), sg=0.3;
  TF1 g("ppg","gaus",x0-0.6,x0+0.6);
  for(int it=0; it<6; it++){
    g.SetRange(x0-2.0*sg, x0+2.0*sg);
    h->Fit(&g,"RQ0");
    x0=g.GetParameter(1); sg=fabs(g.GetParameter(2));
    if(sg<=0 || sg>2.) break;
  }
  if(err)  *err  = g.GetParError(2);
  if(frac) *frac = (h->Integral()>0)
      ? h->Integral(h->FindBin(x0-3*sg), h->FindBin(x0+3*sg))/h->Integral() : 0.;
  double out=sg; delete h; return out;
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
    // single-SiPM LY: DOWNSTREAM-only WLS photons (H1[39]); fall back to the
    // all-corner/both-end PhotonsWLS/8 for older files that lack H1[39].
    TH1D* hN =(TH1D*)fp->Get("PhotonsWLSDown");
    double npeScale=1.0;
    if(!hN){ hN=(TH1D*)fp->Get("PhotonsWLS"); npeScale=1.0/8.0; }  // ~4 corners x 2 ends
    TH1D* hE =(TH1D*)fp->Get("TotalCornerWLS");   // fiber energy (MeV)/event
    TH1D* hT =(TH1D*)fp->Get("DeltaT_SingleDown");// single-ended downstream (ns)
    if(!hN||!hE||!hT||hT->GetEntries()<50){ printf("  %-8s  -- no data --\n",scales[i]); fp->Close(); continue; }
    double ly = (hE->GetMean()>0)? npeScale*hN->GetMean()/hE->GetMean() : -1;
    double ste_ns=0., frac=0.;
    double st  = promptPeakSigma(hT,&ste_ns,&frac)*1000.;   // prompt-peak sigma, ns -> ps
    double ste = ste_ns*1000.;
    // crossFrac = fraction of the 1000 events that cleared the fixed threshold at
    // all. Well below ~90% means the point is threshold-STARVED, not measured.
    double crossFrac = hT->GetEntries()/1000.;
    if(ly>0 && st>0){
      LY.push_back(ly); sigT.push_back(st); sigTerr.push_back(ste);
      printf("  %-8s  %10.1f   %8.1f      %4.0f%%       %4.0f%%%s\n",
             scales[i],ly,st,100*frac,100*crossFrac,
             crossFrac<0.90 ? "   <-- THRESHOLD-STARVED" : "");
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
  g->GetYaxis()->SetRangeUser(20,6000);
  // paper's law: sigma_t = 485 ps / sqrt(LY) (pure photostatistics, no floor)
  TF1* fpap=new TF1("fpap","485./sqrt(x)",0.5,2000.);
  fpap->SetLineColor(kRed); fpap->SetLineStyle(2); fpap->Draw("same");
  // sim's own law: photostatistics a/sqrt(LY) PLUS a constant single-ended floor c.
  // A pure 1/sqrt(LY) cannot describe the high-LY flattening — single-ended timing
  // carries the shower-development / fiber-reference jitter that the dual-ended
  // (DW-UP)/2 method cancels, so sigma_t floors instead of going to zero.
  TF1* fo=new TF1("fo","sqrt([0]*[0]/x+[1]*[1])",0.5,2000.);
  fo->SetParameters(485,1000); fo->SetParNames("stoch(ps)","floor(ps)");
  fo->SetLineColor(kBlue+1);
  g->Fit(fo,"RQ");
  fo->Draw("same");
  TLatex t; t.SetNDC(); t.SetTextSize(0.036);
  t.SetTextColor(kBlue+1);
  t.DrawLatex(0.38,0.86,Form("sim: #sigma_{t} = %.0f ps/#sqrt{LY} #oplus %.0f ps",
              fabs(fo->GetParameter(0)),fabs(fo->GetParameter(1))));
  t.SetTextColor(kRed);
  t.DrawLatex(0.38,0.79,"paper Fig 8: 485 ps/#sqrt{LY}");
  t.SetTextColor(kBlack);
  gSystem->mkdir("build/plots",true);
  c->SaveAs("build/plots/fig8_timing_vs_LY.png");
  printf("\n  sim law: sigma_t = %.0f ps/sqrt(LY) (+) %.0f ps floor   [paper Fig 8: 485 ps/sqrt(LY)]\n",
         fabs(fo->GetParameter(0)),fabs(fo->GetParameter(1)));
  printf("  Saved build/plots/fig8_timing_vs_LY.png\n\n");
}
