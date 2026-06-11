// plot_leakage.C — validate the tail-catcher leakage correction.
//
//   root -l -b -q analysis/plot_leakage.C
//
// The EventAction uses E_combined = E_LYSO + 0.18 * E_PbGlass.
// This macro validates that coefficient by:
//
//  (A) Leakage fraction: mean(E_PbGlass)/E_beam vs E_beam.
//      Shows how much energy escapes downstream as a function of energy.
//
//  (B) Response before/after correction:
//      mean(E_LYSO/E_beam) vs E_beam  — uncorrected LYSO-only response
//      mean(E_combined)/E_beam vs E_beam — corrected response (should be flat)
//
//  (C) Coefficient scan: for each energy, compute
//        sigma(E_LYSO + k*E_PbGlass) / mean(E_LYSO + k*E_PbGlass)
//      over k in [0.05, 0.60]. Optimal k minimizes sigma/E.
//      If 0.18 is correct, all energies should agree and converge near k=0.18.
//
//  (D) Correlation: E_LYSO vs E_PbGlass scatter plot (H2[13]) at one energy
//      to visualise the anti-correlation that drives the correction.
//
// Output:
//   build/plots/leakage_fraction.png
//   build/plots/leakage_response.png
//   build/plots/leakage_coefficient_scan.png
//   build/plots/leakage_correlation.png  (for highest energy)

#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TProfile.h"
#include "TGraphErrors.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TF1.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TLine.h"
#include "TSystem.h"
#include "TMath.h"

// Gaussian core fit (iterative 2σ clip)
TF1* coreFit(TH1* h, double nsig=2.0, int iters=4) {
    double mu=h->GetMean(), sg=h->GetRMS();
    TF1* g=new TF1(Form("cf_%s_%p",h->GetName(),(void*)h),"gaus",mu-nsig*sg,mu+nsig*sg);
    for(int i=0;i<iters;i++){
        g->SetRange(mu-nsig*sg,mu+nsig*sg);
        h->Fit(g,"RQL0");
        mu=g->GetParameter(1); sg=g->GetParameter(2);
        if(sg<=0) break;
    }
    return g;
}

// From H2 (LYSO vs PbGlass scatter), compute sigma/E for
// E_combined = E_LYSO + k * E_PbGlass as a function of k.
// Returns a TGraph of (k, sigma/E) from k_lo to k_hi in nk steps.
TGraph* coeffScan(TH2D* h2, double k_lo=0.04, double k_hi=0.65, int nk=100) {
    int nEv = 0;
    // Collect event-level pairs from H2 bin centers
    // Each bin (ix,iy) represents a cluster of events — use bin content as weight
    double kStep = (k_hi - k_lo) / (nk - 1);
    std::vector<double> kv, sv;

    for (int ik = 0; ik < nk; ik++) {
        double k = k_lo + ik * kStep;
        // compute weighted mean and variance of E_comb = x + k*y
        double sumW=0, sumX=0, sumX2=0;
        for (int ix=1; ix<=h2->GetNbinsX(); ix++) {
            for (int iy=1; iy<=h2->GetNbinsY(); iy++) {
                double w = h2->GetBinContent(ix,iy);
                if (w<=0) continue;
                double eLYSO = h2->GetXaxis()->GetBinCenter(ix);  // GeV
                double ePbGl = h2->GetYaxis()->GetBinCenter(iy);  // GeV
                double eComb = eLYSO + k * ePbGl;
                sumW  += w;
                sumX  += w * eComb;
                sumX2 += w * eComb * eComb;
            }
        }
        if (sumW < 2 || sumX < 1e-9) continue;
        double mean = sumX / sumW;
        double var  = sumX2/sumW - mean*mean;
        if (var < 0) var = 0;
        kv.push_back(k);
        sv.push_back(100. * TMath::Sqrt(var) / mean);
    }
    auto gr = new TGraph(kv.size(), kv.data(), sv.data());
    return gr;
}

void plot_leakage() {
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetPadGridX(1);
    gStyle->SetPadGridY(1);

    const int N = 6;
    double E[N] = {5, 10, 20, 50, 100, 120};
    int  cols[N] = {kRed+1, kOrange+1, kGreen+2, kAzure+2, kBlue+1, kMagenta+1};
    double zero[N] = {0};

    double leakFrac[N], leakFracErr[N];   // E_PbGlass / E_beam
    double respRaw[N],  respRawErr[N];    // mean(E_LYSO) / E_beam (uncorrected)
    double respCorr[N], respCorrErr[N];   // mean(E_combined) / E_beam

    printf("\n  E_beam  LeakFrac(%%)  RespRaw    RespCorr   sigma/E(raw)  sigma/E(corr)\n");
    printf("  -------------------------------------------------------------------------\n");

    for (int i = 0; i < N; i++) {
        TFile* f = TFile::Open(Form("build/scan/radical_E%.0fGeV.root", E[i]));
        if (!f || f->IsZombie()) {
            leakFrac[i]=0; leakFracErr[i]=0;
            respRaw[i]=0;  respRawErr[i]=0;
            respCorr[i]=0; respCorrErr[i]=0;
            continue;
        }
        TH1D* hL  = (TH1D*)f->Get("TotalLYSO");
        TH1D* hPb = (TH1D*)f->Get("PbGlassEnergy");
        TH1D* hC  = (TH1D*)f->Get("ECombined");

        // Rebin ECombined for clean fit
        { TF1* pre = coreFit(hC,2.0,3);
          double sg0=pre->GetParameter(2), bw=hC->GetBinWidth(1);
          int rb=TMath::Max(1,(int)std::round((sg0/5.)/bw));
          if(rb>1) hC->Rebin(rb); delete pre; }

        TF1* gC = coreFit(hC, 2.0, 4);
        double muC   = gC->GetParameter(1);
        double sgC   = gC->GetParameter(2);
        double errC  = gC->GetParError(1);

        double muL   = hL->GetMean();
        double errL  = hL->GetMeanError();

        double muPb  = (hPb && hPb->GetEntries()>0) ? hPb->GetMean()  : 0.;
        double errPb = (hPb && hPb->GetEntries()>0) ? hPb->GetMeanError() : 0.;

        // LYSO-only resolution
        TF1* gL = coreFit(hL, 2.0, 4);
        double muLfit = gL->GetParameter(1);
        double sgLfit = gL->GetParameter(2);

        leakFrac[i]    = 100. * muPb  / E[i];
        leakFracErr[i] = 100. * errPb / E[i];

        // Raw response: need to invert the sampling fraction
        // E_LYSO ≈ f_s * E_shower, so E_reco_raw = E_LYSO / f_s
        // But f_s is what we're determining; use mean ratio instead.
        respRaw[i]    = muL / E[i];         // dimensionless LYSO/beam
        respRawErr[i] = errL / E[i];

        respCorr[i]    = muC / E[i];
        respCorrErr[i] = errC / E[i];

        printf("  %5.0f    %8.3f     %8.4f   %8.4f    %8.3f%%    %8.3f%%\n",
               E[i], leakFrac[i], respRaw[i], respCorr[i],
               (muLfit>0 ? 100.*sgLfit/muLfit : 0.),
               (muC>0    ? 100.*sgC/muC       : 0.));
        f->Close();
    }

    gSystem->mkdir("build/plots", kTRUE);

    // ── (A) Leakage fraction vs E_beam ───────────────────────────────────────
    {
        TCanvas* ca = new TCanvas("ca","leakfrac",800,550);
        ca->SetLeftMargin(0.15); ca->SetRightMargin(0.06);
        ca->SetBottomMargin(0.14); ca->SetTopMargin(0.09);

        auto gr = new TGraphErrors(N, E, leakFrac, zero, leakFracErr);
        gr->SetMarkerStyle(20); gr->SetMarkerSize(1.5);
        gr->SetMarkerColor(kRed+1); gr->SetLineColor(kRed+1); gr->SetLineWidth(2);
        gr->SetTitle(";E_{beam} (GeV);E_{PbGlass} / E_{beam} (%)");
        gr->GetXaxis()->SetTitleSize(0.055); gr->GetYaxis()->SetTitleSize(0.055);
        gr->GetXaxis()->SetLabelSize(0.046); gr->GetYaxis()->SetLabelSize(0.046);
        gr->GetYaxis()->SetTitleOffset(1.25);
        double ylo = 0, yhi = *std::max_element(leakFrac,leakFrac+N)*1.4;
        gr->GetYaxis()->SetRangeUser(ylo, yhi);
        gr->Draw("AP");

        TLatex la; la.SetNDC(); la.SetTextSize(0.044);
        la.DrawLatex(0.17, 0.92, "Downstream leakage fraction vs beam energy");
        la.SetTextSize(0.036); la.SetTextColor(kGray+2);
        la.DrawLatex(0.17, 0.84, "Higher energies → deeper shower max → more forward leakage");

        ca->SaveAs("build/plots/leakage_fraction.png");
        Printf("Saved: build/plots/leakage_fraction.png");
    }

    // ── (B) Response before vs after correction ──────────────────────────────
    {
        TCanvas* cb = new TCanvas("cb","resp",800,550);
        cb->SetLeftMargin(0.15); cb->SetRightMargin(0.06);
        cb->SetBottomMargin(0.14); cb->SetTopMargin(0.09);

        // Normalize respRaw so it's on the same scale as respCorr
        // (they're in different absolute units; normalise each to its mean)
        double rawMean=0, corrMean=0;
        for(int i=0;i<N;i++){ rawMean+=respRaw[i]; corrMean+=respCorr[i]; }
        rawMean/=N; corrMean/=N;
        double rawNorm[N], corrNorm[N], rawNormErr[N], corrNormErr[N];
        for(int i=0;i<N;i++){
            rawNorm[i]    = respRaw[i]/rawMean;
            rawNormErr[i] = respRawErr[i]/rawMean;
            corrNorm[i]   = respCorr[i]/corrMean;
            corrNormErr[i]= respCorrErr[i]/corrMean;
        }

        auto grRaw  = new TGraphErrors(N,E,rawNorm, zero,rawNormErr);
        auto grCorr = new TGraphErrors(N,E,corrNorm,zero,corrNormErr);

        grRaw->SetMarkerStyle(24); grRaw->SetMarkerSize(1.5);
        grRaw->SetMarkerColor(kBlack); grRaw->SetLineColor(kBlack); grRaw->SetLineWidth(2);

        grCorr->SetMarkerStyle(20); grCorr->SetMarkerSize(1.5);
        grCorr->SetMarkerColor(kBlue+1); grCorr->SetLineColor(kBlue+1); grCorr->SetLineWidth(2);

        // Frame
        double allMin=1e9, allMax=-1e9;
        for(int i=0;i<N;i++){
            allMin=TMath::Min(allMin, TMath::Min(rawNorm[i]-rawNormErr[i]-0.003,
                                                  corrNorm[i]-corrNormErr[i]-0.003));
            allMax=TMath::Max(allMax, TMath::Max(rawNorm[i]+rawNormErr[i]+0.003,
                                                  corrNorm[i]+corrNormErr[i]+0.003));
        }
        grCorr->GetYaxis()->SetRangeUser(allMin-0.005, allMax+0.005);
        grCorr->SetTitle(";E_{beam} (GeV);Response (normalised to mean)");
        grCorr->GetXaxis()->SetTitleSize(0.055); grCorr->GetYaxis()->SetTitleSize(0.055);
        grCorr->GetXaxis()->SetLabelSize(0.046); grCorr->GetYaxis()->SetLabelSize(0.046);
        grCorr->GetYaxis()->SetTitleOffset(1.25);
        grCorr->Draw("AP");
        grRaw->Draw("P same");

        TLine* flatLine = new TLine(3,1,130,1);
        flatLine->SetLineColor(kGray+1); flatLine->SetLineStyle(2); flatLine->SetLineWidth(1);
        flatLine->Draw();

        TLegend* leg = new TLegend(0.40,0.15,0.90,0.32);
        leg->SetBorderSize(1); leg->SetFillColor(0); leg->SetTextSize(0.036);
        leg->AddEntry(grRaw,  "LYSO only (uncorrected)", "p");
        leg->AddEntry(grCorr, "E_{comb} = E_{LYSO} + 0.18 E_{PbGlass}", "p");
        leg->Draw();

        TLatex la; la.SetNDC(); la.SetTextSize(0.044);
        la.DrawLatex(0.17, 0.92, "Energy response: before vs after leakage correction");

        cb->SaveAs("build/plots/leakage_response.png");
        Printf("Saved: build/plots/leakage_response.png");
    }

    // ── (C) Coefficient scan: optimal k at each energy ────────────────────────
    {
        TCanvas* cc = new TCanvas("cc","coeff",800,580);
        cc->SetLeftMargin(0.15); cc->SetRightMargin(0.06);
        cc->SetBottomMargin(0.14); cc->SetTopMargin(0.09);

        TLegend* leg = new TLegend(0.55,0.55,0.92,0.88);
        leg->SetBorderSize(1); leg->SetFillColor(0); leg->SetTextSize(0.034);

        bool first = true;
        double gMin=1e9, gMax=-1e9;
        std::vector<TGraph*> scanGr;

        for (int i = 0; i < N; i++) {
            TFile* f = TFile::Open(Form("build/scan/radical_E%.0fGeV.root", E[i]));
            if (!f || f->IsZombie()) continue;
            TH2D* h2 = (TH2D*)f->Get("LYSOvsPbGlass");
            if (!h2 || h2->GetEntries() < 10) { f->Close(); continue; }

            TGraph* gr = coeffScan(h2);
            gr->SetLineColor(cols[i]);
            gr->SetLineWidth(2);

            // Find minimum
            double kOpt = 0, sigOpt = 1e9;
            for (int p=0; p<gr->GetN(); p++) {
                if (gr->GetY()[p] < sigOpt) { sigOpt=gr->GetY()[p]; kOpt=gr->GetX()[p]; }
            }
            Printf("  E=%.0f GeV: optimal k = %.3f  sigma/E_min = %.2f%%",
                   E[i], kOpt, sigOpt);

            for(int p=0;p<gr->GetN();p++){
                gMin=TMath::Min(gMin,gr->GetY()[p]);
                gMax=TMath::Max(gMax,gr->GetY()[p]);
            }
            scanGr.push_back(gr);

            if (first) {
                gr->SetTitle(";Correction coefficient k;#sigma/E (%)");
                gr->GetXaxis()->SetTitleSize(0.055);
                gr->GetYaxis()->SetTitleSize(0.055);
                gr->GetXaxis()->SetLabelSize(0.046);
                gr->GetYaxis()->SetLabelSize(0.046);
                gr->GetYaxis()->SetTitleOffset(1.25);
                gr->Draw("AL");
                first = false;
            } else {
                gr->Draw("L same");
            }
            leg->AddEntry(gr, Form("%.0f GeV", E[i]), "l");
            f->Close();
        }

        if (!scanGr.empty()) {
            scanGr[0]->GetYaxis()->SetRangeUser(gMin*0.97, gMax*1.05);
        }

        // Vertical line at k = 0.18 (used in simulation)
        TLine* kLine = new TLine(0.18, gMin*0.97, 0.18, gMax*0.85);
        kLine->SetLineColor(kRed); kLine->SetLineStyle(2); kLine->SetLineWidth(2);
        kLine->Draw();

        TLatex la; la.SetNDC(); la.SetTextSize(0.044);
        la.DrawLatex(0.17, 0.92, "Optimal leakage coefficient k (minimize #sigma/E)");
        la.SetTextSize(0.034); la.SetTextColor(kRed);
        la.DrawLatex(0.17, 0.84, "dashed: k = 0.18 (used in simulation)");
        leg->Draw();

        cc->SaveAs("build/plots/leakage_coefficient_scan.png");
        Printf("Saved: build/plots/leakage_coefficient_scan.png");
    }

    // ── (D) LYSO vs PbGlass scatter at highest energy ─────────────────────────
    {
        TFile* fH = TFile::Open(Form("build/scan/radical_E%.0fGeV.root", E[N-1]));
        if (fH && !fH->IsZombie()) {
            TH2D* h2 = (TH2D*)fH->Get("LYSOvsPbGlass");
            if (h2 && h2->GetEntries() > 10) {
                TCanvas* cd = new TCanvas("cd","corr",720,620);
                cd->SetLeftMargin(0.14); cd->SetRightMargin(0.16);
                cd->SetBottomMargin(0.13); cd->SetTopMargin(0.09);
                gStyle->SetPalette(kBird);
                h2->SetTitle(Form(";E_{LYSO} (GeV);E_{PbGlass} (GeV)"));
                h2->GetXaxis()->SetTitleSize(0.050);
                h2->GetYaxis()->SetTitleSize(0.050);
                h2->GetXaxis()->SetLabelSize(0.042);
                h2->GetYaxis()->SetLabelSize(0.042);
                h2->GetZaxis()->SetTitle("Events");
                h2->Draw("colz");

                // Profile line: fit <E_PbGlass> = a + b*E_LYSO
                TProfile* prof = h2->ProfileX("prof_corr", 1, -1, "");
                prof->SetLineColor(kRed); prof->SetLineWidth(2);
                prof->SetMarkerColor(kRed); prof->SetMarkerStyle(20); prof->SetMarkerSize(0.7);
                prof->Draw("same");

                TLatex la; la.SetNDC(); la.SetTextSize(0.044);
                la.DrawLatex(0.15, 0.93,
                    Form("E_{{LYSO}} vs E_{{PbGlass}} anti-correlation — %.0f GeV", E[N-1]));
                la.SetTextSize(0.034); la.SetTextColor(kRed);
                la.DrawLatex(0.15, 0.86, "Red: #LTE_{PbGlass}#GT vs E_{LYSO} profile");

                cd->SaveAs("build/plots/leakage_correlation.png");
                Printf("Saved: build/plots/leakage_correlation.png");
            }
            fH->Close();
        }
    }
    Printf("\nAll leakage plots written to build/plots/\n");
}
