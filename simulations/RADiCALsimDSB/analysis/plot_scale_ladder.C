// plot_scale_ladder.C — decompose the timing stochastic term into a
// light-DEPENDENT (photostatistics) part and a light-INDEPENDENT floor.
//
// For each ladder point f (coherent light multiplier; f=1 == paperJ config):
//   1. read H1[34] DeltaT_CFD_4c_Scint_DRS4 at each energy, gaussian-core fit,
//      sigma_t = sigma(DeltaT)/2  (the (DW-UP)/2 corner trick), ns -> ps
//   2. fit sigma_t(E) = a(f)/sqrt(E) (+) b(f)
// then decompose the stochastic term across the ladder:
//      a^2(f) = A^2/f + B^2
// which is LINEAR in x = 1/f, so slope = A^2 (photostatistics at f=1) and
// intercept = B^2 (the light-independent shower/geometric floor).
//
// The point: sigma_t at the TRUE light yield is then an EXTRAPOLATION, not a
// tuned knob. If B dominates, the paper's 256 ps/sqrt(E) is sampling physics
// and light yield barely matters; if A dominates, our light-collection model
// is the thing to fix.
//
//   root -l -b -q 'analysis/plot_scale_ladder.C(1000)'

double coreSig(TH1* h){                    // iterative +-2.5 sigma gaussian core
  double mu=h->GetMean(), sg=h->GetRMS();
  TF1 g("cg","gaus",mu-2.5*sg,mu+2.5*sg);
  for(int i=0;i<4;i++){ g.SetRange(mu-2.5*sg,mu+2.5*sg); h->Fit(&g,"RQL0");
    mu=g.GetParameter(1); sg=fabs(g.GetParameter(2)); if(sg<=0) break; }
  return sg;
}

void plot_scale_ladder(int NEVT=1000){
  gStyle->SetOptStat(0);
  const char* fs[]={"0.1","0.3","1","3"};
  const double fv[]={0.1,0.3,1.0,3.0};
  const int NF=4;
  double E[6]={25,50,75,100,125,150};

  std::vector<double> vx, va2, va2e, vf, va, vb;
  printf("\n   f     a(f) [ps/sqrt(E)]   b(f) [ps]   <npe/MeV detected>\n");
  printf("  ----------------------------------------------------------\n");

  for(int k=0;k<NF;k++){
    double st[6], ste[6], zer[6]={0}, npeSum=0; int n=0, nNpe=0;
    for(int i=0;i<6;i++){
      TString fn=Form("build/scan/optical_scan_%d_lad%s/optical_E%.0fGeV.root",
                      NEVT,fs[k],E[i]);
      TFile* fp=TFile::Open(fn);
      if(!fp||fp->IsZombie()){ if(fp) fp->Close(); continue; }
      TH1* h=(TH1*)fp->Get("DeltaT_CFD_4c_Scint_DRS4");
      TH1* W=(TH1*)fp->Get("PhotonsScint");   // the population that times
      TH1* L=(TH1*)fp->Get("TotalLYSO");      // GeV
      if(h && h->GetEntries()>50){
        TF1* g=new TF1(Form("g%d%d",k,i),"gaus",0,1);
        double sg=coreSig(h);
        st[n]=sg*500.;                       // ns -> ps, and /2 corner trick
        ste[n]=st[n]*0.05;                   // ~5% fit error placeholder
        E[n]=E[i]; n++;
        delete g;
      }
      if(W&&L&&L->GetMean()>0){ npeSum += W->GetMean()/(L->GetMean()*1000.); nNpe++; }
      fp->Close();
    }
    if(n<3){ printf("  %-5s  -- not enough energy points --\n",fs[k]); continue; }
    double Eloc[6]; for(int i=0;i<n;i++) Eloc[i]=E[i];
    TGraphErrors g(n,Eloc,st,zer,ste);
    TF1 ff("ff","sqrt([0]*[0]/x+[1]*[1])",20,160);
    ff.SetParameters(250,18);
    g.Fit(&ff,"RQ");
    double a=fabs(ff.GetParameter(0)), b=fabs(ff.GetParameter(1));
    double ae=ff.GetParError(0);
    vf.push_back(fv[k]); va.push_back(a); vb.push_back(b);
    vx.push_back(1.0/fv[k]);
    va2.push_back(a*a);
    va2e.push_back(2*a*ae);
    printf("  %-5s  %10.1f          %8.1f      %10.2f\n",
           fs[k], a, b, nNpe? npeSum/nNpe : -1);
  }

  if(vx.size()<3){ printf("\n  Need >=3 ladder points — run run_scale_ladder.sh first.\n"); return; }

  // a^2 = A^2 * (1/f) + B^2   -> straight line in x=1/f
  int N=vx.size();
  TGraphErrors* gd=new TGraphErrors(N, vx.data(), va2.data(), nullptr, va2e.data());
  TF1* fl=new TF1("fl","[0]*x+[1]",0, *std::max_element(vx.begin(),vx.end())*1.1);
  fl->SetParNames("A^2 (photostat)","B^2 (floor)");
  gd->Fit(fl,"RQ");
  double A2=fl->GetParameter(0), B2=fl->GetParameter(1);
  double A=(A2>0)?sqrt(A2):0, B=(B2>0)?sqrt(B2):0;

  printf("\n  DECOMPOSITION  a^2(f) = A^2/f + B^2 :\n");
  printf("    A (photostatistics @ f=1) = %.1f ps/sqrt(E)\n", A);
  printf("    B (light-INDEPENDENT floor) = %.1f ps/sqrt(E)\n", B);
  printf("    -> at f=1  a = %.1f   [measured paperJ: 247.9]\n", sqrt(A2+B2));
  printf("\n  EXTRAPOLATION to true light yield (~100x the f=1 photon count):\n");
  printf("    f=10   a = %.1f\n", sqrt(A2/10.+B2));
  printf("    f=100  a = %.1f     <-- true LY   [paper: 256 ps/sqrt(E)]\n", sqrt(A2/100.+B2));
  printf("    f->inf a = %.1f     (pure floor)\n", B);
  printf("\n  READ: if B >> A/10 the paper's stochastic term is SAMPLING physics\n"
         "        (light yield nearly irrelevant); if A/10 >> B our light-collection\n"
         "        model is off by ~10x and that is the bug to hunt.\n\n");

  TCanvas* c=new TCanvas("c","ladder",900,650);
  c->SetGrid();
  gd->SetTitle("Timing stochastic term vs inverse light multiplier;"
               "1/f  (inverse light);a^{2}  (ps^{2}/GeV)");
  gd->SetMarkerStyle(20); gd->SetMarkerSize(1.5); gd->SetMarkerColor(kBlue+1);
  gd->SetLineColor(kBlue+1);
  gd->Draw("AP"); fl->SetLineColor(kRed+1); fl->Draw("same");
  TLatex t; t.SetNDC(); t.SetTextSize(0.038);
  t.DrawLatex(0.16,0.85,Form("A (photostat) = %.1f ps/#sqrt{E}",A));
  t.DrawLatex(0.16,0.79,Form("B (floor)     = %.1f ps/#sqrt{E}",B));
  gSystem->mkdir("build/plots",true);
  c->SaveAs("build/plots/scale_ladder_decomposition.png");
  printf("  Saved build/plots/scale_ladder_decomposition.png\n\n");
}
