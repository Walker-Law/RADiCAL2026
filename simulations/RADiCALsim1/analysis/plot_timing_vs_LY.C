// plot_timing_vs_LY.C — reproduce paper Fig. 8:
//   Timing resolution vs detected light yield (npe/MeV), 50 GeV electron shower.
//
// The paper (Perez-Lara et al. NIM A 1068 (2024) 169737, Fig. 8) shows a pure
// photostatistics curve: σ_t ∝ 1/√(LY) for the downstream SiPM readout only.
//
// This macro overlays:
//   (1) The theoretical 1/√LY curve (calibrated to paper anchor at LY=100→52 ps)
//   (2) Paper Fig. 8 simulation points (DSB1, digitized from figure)
//   (3) Actual Geant4 sim points extracted from optical runs at 20/50/125 GeV
//       -- LY from H1[21] PhotonsDetected / 8 ends / E_beam_MeV
//       -- σ_t  from H1[6]  DeltaT RMS (spread across events, downstream−upstream)
//   (4) DSB1 estimated operating point
//
// Note on LuAG:Ce vs DSB1:
//   - Paper used DSB1 WLS (organic plastic).  Our sim uses LuAG:Ce (ceramic).
//   - Both follow the SAME photostatistics curve; only their operating LY differs.
//   - DSB1 in the paper's FTBF studies: estimated ~25 npe/MeV.
//   - LuAG:Ce (our sim): 22000 ph/MeV yield × 20% QE × 1/8-ends ~ few hundred npe/MeV.
//
// Usage: root -l -b -q analysis/plot_timing_vs_LY.C
//   Run from the RADiCALsim1/ directory, or from build/ with adjusted paths.
// Output: build/plots/timing_vs_LY.png

#include "TFile.h"
#include "TH1D.h"
#include "TGraph.h"
#include "TGraphErrors.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TMath.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TArrow.h"
#include "TSystem.h"

// ── Helper: extract LY [npe/MeV] and σ_t [ns] from an optical ROOT file ──────
// LY  = mean(PhotonsDetected) / (2 × mean(TotalCornerWLS_MeV))
//       = npe per SiPM end per MeV deposited in WLS material
//       (PhotonsDetected = all 8 ends, TotalCornerWLS = all 4 corners → factor of 2)
// σ_t = RMS of DeltaT histogram (optical downstream−upstream 1st-photon ΔT)
//       This includes geometric shower-depth spread and photon statistics.
//       Expect points to lie ABOVE the pure-photostatistics theory curve.
bool extractOptPoint(const char* fname, double& LY, double& sig_t, double& sig_t_err) {
    TFile* f = TFile::Open(fname);
    if (!f || f->IsZombie()) { Printf("ERROR: cannot open %s", fname); return false; }

    TH1D* hPh  = (TH1D*)f->Get("PhotonsDetected");
    TH1D* hDT  = (TH1D*)f->Get("DeltaT");
    TH1D* hWLS = (TH1D*)f->Get("TotalCornerWLS");
    if (!hPh || !hDT || !hWLS) {
        Printf("ERROR: missing histograms in %s", fname);
        f->Close(); return false;
    }

    double nEv       = hPh->GetEntries();
    double meanPhot  = hPh->GetMean();    // total photons (all 8 ends) per event
    double meanWLS   = hWLS->GetMean();   // total WLS energy all 4 corners (MeV/event)
    double nDT       = hDT->GetEntries();
    double rms       = hDT->GetRMS();     // ns — 1σ spread including geometric component

    f->Close();

    if (nEv < 1 || nDT < 2 || meanWLS < 1e-6 || rms < 1e-6) {
        Printf("WARNING: insufficient data in %s (nEv=%.0f nDT=%.0f meanWLS=%.3f)",
               fname, nEv, nDT, meanWLS);
        return false;
    }

    // LY: detected photons per SiPM-end per MeV of WLS energy deposited
    // PhotonsDetected / 8 ends / (TotalCornerWLS / 4 corners) = PhotonsDetected / (2 * TotalCornerWLS)
    LY    = meanPhot / (2.0 * meanWLS);
    sig_t = rms;
    // Uncertainty on σ from a sample: δσ ≈ σ/√(2N)
    sig_t_err = rms / TMath::Sqrt(2. * nDT);

    Printf("  %-40s  LY=%6.0f npe/MeV (WLS)   σ_t=%5.1f ± %.1f ps  (nEv=%.0f nDT=%.0f)",
           fname, LY, sig_t*1000., sig_t_err*1000., nEv, nDT);
    return true;
}

void plot_timing_vs_LY() {
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetPadLeftMargin(0.15);
    gStyle->SetPadBottomMargin(0.14);
    gStyle->SetPadRightMargin(0.06);
    gStyle->SetPadTopMargin(0.07);

    // ── Paper Fig. 8 data points (digitized from figure) ─────────────────────
    const int Np = 7;
    double LY_paper[Np]  = {1.,    3.,    10.,   30.,   100.,  300.,  1000.};
    double sig_paper[Np] = {0.50,  0.30,  0.16,  0.093, 0.052, 0.030, 0.016};

    // ── Theoretical photostatistics curve ─────────────────────────────────────
    // σ_t = A/√LY.  Anchor: LY=100 → σ_t=0.052 ns  (from paper Fig. 8)
    double A_theory = sig_paper[4] * TMath::Sqrt(LY_paper[4]);  // 0.52

    const int Nc = 300;
    double LY_c[Nc], sig_c[Nc];
    for (int i = 0; i < Nc; i++) {
        LY_c[i]  = TMath::Power(10., 0. + i * 4.0 / (Nc - 1));  // 1 → 10000 npe/MeV
        sig_c[i] = A_theory / TMath::Sqrt(LY_c[i]);
    }
    auto gCurve = new TGraph(Nc, LY_c, sig_c);

    // ── Actual Geant4 sim points from optical runs ────────────────────────────
    Printf("\nExtracting actual sim points from optical runs:");
    const int NS = 3;
    const char* optFiles[NS] = {
        "build/optical_timing/optical_E20GeV.root",
        "build/optical_timing/optical_E50GeV.root",
        "build/optical_timing/optical_E125GeV.root"
    };
    double LY_sim[NS], sig_sim[NS], sig_sim_err[NS];
    int    nSim = 0;
    bool   simOk[NS] = {false, false, false};

    for (int i = 0; i < NS; i++) {
        simOk[i] = extractOptPoint(optFiles[i],
                                   LY_sim[i], sig_sim[i], sig_sim_err[i]);
        if (simOk[i]) nSim++;
    }

    // Collect valid sim points
    std::vector<double> vLY, vSig, vErrLY, vErrSig;
    for (int i = 0; i < NS; i++) {
        if (simOk[i]) {
            vLY.push_back(LY_sim[i]);
            vSig.push_back(sig_sim[i]);
            vErrLY.push_back(0.);                  // no x-error shown
            vErrSig.push_back(sig_sim_err[i]);
        }
    }
    auto gSim = (vLY.size() > 0) ?
        new TGraphErrors(vLY.size(), vLY.data(), vSig.data(),
                         vErrLY.data(), vErrSig.data()) : nullptr;

    // ── DSB1 paper estimate ───────────────────────────────────────────────────
    double LY_dsb1 = 25.;
    double sig_dsb1 = A_theory / TMath::Sqrt(LY_dsb1);

    // ── Canvas ────────────────────────────────────────────────────────────────
    auto c = new TCanvas("cLY", "Timing vs LY", 760, 620);
    c->SetLogx();
    c->SetLogy();
    c->SetGrid(1, 1);

    // Frame: x from 1 to 10000, y from 5 ps to 1 ns
    auto frame = c->DrawFrame(1.0, 0.005, 10000., 1.2);
    frame->GetXaxis()->SetTitle("LY (npe/MeV)");
    frame->GetYaxis()->SetTitle("time resolution (ns)");
    frame->GetXaxis()->SetTitleSize(0.052);
    frame->GetYaxis()->SetTitleSize(0.052);
    frame->GetXaxis()->SetLabelSize(0.044);
    frame->GetYaxis()->SetLabelSize(0.044);
    frame->GetXaxis()->SetTitleOffset(1.05);
    frame->GetYaxis()->SetTitleOffset(1.25);
    // Clean log axis labeling: major powers only, no cluttered sub-ticks
    frame->GetXaxis()->SetNdivisions(510);
    frame->GetYaxis()->SetNdivisions(505);
    frame->GetXaxis()->SetMoreLogLabels(kFALSE);
    frame->GetYaxis()->SetMoreLogLabels(kFALSE);
    frame->GetXaxis()->SetNoExponent(kFALSE);

    // Theoretical curve
    gCurve->SetLineColor(kBlack);
    gCurve->SetLineWidth(2);
    gCurve->Draw("L same");

    // Paper data points
    auto gPaper = new TGraph(Np, LY_paper, sig_paper);
    gPaper->SetMarkerStyle(20);
    gPaper->SetMarkerSize(1.3);
    gPaper->SetMarkerColor(kBlack);
    gPaper->Draw("P same");

    // Actual Geant4 sim points (with error bars on σ_t)
    if (gSim && vLY.size() > 0) {
        gSim->SetMarkerStyle(29);      // filled star
        gSim->SetMarkerSize(2.2);
        gSim->SetMarkerColor(kRed+1);
        gSim->SetLineColor(kRed+1);
        gSim->SetLineWidth(2);
        gSim->Draw("P same");
    }

    // DSB1 operating point
    auto gDSB1 = new TGraph(1, &LY_dsb1, &sig_dsb1);
    gDSB1->SetMarkerStyle(22);
    gDSB1->SetMarkerSize(1.8);
    gDSB1->SetMarkerColor(kBlue);
    gDSB1->Draw("P same");

    // ── Annotations (placed inside axes) ─────────────────────────────────────
    TLatex la;
    la.SetTextSize(0.034);
    la.SetTextColor(kBlue);
    la.DrawLatex(3.5, sig_dsb1 * 0.60,
                 Form("DSB1 (paper) ~%.0f npe/MeV", LY_dsb1));

    // Label each Geant4 sim point by beam energy.
    // All three points cluster at ~100 ps; stagger x and y to avoid overlap.
    const char* simLabels[NS] = {"20 GeV", "50 GeV", "125 GeV"};
    // Vertical/horizontal stagger to avoid overlap (log x-scale, clustered y)
    double xOff[NS] = {1.35, 1.35, 1.35};
    double yOff[NS] = {1.60, 0.62, 1.65};   // 20 up-right, 50 below-right, 125 above-right
    for (int i = 0; i < NS; i++) {
        if (!simOk[i]) continue;
        TLatex lb;
        lb.SetTextSize(0.030);
        lb.SetTextColor(kRed+1);
        lb.DrawLatex(LY_sim[i] * xOff[i], sig_sim[i] * yOff[i], simLabels[i]);
    }

    // Note at top-left inside plot area (above the theory curve, below title)
    TLatex note;
    note.SetNDC();
    note.SetTextSize(0.028);
    note.SetTextColor(kGray+2);
    note.DrawLatex(0.17, 0.88,
        "#star above curve: sim #sigma_{t} includes geometric shower-depth spread");

    // ── Legend ────────────────────────────────────────────────────────────────
    double legX1 = 0.42, legY1 = 0.14, legX2 = 0.92, legY2 = 0.38;
    auto legend = new TLegend(legX1, legY1, legX2, legY2);
    legend->SetBorderSize(1);
    legend->SetFillColor(0);
    legend->SetTextSize(0.036);
    legend->AddEntry(gPaper,  "Paper Fig. 8 (Geant4, DSB1, 50 GeV)", "p");
    legend->AddEntry(gCurve,  "#sigma_{t} = 0.52 / #sqrt{LY}  (ns)", "l");
    if (gSim && vLY.size() > 0)
        legend->AddEntry(gSim, "This sim: LuAG:Ce optical (25 ev/E)", "p");
    legend->AddEntry(gDSB1,   Form("DSB1 est. ~%.0f npe/MeV (paper)", LY_dsb1), "p");
    legend->Draw();

    // Title
    TLatex title;
    title.SetNDC();
    title.SetTextFont(42);
    title.SetTextSize(0.046);
    title.DrawLatex(0.17, 0.960, "Timing resolution vs detected light yield");

    gSystem->mkdir("build/plots", kTRUE);
    c->SaveAs("build/plots/timing_vs_LY.png");
    Printf("\nSaved: build/plots/timing_vs_LY.png");

    // Summary table
    Printf("\n%-10s  %-16s  %-12s  %-14s  %-12s",
           "File", "LY (npe/MeV)", "σ_t sim (ps)", "Theory σ_t (ps)", "Geom excess");
    const char* eLabels[NS] = {"20 GeV","50 GeV","125 GeV"};
    for (int i = 0; i < NS; i++) {
        if (!simOk[i]) continue;
        double theory = A_theory / TMath::Sqrt(LY_sim[i]) * 1000.;
        double excess = TMath::Sqrt(TMath::Max(0., sig_sim[i]*sig_sim[i]*1e6 - theory*theory));
        Printf("%-10s  %-16.0f  %-12.1f  %-14.1f  %-12.1f ps (geom)",
               eLabels[i], LY_sim[i], sig_sim[i]*1000., theory, excess);
    }
    Printf("DSB1:  LY=%.0f npe/MeV  σ_t=%.1f ps (theory)", LY_dsb1, sig_dsb1*1000.);
}
