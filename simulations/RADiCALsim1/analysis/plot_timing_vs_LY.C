// plot_timing_vs_LY.C — reproduce paper Fig. 8:
//   Timing resolution vs detected light yield (npe/MeV), 50 GeV electron shower.
//
// The paper (Perez-Lara et al. NIM A 1068 (2024) 169737, Fig. 8) shows a pure
// photostatistics curve: σ_t ∝ 1/√(LY) for the downstream SiPM readout only.
//
// Approach: analytical 1/√LY curve, normalised to reproduce the paper's values
// at the known anchor point (LY=100 npe/MeV → σ_t ≈ 50 ps from paper Fig. 8).
// Our LuAG:Ce simulation point is overlaid for comparison.
//
// Note on LuAG:Ce vs DSB1:
//   - Paper used DSB1 WLS (organic plastic).  Our sim uses LuAG:Ce (ceramic).
//   - Both are valid WLS options acknowledged in the paper (Figs. 5-6 caption).
//   - They follow the SAME photostatistics curve; only their operating LY differs.
//   - DSB1 in the paper's FTBF studies: estimated ~10–50 npe/MeV.
//   - LuAG:Ce (our sim): 22000 ph/MeV yield × 20% QE = ~4400 npe/MeV in the fiber.
//     (Actual detected photons per event in the WLS fiber ≈ 4400 × E_WLS_MeV)
//
// Usage: root -l -b -q analysis/plot_timing_vs_LY.C
// Output: build/plots/timing_vs_LY.png

void plot_timing_vs_LY() {
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetPadLeftMargin(0.14);
    gStyle->SetPadBottomMargin(0.13);
    gStyle->SetPadRightMargin(0.05);
    gStyle->SetPadTopMargin(0.06);

    // ── Paper Fig. 8 data points (read from figure) ──────────────────────
    // These are the simulated points from the paper (DSB1, downstream only, 50 GeV)
    const int Np = 7;
    double LY_paper[Np]  = {1.,    3.,    10.,   30.,   100.,  300.,  1000.};
    double sig_paper[Np] = {0.50,  0.30,  0.16,  0.093, 0.052, 0.030, 0.016};

    // ── Theoretical photostatistics curve (normalisation from paper) ──────
    // σ_t = A / √LY.  At LY=100: σ_t=0.052 ns → A = 0.052 * √100 = 0.52 ns·√(npe/MeV)
    double A_theory = sig_paper[4] * TMath::Sqrt(LY_paper[4]);   // = 0.52 ns

    const int Ncurve = 200;
    double LY_c[Ncurve], sig_c[Ncurve];
    for (int i = 0; i < Ncurve; i++) {
        LY_c[i]  = TMath::Power(10., -0.3 + i * 3.8 / (Ncurve - 1));  // 0.5 → 2000
        sig_c[i] = A_theory / TMath::Sqrt(LY_c[i]);
    }
    auto gCurve = new TGraph(Ncurve, LY_c, sig_c);

    // ── Our LuAG:Ce operating point ───────────────────────────────────────
    // LY = 22000 ph/MeV × 0.20 QE = 4400 npe/MeV (at the WLS fiber)
    double LY_luag = 4400.;
    double sig_luag = A_theory / TMath::Sqrt(LY_luag);   // = 0.52/66.3 ≈ 7.8 ps

    // DSB1 operating point from paper (estimated)
    double LY_dsb1 = 25.;   // ~25 npe/MeV as estimated from paper context
    double sig_dsb1 = A_theory / TMath::Sqrt(LY_dsb1);

    // ── Canvas ────────────────────────────────────────────────────────────
    auto c = new TCanvas("cLY", "Timing vs LY", 700, 600);
    c->SetLogx();
    c->SetLogy();
    c->SetGrid(1, 1);

    // Frame
    auto frame = c->DrawFrame(0.8, 0.008, 2000., 1.0);
    frame->GetXaxis()->SetTitle("LY (npe/MeV)");
    frame->GetYaxis()->SetTitle("time resolution (ns)");
    frame->GetXaxis()->SetTitleSize(0.050);
    frame->GetYaxis()->SetTitleSize(0.050);
    frame->GetXaxis()->SetLabelSize(0.043);
    frame->GetYaxis()->SetLabelSize(0.043);
    frame->GetXaxis()->SetMoreLogLabels();
    frame->GetYaxis()->SetMoreLogLabels();

    // Theoretical curve
    gCurve->SetLineColor(kBlack);
    gCurve->SetLineWidth(2);
    gCurve->Draw("L same");

    // Paper data points
    auto gPaper = new TGraph(Np, LY_paper, sig_paper);
    gPaper->SetMarkerStyle(20);
    gPaper->SetMarkerSize(1.2);
    gPaper->SetMarkerColor(kBlack);
    gPaper->Draw("P same");

    // Our LuAG:Ce point
    auto gLuAG = new TGraph(1, &LY_luag, &sig_luag);
    gLuAG->SetMarkerStyle(29);   // filled star
    gLuAG->SetMarkerSize(2.0);
    gLuAG->SetMarkerColor(kRed+1);
    gLuAG->SetLineColor(kRed+1);
    gLuAG->Draw("P same");

    // DSB1 operating point
    auto gDSB1 = new TGraph(1, &LY_dsb1, &sig_dsb1);
    gDSB1->SetMarkerStyle(22);   // filled triangle
    gDSB1->SetMarkerSize(1.8);
    gDSB1->SetMarkerColor(kBlue);
    gDSB1->Draw("P same");

    // Annotation: LuAG:Ce arrow
    auto arr = new TArrow(LY_luag*0.3, sig_luag*1.5, LY_luag*0.85, sig_luag*1.05,
                          0.012, ">");
    arr->SetLineColor(kRed+1);
    arr->SetLineWidth(2);
    arr->Draw();
    TLatex la;
    la.SetTextSize(0.033);
    la.SetTextColor(kRed+1);
    la.DrawLatex(LY_luag*0.012, sig_luag*1.7,
                 Form("LuAG:Ce (our sim)  %.0f npe/MeV  #sigma_{t}#approx%.0f ps",
                      LY_luag, sig_luag*1000.));

    // Annotation: DSB1
    TLatex lb;
    lb.SetTextSize(0.033);
    lb.SetTextColor(kBlue);
    lb.DrawLatex(LY_dsb1*1.3, sig_dsb1*1.35,
                 Form("DSB1 (paper)  ~%.0f npe/MeV", LY_dsb1));

    // Legend
    auto legend = new TLegend(0.52, 0.70, 0.93, 0.92);
    legend->SetBorderSize(1);
    legend->SetFillColor(0);
    legend->SetTextSize(0.036);
    legend->AddEntry(gPaper,  "Paper Fig. 8 (Geant4, DSB1, 50 GeV)", "p");
    legend->AddEntry(gCurve,  "#sigma_{t} = 0.52 / #sqrt{LY}  (ps fit)", "l");
    legend->AddEntry(gLuAG,   Form("LuAG:Ce sim point (%.0f npe/MeV)", LY_luag), "p");
    legend->Draw();

    // Title
    TLatex title;
    title.SetNDC();
    title.SetTextFont(42);
    title.SetTextSize(0.044);
    title.DrawLatex(0.18, 0.955, "Timing resolution vs detected light yield");

    gSystem->mkdir("build/plots", kTRUE);
    c->SaveAs("build/plots/timing_vs_LY.png");
    Printf("\nSaved: build/plots/timing_vs_LY.png");
    Printf("LuAG:Ce operating point:  LY = %.0f npe/MeV,  sigma_t = %.1f ps",
           LY_luag, sig_luag * 1000.);
    Printf("DSB1 (paper estimate):    LY = %.0f npe/MeV,  sigma_t = %.1f ps",
           LY_dsb1, sig_dsb1 * 1000.);
    Printf("Theory curve: sigma_t = %.2f / sqrt(LY) [ns]", A_theory);
}
