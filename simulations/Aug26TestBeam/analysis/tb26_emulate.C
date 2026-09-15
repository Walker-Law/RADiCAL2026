// tb26_emulate.C — run the BEAM TEST's own analysis on the SIMULATION.
//
// The experiment never sees photons: it sees, per corner, two digitised
// waveforms (CAEN DT5742, 1024 samples at 0.2 ns) — a high-gain chain used for
// timing and a low-gain chain used for energy — and its published numbers
// come from a specific algorithm on those waveforms (radical-t10-2026,
// macros/EnergyScan*.C and macros/DiagDiff.C). This macro
//
//   1. turns the stored photons of every simulated event (phT, phId) into the
//      same two waveforms per corner, with the measured impulse responses
//      (analysis/response_kernels.txt), measured noise, the measured clip wall
//      and the measured pulse placement in the record;
//   2. applies the experiment's functions VERBATIM — pulseOf, leTime, the
//      wall-aware low-gain-to-high-gain transfer calibration, the
//      saturation-recovered constant-fraction discriminator at 15% of the
//      predicted high-gain peak with its two guards, tebSigma, the Sum-LG
//      Gaussian core fit, the on-module gate — with their exact constants;
//   3. prints the two tables the experiment publishes, in the same format:
//      the EnergyScan line (response peak, sigma/E, shower-time sigma) and the
//      DiagDiff line (reference-included sigma, intrinsic sigma), per energy.
//
// Usage (from Aug26TestBeam/):
//   root -l -b -q 'analysis/tb26_emulate.C("build/rootfiles", 1.0)'
// second argument = RADSIMPLE_LIGHT_SCALE the files were produced with. Pulse
// AMPLITUDES are corrected by 1/f so gains stay meaningful on thinned files,
// but photon-statistics noise is then wrong by sqrt(1/f): thinned files test
// the pipeline, not the physics.
//
// Things the simulation does not contain, declared here with their source:
//   response kernels  analysis/response_kernels.txt (fitted to runs 33/27/42)
//   gains k_HG, k_LG  ADC-eq per photon: calibrated on the DSB1 5 GeV file so
//                     that Sum-LG peak = 6769 ADC-eq (run 37) and the transfer
//                     slope HG/LG = 2.9 (run 37); printed, and overridable
//   noise             white, HG 6.6 mV, LG 1.6 mV (pre-pulse RMS, run 33)
//   clip wall         HG saturates 780 mV above baseline (3194 ADC-eq; the
//                     measured 99.5% walls are 3143-3197)
//   placement         light onset at sample 22 so the HG peak sits near sample
//                     62 (measured medians 58-66); baseline window 0-39 is then
//                     contaminated by the pulse foot EXACTLY as in the data
//   reference         the simulation's event time is perfect. The DiagDiff
//                     intrinsic column needs no reference; the "meanIncl"
//                     column is reported twice: against the perfect reference
//                     and with a 110 ps Gaussian jitter added (the measured
//                     MCP + digitiser floor, ResolutionV2 run 15: 107.6 ps)
//   selection         no XCET tag (the beam is pure electrons); the on-module
//                     gate SMIN per material and energy is copied from
//                     EnergyScan.C / EnergyScanDSB1.C / EnergyScanEJ199.C
//
// Corner mapping: simulation corners 0=(+,+) 1=(+,-) 2=(-,+) 3=(-,-) are
// filled into the experiment's slots as caps 4,5,6,7 = TL,TR,BL,BR so that the
// diagonals {0,3} and {1,2} land on the experiment's diagonals (4,7) and (5,6).

#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TProfile.h"
#include "TF1.h"
#include "TCanvas.h"
#include "TRandom3.h"
#include "TSystem.h"
#include "TStyle.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>

// ---------------------------------------------------------------------------
// VERBATIM from radical-t10-2026/macros/EnergyScan.C (constants and functions)
// ---------------------------------------------------------------------------
static const int NSLOT = 18, NSAMP = 1024;
static const double MV2ADC = 4.095;
static const int BASE_MOD = 40, BASE_CTR = 200;
static const int LGs[4] = {4,5,6,7}, HGs[4] = {14,13,16,15};
static const double SRCFD_FRAC = 0.15, THR_MIN = 20.0 * MV2ADC;

struct Pulse { double base, amp; int pkS; };
static Pulse pulseOf(const float *w, int pol, int baseEnd)
{
  Pulse r; r.base = 0;
  for (int s = 0; s < baseEnd; ++s) r.base += w[s];
  r.base /= baseEnd;
  float pkV = w[0]; r.pkS = 0;
  for (int s = 0; s < NSAMP; ++s) { float v = w[s];
    if (pol > 0 ? v > pkV : v < pkV) { pkV = v; r.pkS = s; } }
  r.amp = (pol > 0 ? pkV - r.base : r.base - pkV) * MV2ADC;
  return r;
}
static double leTime(const float *w, const float *tax, int pol, double baseMV, double thrADC)
{
  const double thr = pol > 0 ? baseMV + thrADC / MV2ADC : baseMV - thrADC / MV2ADC;
  for (int s = BASE_MOD; s < NSAMP; ++s)
    if (pol > 0 ? w[s] >= thr : w[s] <= thr) {
      double v0 = w[s-1], v1 = w[s];
      if (v1 == v0) return tax[s];
      return tax[s-1] + (thr - v0) / (v1 - v0) * (tax[s] - tax[s-1]);
    }
  return -1e9;
}
static double tebSigma(std::vector<double> &v, double *errOut = nullptr)
{
  if (v.size() < 50) return -1;
  double rc = 0, rw = 0;
  { double s=0,s2=0; for (double x : v){s+=x;s2+=x*x;}
    rc=s/v.size(); rw=std::sqrt(std::max(0.0,s2/v.size()-rc*rc)); }
  long nC = v.size();
  for (int it = 0; it < 5; ++it) {
    double s=0,s2=0; long n=0;
    for (double x : v) if (std::fabs(x-rc) < 2.5*rw){s+=x;s2+=x*x;++n;}
    if (n < 30) break;
    rc=s/n; rw=std::sqrt(std::max(0.0,s2/n-rc*rc)); nC=n;
  }
  if (!(rw > 1e-6) || !std::isfinite(rc) || !std::isfinite(rw)) return -1;
  const double robust = rw*1000.0/0.9546;
  TH1F h("teb","",120,rc-4*rw,rc+4*rw);
  for (double x : v) h.Fill(x);
  TF1 g("g","gaus",rc-2*rw,rc+2*rw);
  g.SetParameters(h.GetMaximum(),rc,rw);
  h.Fit(&g,"QRN");
  double gf = 1000.0*std::fabs(g.GetParameter(2)), ge = 1000.0*g.GetParError(2);
  bool useG = gf > 0.5*robust && gf < 2.0*robust;
  if (errOut) *errOut = useG ? ge : robust/std::sqrt(2.0*nC);
  return useG ? gf : robust;
}
// ---------------------------------------------------------------------------
// end of the verbatim block
// ---------------------------------------------------------------------------

// on-module gates SMIN [ADC-eq] per material and energy, from EnergyScan*.C
// (LuAG: 60*E+120 except the 11 GeV override; DSB1/EJ199: measured floors)
static double sminOf(const std::string& mat, double E) {
  const double Es[6] = {1,3,5,7,9,11};
  const double luag[6]  = {180, 300, 420, 540, 660, 3800};
  const double dsb1[6]  = {180, 1500, 2500, 4000, 6000, 6500};
  const double ej199[6] = {180, 500, 1200, 1700, 2200, 2500};
  for (int i = 0; i < 6; ++i) if (std::fabs(E - Es[i]) < 0.1)
    return mat == "luag" ? luag[i] : mat == "dsb1" ? dsb1[i] : ej199[i];
  return 60.0*E + 120;
}

struct Kernels { std::vector<double> hg, lg; };
static Kernels readKernels(const char* fn) {
  Kernels k; std::ifstream in(fn); std::string l;
  while (std::getline(in, l)) { if (l.empty() || l[0] == '#') continue;
    std::istringstream ss(l); double t, a, b; ss >> t >> a >> b; k.hg.push_back(a); k.lg.push_back(b); }
  return k;
}

// The emulated digitiser record for one event: 18 slots x 1024 samples in mV,
// exactly the layout of the data's "channel" branch (slots 4-7 low gain,
// 13-16 high gain; the rest stay empty).
struct Emulator {
  Kernels K; double kHG = 1, kLG = 1, noiseHG = 6.6, noiseLG = 1.6, wallMV = 780.;
  double tOnset = 0;            // photon time (ns) mapped to sample 22
  int lgShift = -10;            // low-gain chain 2.0 ns earlier than high gain
  double invF = 1.0;            // 1 / light scale of the file
  TRandom3 rnd{20260914};
  float ch[NSLOT][NSAMP]; float tax[2][NSAMP];
  Emulator() { for (int s = 0; s < NSAMP; ++s) tax[0][s] = tax[1][s] = 0.2*s; }
  void build(const std::vector<double>& phT, const std::vector<double>& phId) {
    static double cnt[4][NSAMP];
    for (int c = 0; c < 4; ++c) for (int s = 0; s < NSAMP; ++s) cnt[c][s] = 0;
    for (size_t i = 0; i < phT.size(); ++i) {
      int c = (int)phId[i]; if (c < 0 || c > 3) continue;
      int s = 22 + (int)std::lround((phT[i] - tOnset) / 0.2);
      if (s >= 0 && s < NSAMP) cnt[c][s] += 1;
    }
    for (int s = 0; s < NSLOT; ++s) for (int i = 0; i < NSAMP; ++i) ch[s][i] = 0;
    const int nk = (int)K.hg.size();
    for (int c = 0; c < 4; ++c) {
      static double yH[NSAMP], yL[NSAMP];
      for (int i = 0; i < NSAMP; ++i) { yH[i] = 0; yL[i] = 0; }
      for (int s = 0; s < NSAMP; ++s) { if (cnt[c][s] == 0) continue; const double n = cnt[c][s] * invF;
        const int jmax = std::min(nk, NSAMP - s);
        for (int j = 0; j < jmax; ++j) yH[s+j] += n * K.hg[j];
        const int jmaxL = std::min(nk, NSAMP - s - lgShift);
        for (int j = std::max(0, -(s+lgShift)); j < jmaxL; ++j) yL[s+lgShift+j] += n * K.lg[j]; }
      // ADC-eq -> mV, noise, clip; data convention: positive pulses on a baseline
      const double bH = -300., bL = -330.;
      for (int i = 0; i < NSAMP; ++i) {
        double h = bH + kHG*yH[i]/MV2ADC + rnd.Gaus(0, noiseHG);
        if (h > bH + wallMV) h = bH + wallMV;
        ch[HGs[c]][i] = (float)h;
        ch[LGs[c]][i] = (float)(bL + kLG*yL[i]/MV2ADC + rnd.Gaus(0, noiseLG));
      }
    }
  }
};

struct Line { double E; long nOn; double pk, pke, sg, sge, tMean, tMeanE, tMed, tMedE, dMeanP, dMeanPE, dMeanR, dMeanRE, dIntr, dIntrE, phIntr, phIntrE; long n4; double onFrac; };

static bool onePoint(const std::string& mat, double E, const char* fn, Emulator& em, const TString& plots, Line& L, bool calibOnly, double* meanLGpk, double* meanHGpk, double* meanNpe)
{
  TFile* f = TFile::Open(fn); if (!f || f->IsZombie()) return false;
  TTree* t = (TTree*)f->Get("ev"); if (!t) return false;
  std::vector<double> *phT = nullptr, *phId = nullptr, *t05 = nullptr; double Npe = 0;
  t->SetBranchAddress("phT", &phT); t->SetBranchAddress("phId", &phId); t->SetBranchAddress("t05Up", &t05); t->SetBranchAddress("Npe", &Npe);
  const Long64_t nEnt = t->GetEntries();
  // light onset for the placement: 1% quantile of all photon times in the file
  { std::vector<double> all; for (Long64_t i = 0; i < nEnt; ++i) { t->GetEntry(i); for (double v : *phT) all.push_back(v); }
    if (all.empty()) return false; std::sort(all.begin(), all.end()); em.tOnset = all[all.size()/100]; }
  const double SMIN = sminOf(mat, E);

  // ---- pass 1 (as EnergyScan.C): wall + transfer per capillary; every event is a beam event
  TH1F *hA[4]; TH2F *hHL[4];
  for (int j = 0; j < 4; ++j) { hA[j] = new TH1F(Form("hA%d",j),"",128,0,3200); hHL[j] = new TH2F(Form("hL%d",j),"",120,0,1200,128,0,3200); }
  double sLG = 0, sHG = 0, sN = 0; long nCal = 0;
  for (Long64_t i = 0; i < nEnt; ++i) {
    t->GetEntry(i); em.build(*phT, *phId);
    double Scal = 0; Pulse lv[4], hv[4];
    for (int j = 0; j < 4; ++j) { lv[j] = pulseOf(em.ch[LGs[j]],+1,BASE_MOD); hv[j] = pulseOf(em.ch[HGs[j]],+1,BASE_MOD); Scal += lv[j].amp; sLG += lv[j].amp; sHG += hv[j].amp; }
    sN += Npe; ++nCal;
    if (Scal > 300) for (int j = 0; j < 4; ++j) { hA[j]->Fill(hv[j].amp); hHL[j]->Fill(lv[j].amp, hv[j].amp); }
  }
  if (meanLGpk) *meanLGpk = sLG/nCal; if (meanHGpk) *meanHGpk = sHG/nCal; if (meanNpe) *meanNpe = sN/nCal;
  if (calibOnly) { for (int j = 0; j < 4; ++j) { delete hA[j]; delete hHL[j]; } f->Close(); return true; }
  double wall[4], a[4], b[4];
  for (int j = 0; j < 4; ++j) {
    double q = 0.995; hA[j]->GetQuantiles(1, &wall[j], &q);
    if (wall[j] < 2900) wall[j] = 3150;
    TProfile *pr = hHL[j]->ProfileX(Form("p%d",j));
    double lgMax = 1200, ceil_ = 0.72*wall[j];
    for (int bb = pr->FindBin(60); bb <= pr->GetNbinsX(); ++bb)
      if (pr->GetBinEntries(bb) > 3 && pr->GetBinContent(bb) > ceil_) { lgMax = pr->GetBinCenter(bb); break; }
    TF1 fl("fl","pol1",30,lgMax); pr->Fit(&fl,"QRN","",30,lgMax);
    a[j] = fl.GetParameter(0); b[j] = fl.GetParameter(1);
  }
  printf("   %s %.0f GeV: transfer HG = a + b*LG: b = %.2f/%.2f/%.2f/%.2f, walls %.0f/%.0f/%.0f/%.0f, SMIN %.0f\n", mat.c_str(), E, b[0],b[1],b[2],b[3], wall[0],wall[1],wall[2],wall[3], SMIN);

  // ---- pass 2 (as EnergyScan.C + DiagDiff.C): spectrum, srCFD times
  TH1F* hS = new TH1F(Form("hS_%s_%.0f",mat.c_str(),E), ";#Sigma LG [ADC-eq];events", 320, 0, 16000);
  std::vector<double> dtA, dtMed, dMeanP, dMeanR, dDiag, phDiag;
  long nOnMod = 0;
  TProfile mH("mH","",1000,-20,180), mL("mL","",1000,-20,180);   // mean emulated shapes, HG-peak aligned
  for (Long64_t i = 0; i < nEnt; ++i) {
    t->GetEntry(i); em.build(*phT, *phId);
    double S = 0; for (int j = 0; j < 4; ++j) S += pulseOf(em.ch[LGs[j]],+1,BASE_MOD).amp;
    if (S <= SMIN) continue;
    hS->Fill(S); ++nOnMod;
    const double t1 = 0.2*22 + (0.0);                   // PERFECT reference: the event time on the grid
    const double t1r = t1 + em.rnd.Gaus(0, 0.110);      // + emulated MCP/digitiser floor (110 ps)
    double ts = 0; int nOK = 0; std::vector<double> tcv; double tc[4]; bool all4 = true;
    for (int j = 0; j < 4; ++j) {
      Pulse l = pulseOf(em.ch[LGs[j]],+1,BASE_MOD), h = pulseOf(em.ch[HGs[j]],+1,BASE_MOD);
      double thr = SRCFD_FRAC * (a[j] + b[j]*l.amp);
      if (thr < THR_MIN || thr > 0.9*wall[j] || h.amp < thr) { all4 = false; continue; }
      double tcj = leTime(em.ch[HGs[j]], em.tax[1], +1, h.base, thr);
      if (tcj > -1e8) { ts += tcj; ++nOK; tcv.push_back(tcj); tc[j] = tcj; } else all4 = false;
      if (h.amp > 800 && h.amp < 2800 && h.pkS > 60 && h.pkS < 700) {
        for (int s = -100; s < 900; ++s) { int k = h.pkS + s; if (k < 0 || k >= NSAMP) continue;
          mH.Fill(s*0.2+0.1, (em.ch[HGs[j]][k]-h.base)*MV2ADC/h.amp); if (l.amp > 60) mL.Fill(s*0.2+0.1, (em.ch[LGs[j]][k]-l.base)*MV2ADC/l.amp); } }
    }
    if (nOK >= 2) { dtA.push_back(ts/nOK - t1r); std::sort(tcv.begin(), tcv.end()); dtMed.push_back(tcv[tcv.size()/2] - t1r); }
    if (all4 && nOK == 4) {
      dMeanP.push_back(0.25*(tc[0]+tc[1]+tc[2]+tc[3]) - t1);
      dMeanR.push_back(0.25*(tc[0]+tc[1]+tc[2]+tc[3]) - t1r);
      dDiag.push_back(0.5*(tc[0]+tc[3]) - 0.5*(tc[1]+tc[2]));
      if (t05->size() == 4 && (*t05)[0] > -999 && (*t05)[1] > -999 && (*t05)[2] > -999 && (*t05)[3] > -999)
        phDiag.push_back(0.5*((*t05)[0]+(*t05)[3]) - 0.5*((*t05)[1]+(*t05)[2]));
    }
  }
  L.E = E; L.nOn = nOnMod; L.onFrac = nEnt ? double(nOnMod)/nEnt : 0; L.n4 = (long)dDiag.size();
  // ---- peak fit exactly as EnergyScan.C
  L.pk = L.pke = L.sg = L.sge = 0;
  if (hS->GetEntries() >= 50) {
    int pb = hS->FindBin(SMIN + 60); double pv = 0; int pbb = pb;
    for (int bb = pb; bb <= hS->GetNbinsX(); ++bb) if (hS->GetBinContent(bb) > pv) { pv = hS->GetBinContent(bb); pbb = bb; }
    double m = hS->GetBinCenter(pbb), s = 0.35*m;
    TF1* g = new TF1(Form("g_%s_%.0f",mat.c_str(),E), "gaus", m-2*s, m+2*s);
    g->SetParameters(pv, m, s); g->SetParLimits(1, SMIN + 30, 13400); g->SetParLimits(2, 25, 0.8*m);
    for (int it = 0; it < 3; ++it) { hS->Fit(g, "QNR", "", std::max(SMIN, m-1.7*s), m+1.7*s); m = g->GetParameter(1); s = std::fabs(g->GetParameter(2)); }
    hS->Fit(g, "QR", "", std::max(SMIN, m-1.7*s), m+1.7*s);
    L.pk = g->GetParameter(1); L.pke = g->GetParError(1); L.sg = std::fabs(g->GetParameter(2)); L.sge = g->GetParError(2);
    TCanvas c("c","",700,500); hS->Draw("hist"); g->Draw("same"); c.SaveAs(Form("%s/SumLG_%s_E%.0f.png", plots.Data(), mat.c_str(), E));
  }
  double e;
  L.tMean = tebSigma(dtA, &e); L.tMeanE = e; L.tMed = tebSigma(dtMed, &e); L.tMedE = e;
  L.dMeanP = tebSigma(dMeanP, &e); L.dMeanPE = e; L.dMeanR = tebSigma(dMeanR, &e); L.dMeanRE = e;
  double sD = tebSigma(dDiag, &e); L.dIntr = sD > 0 ? sD/2 : -1; L.dIntrE = e/2;
  double sP = tebSigma(phDiag, &e); L.phIntr = sP > 0 ? sP/2 : -1; L.phIntrE = e/2;
  { TCanvas c("c2","",700,500); TH1F hd("hd", Form("diagonal difference %s %.0f GeV;(t_{TL}+t_{BR})/2 - (t_{TR}+t_{BL})/2  [ns];events", mat.c_str(), E), 100, -2, 2);
    for (double v : dDiag) hd.Fill(v); hd.Draw("hist"); c.SaveAs(Form("%s/DiagDiff_%s_E%.0f.png", plots.Data(), mat.c_str(), E)); }
  { FILE* o = fopen(Form("%s/meanshape_%s_E%.0f.txt", plots.Data(), mat.c_str(), E), "w");
    fprintf(o, "# t_ns(rel HG peak)  HG/peak  LG/peak   emulated mean pulse shapes, same selection as the data's meanshape (%s %.0f GeV)\n", mat.c_str(), E);
    for (int bb = 1; bb <= mH.GetNbinsX(); ++bb) fprintf(o, "%8.2f %9.5f %9.5f\n", mH.GetBinCenter(bb), mH.GetBinContent(bb), mL.GetBinContent(bb)); fclose(o); }
  for (int j = 0; j < 4; ++j) { delete hA[j]; delete hHL[j]; }
  f->Close();
  return true;
}

void tb26_emulate(const char* base = "build/rootfiles", double lightScale = 1.0, double kHG = -1, double kLG = -1)
{
  gStyle->SetOptStat(0);
  const std::vector<std::string> MATS = {"dsb1", "luag", "ej199"};
  const std::vector<double> ENERGIES = {1, 3, 5, 7, 9, 11};
  TString PLOTS = base; PLOTS.ReplaceAll("rootfiles", "plots"); if (PLOTS == base) PLOTS += "/plots"; PLOTS += "/emulate";
  gSystem->mkdir(PLOTS, true);
  Emulator em; em.K = readKernels("analysis/response_kernels.txt");
  if (em.K.hg.empty()) { printf("analysis/response_kernels.txt not found — run from Aug26TestBeam/\n"); return; }
  em.invF = 1.0/lightScale;
  printf("tb26_emulate: light scale of the files %.3g (amplitudes x %.3g)%s\n", lightScale, em.invF,
         lightScale < 0.999 ? "  [THINNED FILES: photon-statistics noise is wrong by sqrt(1/f); pipeline test only]" : "");

  // ---- gain calibration: two numbers (ADC-equivalent per photon, one per chain).
  // They belong to the ELECTRONICS, not to the material — the same card at the
  // same 29 V bias read every run from 13 onward — so ANY material's 5 GeV point
  // can anchor them, and every other material, energy and width is then a
  // prediction. Measured 5 GeV anchors (Sum-LG Gaussian peak, ADC-equivalent,
  // and mean transfer slope HG/LG):
  //   LuAG  run 15: 2962 +/- 37   slope 2.69   (EnergyScan_summary, TransferFit)
  //   DSB1  run 37: 6769 +/- 138  slope 2.91
  //   EJ199 run 41: 2740 +/- 74   slope 2.91 (slope not separately fitted)
  // Preference order dsb1 > luag > ej199 only because DSB1's 5 GeV run (37) is
  // the cleanest of the three; the choice does not change the physics.
  if (kHG < 0 || kLG < 0) {
    struct Anchor { const char* mat; double peak, slope; };
    const Anchor ANCH[3] = { {"dsb1", 6769., 2.91}, {"luag", 2962., 2.69}, {"ej199", 2740., 2.91} };
    std::string cm; double cE = 5, peak = 0, slope = 0; TString fn;
    for (const auto& a : ANCH) {
      TString g = Form("%s/%s/E5GeV.root", base, a.mat);
      if (gSystem->AccessPathName(g)) continue;
      fn = g; cm = a.mat; peak = a.peak; slope = a.slope; break;
    }
    if (cm.empty()) { printf("gain calibration needs a 5 GeV file for one of dsb1/luag/ej199 under %s — none found.\n"
                             "Pass the gains explicitly instead: tb26_emulate(base, lightScale, kHG, kLG)\n", base); return; }
    em.kHG = 1; em.kLG = 1; Line dummy; double mLG, mHG, mN;
    if (!onePoint(cm, cE, fn, em, PLOTS, dummy, true, &mLG, &mHG, &mN)) { printf("no file for calibration\n"); return; }
    // mLG is the MEAN Sum-LG over all events at unit gain; the measured anchor is
    // the PEAK of the on-module distribution. They differ by the miss/leakage
    // tail, taken as 0.85 in this first pass (the printed transfer slope and the
    // fitted peak in the table below are the checks on that factor).
    if (kLG < 0) kLG = 0.85*peak / mLG;
    if (kHG < 0) kHG = slope * kLG * (mLG/mHG);
    printf("gain anchor: %s 5 GeV (%s), <Npe> %.0f, mean Sum-LG %.4g and Sum-HG %.4g at unit gain\n"
           "  -> anchored to the measured %s peak %.0f ADC-eq and slope %.2f:  k_LG = %.4g, k_HG = %.4g ADC-eq per photon\n"
           "  Every other material, every other energy and every width below is a PREDICTION.\n",
           cm.c_str(), fn.Data(), mN, mLG, mHG, cm.c_str(), peak, slope, kLG, kHG);
    if (mN < 2000)
      printf("  [!] <Npe> = %.0f is far below true light — is this a thinned smoke file? The gains will be wrong by that factor.\n", mN);
  }
  em.kHG = kHG; em.kLG = kLG;

  for (const auto& mat : MATS) {
    std::vector<Line> lines; bool any = false;
    for (double E : ENERGIES) {
      TString fn = Form("%s/%s/E%.0fGeV.root", base, mat.c_str(), E);
      if (gSystem->AccessPathName(fn)) continue;
      Line L{}; if (!onePoint(mat, E, fn, em, PLOTS, L, false, nullptr, nullptr, nullptr)) continue;
      if (!any) { printf("\n=== %s: EnergyScan format (radical-t10-2026 Output/scan*/EnergyScan*_summary.txt) ===\n", mat.c_str()); any = true; }
      printf("%.0f GeV (sim): N(e,on-module) %ld (%.0f%% of events) | peak %.0f +/- %.0f, sigma %.0f => sigma/E %.1f +/- %.1f %% | t-MEAN %.0f +/- %.0f ps | t-MEDIAN %.0f +/- %.0f ps | diff %+.0f (N=%ld)   [110 ps reference jitter added]\n",
             E, L.nOn, 100*L.onFrac, L.pk, L.pke, L.sg, L.pk > 0 ? 100*L.sg/L.pk : 0,
             L.pk > 0 ? 100*L.sg/L.pk*std::sqrt(std::pow(L.sge/L.sg,2)+std::pow(L.pke/L.pk,2)) : 0,
             L.tMean, L.tMeanE, L.tMed, L.tMedE, L.tMed-L.tMean, L.nOn);
      lines.push_back(L);
    }
    if (!any) continue;
    printf("=== %s: DiagDiff format (Output/summary/DiagDiff_*.txt) — E  N4  sigma_meanIncl[ps] err  sigma_intr[ps] err  implied_ref[ps]   | perfect-ref mean  | photon-level 5%%-quantile intrinsic\n", mat.c_str());
    for (const Line& L : lines) {
      double ref = (L.dMeanR > 0 && L.dIntr > 0 && L.dMeanR > L.dIntr) ? std::sqrt(L.dMeanR*L.dMeanR - L.dIntr*L.dIntr) : -1;
      printf("%g  %ld  %.1f %.1f   %.1f %.1f   %.1f   | %.1f %.1f | %.1f %.1f\n", L.E, L.n4, L.dMeanR, L.dMeanRE, L.dIntr, L.dIntrE, ref, L.dMeanP, L.dMeanPE, L.phIntr, L.phIntrE);
    }
  }
  printf("\nplots and emulated mean pulse shapes: %s/\n", PLOTS.Data());
  printf("Compare line by line with radical-t10-2026/Output/scan*/EnergyScan*_summary.txt and Output/summary/DiagDiff_*.txt.\n");
}
