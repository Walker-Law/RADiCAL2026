// compare_data.C — side-by-side of simulated vs CERN test-beam timing.
//
//   root -l -b -q analysis/compare_data.C
//
// (DW−UP)/2 corner trick: ΔT = t_down − t_up = (L−2z)/v_g, so the physical
// timing resolution is σ_t = σ(ΔT)/2. All sim values below are reported as
// σ(ΔT)/2. Data values are σ(ΔT) from raw DRS4 waveforms (uncorrected for
// DRS4 inter-cell timing jitter); divide by 2 to compare with sim on the same
// footing, but the residual data/sim gap is dominated by uncorrected DRS4 jitter.
//
// Data reference values (extracted from RUN1211/1259/1260/1261 waveforms,
// in-time pulse selection, 5% CFD [per experiment convention], iterative ±2σ
// Gaussian core fit):
//   σ(ΔT)(25 GeV)  = 614 ps → σ_t = 307 ps  (data, DRS4-uncorrected)
//   σ(ΔT)(150 GeV) = 476 ps → σ_t = 238 ps  (data, DRS4-uncorrected; 3-run mean: 488/470/471)
//   pulse FWHM  ≈ 8.3 ns     (ch0/ch1 capillary pair)
// Sim files: build/datacomp/radical_E{25,150}GeV.root
//   DeltaT_CFD = waveform-emulated (SPR conv. + 5 GS/s + 5% CFD) — the
//   data-identical estimator.  DeltaT = first-photon leading edge (idealized).
//   Both are reported as σ(ΔT)/2 to give physical σ_t.

TF1* coreFit(TH1* h, double nsig=2.0, int iters=4) {
  double mu=h->GetMean(), sg=h->GetRMS();
  TF1* g=new TF1(Form("cf_%s",h->GetName()),"gaus",mu-nsig*sg,mu+nsig*sg);
  for(int i=0;i<iters;i++){ g->SetRange(mu-nsig*sg,mu+nsig*sg); h->Fit(g,"RQL0");
    mu=g->GetParameter(1); sg=g->GetParameter(2); if(sg<=0)break; }
  return g;
}

void compare_data() {
  gStyle->SetOptStat(0);
  const int N=2; double E[N]={25,150};
  double dataSig[N]={614,476};                 // ps, measured (5% CFD)
  double dataFWHM=8.3;                          // ns
  printf("\n E(GeV) | DATA sigma_t | SIM CFD sigma_t | SIM first-photon | SIM FWHM\n");
  printf(" -----------------------------------------------------------------------\n");
  for (int i=0;i<N;i++){
    TFile* f=TFile::Open(Form("build/datacomp/radical_E%.0fGeV.root",E[i]));
    if(!f||f->IsZombie()){ printf(" %5.0f | (sim file missing)\n",E[i]); continue; }
    TH1D* hC=(TH1D*)f->Get("DeltaT_CFD");
    TH1D* hF=(TH1D*)f->Get("DeltaT");
    TH1D* hW=(TH1D*)f->Get("PulseFWHM");
    TF1* gC=coreFit(hC); TF1* gF=coreFit(hF);
    printf(" %5.0f | %9.0f ps | %12.0f ps | %13.0f ps | %5.1f ns\n",
      E[i], dataSig[i], gC->GetParameter(2)*1000, gF->GetParameter(2)*1000,
      hW->GetMean());
    f->Close();
  }
  printf("\n DATA pulse FWHM ~ %.1f ns (ch0/ch1)\n\n", dataFWHM);
}
