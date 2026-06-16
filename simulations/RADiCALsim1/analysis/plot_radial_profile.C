// plot_radial_profile.C — transverse shower profile vs. radial distance
// (azimuthal average of LateralProfile_Slice{0..5} 2D histograms)
//
// Reproduces the style of standard lateral shower profile reference plots
// (e.g. Amaldi et al.): y = dE/dA, x = radial distance from shower axis,
// one curve per depth slice.  Secondary x-axis shows Molière radii.
//
// Usage:
//   root -l -b -q 'analysis/plot_radial_profile.C("build/scan/radical_E120GeV.root",120)'
// Output: build/plots/transverse_radial_E<N>GeV.png

void plot_radial_profile(const char* fname = "build/scan/radical_E120GeV.root",
                          double Egev = 120) {
    gStyle->SetOptStat(0); gStyle->SetOptTitle(0);
    gStyle->SetPadGridX(1); gStyle->SetPadGridY(1);

    // ── Physical parameters ───────────────────────────────────────────────────
    // Molière radius for pure LYSO (Lu2SiO5:Ce), PDG 2024.
    // Effective R_M of the LYSO/W stack is slightly smaller (~14–17 mm), but we
    // quote the LYSO value since shower spreading primarily occurs in the scintillator.
    const double RM_mm = 20.9;   // mm

    // Depth of each depth slice (center of LYSO-layer range) in radiation lengths.
    // Slice s covers LYSO layers [5s, 5s+4] (Slice5: layers 25-28, 4 layers).
    // Center-layer depth (mm from upstream face): L × 4.4064 + 0.75
    // Effective X₀/mm for the LYSO/W stack: 0.845 X₀ per 4.4064 mm period = 0.1917 X₀/mm
    const int NS = 6;
    const double sliceX0[NS] = { 1.8, 6.1, 10.3, 14.5, 18.7, 22.5 };
    int cols[NS] = { kAzure+2, kRed+1, kGreen+2, kMagenta+1, kOrange+1, kCyan+2 };
    int mst[NS]  = { 24, 20, 25, 21, 26, 22 };   // open circle, filled circle, open sq, ...

    TFile* f = TFile::Open(fname);
    if (!f || f->IsZombie()) { printf("ERROR: cannot open %s\n", fname); return; }

    // ── Azimuthal-average radial profiles ─────────────────────────────────────
    // Project each H2 (x,y) → R = sqrt(x²+y²), normalize by ring area 2πR·ΔR,
    // and express radius in Molière radius units (R_M).
    // Limit to R < 7 mm (= 0.335 R_M) where azimuthal coverage is complete.
    const double Rmax_mm = 7.0;              // mm — full-coverage aperture radius
    const double Rmax    = Rmax_mm / RM_mm;  // in R_M
    const int    NR      = 35;
    const double dR      = Rmax / NR;        // R_M per bin (~0.0096 R_M)

    double gMax = 0, gMin = 1e30;
    TH1D* hR[NS] = {};

    for (int s = 0; s < NS; s++) {
        TH2D* h2 = (TH2D*)f->Get(Form("LateralProfile_Slice%d", s));
        if (!h2) { printf("WARNING: missing LateralProfile_Slice%d\n", s); continue; }

        hR[s] = new TH1D(Form("hR%d", s), "", NR, 0, Rmax);

        for (int ix = 1; ix <= h2->GetNbinsX(); ix++) {
            for (int iy = 1; iy <= h2->GetNbinsY(); iy++) {
                double x = h2->GetXaxis()->GetBinCenter(ix);
                double y = h2->GetYaxis()->GetBinCenter(iy);
                double R_mm = TMath::Sqrt(x*x + y*y);
                double w    = h2->GetBinContent(ix, iy);
                if (w > 0 && R_mm < Rmax_mm) hR[s]->Fill(R_mm / RM_mm, w);
            }
        }

        // Divide by ring area (in mm²) → dE/dA
        for (int b = 1; b <= NR; b++) {
            double Rc   = hR[s]->GetBinCenter(b) * RM_mm;   // back to mm for area
            double area = 2.0 * TMath::Pi() * Rc * (dR * RM_mm);
            double v = (area > 0 && hR[s]->GetBinContent(b) > 0)
                       ? hR[s]->GetBinContent(b) / area : 0;
            hR[s]->SetBinContent(b, v);
        }

        for (int b = 1; b <= NR; b++) {
            double v = hR[s]->GetBinContent(b);
            if (v > gMax) gMax = v;
            if (v > 0 && v < gMin) gMin = v;
        }
        printf("  Slice %d  (%.1f X₀): peak dE/dA = %.1f a.u.  first-bin R = %.2f mm\n",
               s, sliceX0[s], hR[s]->GetBinContent(1), hR[s]->GetBinCenter(1));
    }

    // ── Canvas ────────────────────────────────────────────────────────────────
    TCanvas* c = new TCanvas("cRad", "radial profile", 620, 700);
    c->SetLeftMargin(0.15);
    c->SetBottomMargin(0.13);
    c->SetTopMargin(0.18);
    c->SetRightMargin(0.06);
    c->SetLogy();
    c->SetGrid(1, 1);

    TLegend* leg = new TLegend(0.54, 0.43, 0.92, 0.84);
    leg->SetTextSize(0.034); leg->SetBorderSize(1); leg->SetFillColor(0);

    bool first = true;
    for (int s = 0; s < NS; s++) {
        if (!hR[s]) continue;
        hR[s]->SetMarkerColor(cols[s]); hR[s]->SetMarkerStyle(mst[s]); hR[s]->SetMarkerSize(0.9);
        hR[s]->SetLineColor(cols[s]);   hR[s]->SetLineWidth(1);
        hR[s]->GetXaxis()->SetTitle("Distance from shower axis  [R_{M}]");
        hR[s]->GetXaxis()->SetTitleSize(0.046); hR[s]->GetXaxis()->SetLabelSize(0.040);
        hR[s]->GetYaxis()->SetTitle("Energy deposit [a.u.]");
        hR[s]->GetYaxis()->SetTitleSize(0.046); hR[s]->GetYaxis()->SetLabelSize(0.040);
        hR[s]->GetYaxis()->SetTitleOffset(1.35);
        hR[s]->SetMaximum(gMax * 15.0);
        hR[s]->SetMinimum(gMin * 0.3);
        if (first) { hR[s]->Draw("P"); first = false; }
        else        hR[s]->Draw("P same");
        leg->AddEntry(hR[s], Form("%.1f X_{0}", sliceX0[s]), "p");
    }
    leg->Draw();

    // ── Inline depth labels ───────────────────────────────────────────────────
    TLatex lb; lb.SetNDC(); lb.SetTextSize(0.033); lb.SetTextColor(kGray+2);
    lb.DrawLatex(0.18, 0.34, "Early");        // 1.8 X0 — low, small R
    lb.DrawLatex(0.22, 0.78, "Shower max");   // 6.1 X0 — highest
    lb.DrawLatex(0.50, 0.52, "Tail");

    // ── Title ─────────────────────────────────────────────────────────────────
    TLatex tit; tit.SetNDC(); tit.SetTextFont(42); tit.SetTextSize(0.042);
    tit.DrawLatex(0.15, 0.975, Form("Transverse shower profile  (%.0f GeV e^{-})", Egev));

    // ── Note: mm equivalent of x-axis ────────────────────────────────────────
    TLatex note; note.SetNDC(); note.SetTextSize(0.030); note.SetTextColor(kGray+2);
    note.DrawLatex(0.15, 0.955, Form("R_{M}(LYSO) = %.0f mm  #rightarrow  0.33 R_{M} = %.0f mm", RM_mm, Rmax_mm));

    // ── Save ──────────────────────────────────────────────────────────────────
    gSystem->mkdir("build/plots", kTRUE);
    TString out = Form("build/plots/transverse_radial_E%.0fGeV.png", Egev);
    c->SaveAs(out.Data());
    printf("Saved: %s\n", out.Data());
    f->Close();
}
