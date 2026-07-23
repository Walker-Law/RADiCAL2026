// ladder_paperR.C — decompose the realistic-composition timing into
// photostatistics (A) and light-independent floor (B), then extrapolate to
// TRUE light. Run after run_ladder_paperR.sh:
//
//   root -l -b -q 'analysis/ladder_paperR.C(500)'
//
// Reads each ladder point's stored TimingResolution_4cAllDRS4 fit (the
// all-light + electronics headline) from resolution_curves.root, measures the
// ACTUAL light ratio f_meas from <PhotonsWLS> (not the nominal knob), fits
//
//   a^2(f) = A^2/f + B^2      (linear in x = 1/f)
//
// and reports a_true = sqrt(A^2/f_true + B^2) at f_true = 1/base_scale (=100
// for the paperR base 1e-2) beside the paper's 256 ps/sqrt(E) (+) 17.5 ps.
// Also prints the per-point WLS capture*PDE fraction (H1 PhotonsWLS /
// PhotonsWLSEmitted) — sanity vs the ~3% bare-fiber TIR x PDE estimate.

void ladder_paperR(int nevt = 500) {
  const int NF = 4;
  const double fnom[NF] = {0.1, 0.3, 1., 3.};
  const double baseScale = 1e-2;                 // paperR thinning at f=1
  const int EREF = 50;                           // light-ratio reference energy

  double light[NF], a[NF], ae[NF], b[NF], cap[NF];
  bool ok[NF] = {false, false, false, false};

  for (int i = 0; i < NF; ++i) {
    TString dir = Form("build/scan/optical_scan_%d_paperR_lad%g", nevt, fnom[i]);
    // light + capture from the 50 GeV file
    TFile* fE = TFile::Open(Form("%s/optical_E%dGeV.root", dir.Data(), EREF), "READ");
    if (!fE || fE->IsZombie()) { printf("  [skip] %s (no data)\n", dir.Data()); continue; }
    TH1* wl = (TH1*)fE->Get("PhotonsWLS");
    TH1* we = (TH1*)fE->Get("PhotonsWLSEmitted");
    light[i] = wl ? wl->GetMean() : -1;
    cap[i]   = (wl && we && we->GetMean() > 0) ? wl->GetMean() / we->GetMean() : -1;
    fE->Close();
    // stored headline fit
    TFile* fC = TFile::Open(Form("%s/resolution_curves.root", dir.Data()), "READ");
    if (!fC || fC->IsZombie()) { printf("  [skip] %s (no curves)\n", dir.Data()); continue; }
    TGraphErrors* g = (TGraphErrors*)fC->Get("TimingResolution_4cAllDRS4");
    TF1* ft = g ? (TF1*)g->GetListOfFunctions()->First() : nullptr;
    if (ft) {
      a[i]  = ft->GetParameter(0); ae[i] = ft->GetParError(0);
      b[i]  = std::fabs(ft->GetParameter(1));
      ok[i] = (a[i] > 0);
    }
    fC->Close();
  }

  // normalize measured light to the f=1 point
  int i1 = 2;                                    // f=1 index
  if (!ok[i1] || light[i1] <= 0) { printf("f=1 point missing — cannot normalize.\n"); return; }

  printf("\n%-8s %10s %8s %12s %8s %12s\n",
         "f_nom", "<PhWLS>", "f_meas", "a [ps/sqE]", "b [ps]", "capture*PDE");
  std::vector<double> X, Y, YE;
  for (int i = 0; i < NF; ++i) {
    if (!ok[i]) continue;
    double fm = light[i] / light[i1];
    printf("%-8g %10.1f %8.3f %7.1f+-%4.1f %8.2f %11.2f%%\n",
           fnom[i], light[i], fm, a[i], ae[i], b[i], 100. * cap[i]);
    X.push_back(1. / fm); Y.push_back(a[i] * a[i]); YE.push_back(2. * a[i] * ae[i]);
  }
  if (X.size() < 3) { printf("need >= 3 points for the ladder fit.\n"); return; }

  TGraphErrors* gr = new TGraphErrors(X.size(), X.data(), Y.data(), nullptr, YE.data());
  TF1* lin = new TF1("lin", "[0]*x+[1]", 0., 12.);
  gr->Fit(lin, "Q0");
  double A = std::sqrt(std::max(0., lin->GetParameter(0)));
  double B2 = lin->GetParameter(1);
  double B = std::sqrt(std::fabs(B2));
  printf("\n  a^2(f) = A^2/f + B^2 :  A = %.1f ps/sqrt(E)   B = %s%.1f ps/sqrt(E)"
         "   chi2/ndf = %.2f/%d\n",
         A, (B2 < 0 ? "(imag) " : ""), B, lin->GetChisquare(), lin->GetNDF());

  double fTrue = 1. / baseScale;
  double aTrue = std::sqrt(A * A / fTrue + std::max(0., B2));
  printf("\n  EXTRAPOLATION to TRUE light (f = %.0f):  a = %.1f ps/sqrt(E)\n", fTrue, aTrue);
  printf("  PAPER:                                   a = 256.0 ps/sqrt(E) (+) 17.5 ps\n\n");
  printf("  (b floors above are per-point constant terms — electronics-dominated,\n"
         "   not extrapolated; compare directly to the paper's 17.5 ps.)\n");
}
