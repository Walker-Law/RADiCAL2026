// plot_lateral_profile.C — transverse (radial) shower profile vs energy.
//
//   root -l -b -q analysis/plot_lateral_profile.C
//
// Reads H2[2] LateralProfile (x,y in mm, weighted by LYSO energy deposit)
// from each scan file. Converts to a radial profile: r = sqrt(x^2+y^2).
// Normalizes each profile to unit integral (fractional energy in ring dr).
//
// Key physics context:
//   Molière radius for LYSO:  R_M(LYSO) ≈ 20 mm
//   Module half-width:        7 mm (half of 14 mm tile)
//   Max inscribed radius:     7 mm   (perpendicular to edge)
//   Max diagonal radius:      9.9 mm (corner)
//   → ~68% containment from R_M definition; 32% leaks laterally if module = 1 R_M
//   → At r > 7 mm the profile is cut by the module boundary.
//
// Output:
//   build/plots/lateral_profile_radial.png  — radial profiles, all energies
//   build/plots/lateral_profile_2D.png      — 2D map at highest energy

#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TLine.h"
#include "TSystem.h"
#include "TMath.h"
#include "TColor.h"

void plot_lateral_profile() {
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);

    const int N = 6;
    double E[N] = {5, 10, 20, 50, 100, 120};
    int    cols[N] = {kRed+1, kOrange+1, kGreen+2, kAzure+2, kBlue+1, kMagenta+1};

    // Radial bins: 0 to 9.9 mm (diagonal of 7mm half-tile) in 0.2 mm steps
    const int nRbins = 50;
    const double rMax = 10.0;

    TCanvas* cR = new TCanvas("cR","radial",800,600);
    cR->SetLeftMargin(0.15);
    cR->SetRightMargin(0.06);
    cR->SetBottomMargin(0.14);
    cR->SetTopMargin(0.08);
    cR->SetGrid(1,1);

    TLegend* leg = new TLegend(0.58, 0.45, 0.92, 0.88);
    leg->SetBorderSize(1);
    leg->SetFillColor(0);
    leg->SetTextSize(0.036);

    TH1D* hFirst = nullptr;

    printf("\n  E_beam   Total edep   r_mean   r_rms   frac_r<5mm   frac_r<7mm\n");
    printf("  ------------------------------------------------------------------\n");

    for (int i = 0; i < N; i++) {
        TFile* f = TFile::Open(Form("build/scan/radical_E%.0fGeV.root", E[i]));
        if (!f || f->IsZombie()) continue;

        TH2D* h2 = (TH2D*)f->Get("LateralProfile");
        if (!h2) { f->Close(); continue; }

        // Build radial profile by iterating over H2 bins.
        // SetDirectory(nullptr): detach from TFile so f->Close() won't delete it.
        TH1D* hRad = new TH1D(Form("hRad_%d", i),
                               Form("%.0f GeV", E[i]),
                               nRbins, 0., rMax);
        hRad->SetDirectory(nullptr);

        double totalW = 0, wR = 0, wR2 = 0;
        double sumInner5 = 0, sumInner7 = 0;

        for (int ix = 1; ix <= h2->GetNbinsX(); ix++) {
            for (int iy = 1; iy <= h2->GetNbinsY(); iy++) {
                double x = h2->GetXaxis()->GetBinCenter(ix);
                double y = h2->GetYaxis()->GetBinCenter(iy);
                double r = TMath::Sqrt(x*x + y*y);
                double w = h2->GetBinContent(ix, iy);
                if (w <= 0.) continue;
                hRad->Fill(r, w);
                totalW += w;
                wR  += w * r;
                wR2 += w * r * r;
                if (r < 5.) sumInner5 += w;
                if (r < 7.) sumInner7 += w;
            }
        }

        if (totalW < 1e-9) { f->Close(); continue; }

        // Divide by 2πr dr (annular area element) so plot shows energy density vs r
        for (int ir = 1; ir <= nRbins; ir++) {
            double r    = hRad->GetBinCenter(ir);
            double dr   = hRad->GetBinWidth(ir);
            double area = 2. * TMath::Pi() * r * dr;
            if (area > 0) {
                hRad->SetBinContent(ir, hRad->GetBinContent(ir) / area);
                hRad->SetBinError(ir,   hRad->GetBinError(ir)   / area);
            }
        }

        // Normalize to unit integral in linear r space (density integrates to 1)
        double integ = 0;
        for (int ir = 1; ir <= nRbins; ir++) {
            double r  = hRad->GetBinCenter(ir);
            double dr = hRad->GetBinWidth(ir);
            integ += hRad->GetBinContent(ir) * 2. * TMath::Pi() * r * dr;
        }
        if (integ > 0) hRad->Scale(1./integ);

        double rMean = wR / totalW;
        double rRMS  = TMath::Sqrt(TMath::Max(0., wR2/totalW - rMean*rMean));

        printf("  %5.0f   %10.2f  %7.2f  %7.2f   %9.3f    %9.3f\n",
               E[i], totalW, rMean, rRMS,
               (totalW>0 ? sumInner5/totalW : 0.),
               (totalW>0 ? sumInner7/totalW : 0.));

        hRad->SetLineColor(cols[i]);
        hRad->SetLineWidth(2);
        hRad->GetXaxis()->SetTitle("r (mm)");
        hRad->GetYaxis()->SetTitle("Energy density (normalized, mm^{-2})");
        hRad->GetXaxis()->SetTitleSize(0.052);
        hRad->GetYaxis()->SetTitleSize(0.052);
        hRad->GetXaxis()->SetLabelSize(0.044);
        hRad->GetYaxis()->SetLabelSize(0.044);
        hRad->GetYaxis()->SetTitleOffset(1.30);

        if (!hFirst) {
            hFirst = hRad;
            hFirst->Draw("hist");
        } else {
            hRad->Draw("hist same");
        }
        leg->AddEntry(hRad, Form("%.0f GeV", E[i]), "l");
        f->Close();
    }

    if (!hFirst) { Printf("ERROR: no valid files found"); return; }

    // Module boundary: at r = 7 mm (half-tile edge, perpendicular)
    // The actual cutoff for a square 14×14 mm tile varies with azimuth angle.
    // The shortest edge is at r = 7 mm, the corner at r = 9.9 mm.
    double yMax = hFirst->GetMaximum() * 1.3;
    hFirst->GetYaxis()->SetRangeUser(0., yMax);

    // Vertical lines at key radii
    TLine* lEdge = new TLine(7., 0., 7., yMax * 0.85);
    lEdge->SetLineColor(kRed); lEdge->SetLineStyle(2); lEdge->SetLineWidth(2);
    lEdge->Draw();

    TLine* lCorner = new TLine(9.9, 0., 9.9, yMax * 0.60);
    lCorner->SetLineColor(kOrange+2); lCorner->SetLineStyle(3); lCorner->SetLineWidth(2);
    lCorner->Draw();

    // Molière radius for LYSO (far beyond detector — mark at axis limit with arrow label)
    TLatex la; la.SetTextSize(0.032);
    la.SetTextColor(kRed);
    la.DrawLatex(7.1, yMax * 0.87, "r = 7 mm (tile edge)");
    la.SetTextColor(kOrange+2);
    la.DrawLatex(7.1, yMax * 0.62, "r = 9.9 mm (tile corner)");
    la.SetTextColor(kGray+2);
    la.DrawLatex(6.3, yMax * 0.48, "R_{M}(LYSO) #approx 20 mm (beyond detector)");

    // Module title and annotation
    la.SetNDC();
    la.SetTextColor(kBlack);
    la.SetTextSize(0.044);
    la.DrawLatex(0.17, 0.92, "Lateral (radial) shower profile — all energies");
    la.SetTextSize(0.032);
    la.SetTextColor(kGray+2);
    la.DrawLatex(0.17, 0.86,
        "Module tile: 14#times14 mm. R_{M}(LYSO) #approx 20 mm > tile half-width.");

    leg->Draw();

    gSystem->mkdir("build/plots", kTRUE);
    cR->SaveAs("build/plots/lateral_profile_radial.png");
    Printf("Saved: build/plots/lateral_profile_radial.png");

    // ── 2D map at highest energy ───────────────────────────────────────────────
    TFile* fHigh = TFile::Open(Form("build/scan/radical_E%.0fGeV.root", E[N-1]));
    if (fHigh && !fHigh->IsZombie()) {
        TH2D* h2Hi = (TH2D*)fHigh->Get("LateralProfile");
        if (h2Hi) {
            TCanvas* c2D = new TCanvas("c2D","2D",720,650);
            c2D->SetLeftMargin(0.13);
            c2D->SetRightMargin(0.16);
            c2D->SetBottomMargin(0.12);
            c2D->SetTopMargin(0.09);
            gStyle->SetPalette(kBird);
            h2Hi->SetTitle(Form(";x (mm);y (mm)"));
            h2Hi->GetXaxis()->SetTitleSize(0.050);
            h2Hi->GetYaxis()->SetTitleSize(0.050);
            h2Hi->GetXaxis()->SetLabelSize(0.042);
            h2Hi->GetYaxis()->SetLabelSize(0.042);
            h2Hi->GetZaxis()->SetTitle("Energy deposit (MeV)");
            h2Hi->GetZaxis()->SetTitleSize(0.042);
            h2Hi->Draw("colz");
            TLatex t2; t2.SetNDC(); t2.SetTextSize(0.042);
            t2.DrawLatex(0.14, 0.93,
                Form("Lateral shower profile (LYSO) — %.0f GeV", E[N-1]));
            c2D->SaveAs("build/plots/lateral_profile_2D.png");
            Printf("Saved: build/plots/lateral_profile_2D.png");
        }
        fHigh->Close();
    }

    printf("\n  Note: Molière radius for LYSO ≈ 20 mm. Module half-width = 7 mm.\n");
    printf("  The profile is truncated by the tile boundary at r = 7 mm.\n");
    printf("  Containment fractions above quantify the actual module acceptance.\n\n");
}
