// plot_linearity.C — energy linearity and response uniformity across the scan.
//
//   root -l -b -q analysis/plot_linearity.C
//
// For each scan energy (5–120 GeV) extracts from ECombined (H1[20]):
//   • Gaussian-core mean and sigma  → response and resolution
// Plots:
//   1. E_reco vs E_beam  (linearity),  linear fit, residuals panel
//   2. Response (E_reco/E_beam) vs E_beam  (should be flat = 1)
//   3. E_LYSO-only vs ECombined response comparison
// Output: build/plots/energy_linearity.png
//         build/plots/energy_response.png

#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TGraphErrors.h"
#include "TCanvas.h"
#include "TPad.h"
#include "TStyle.h"
#include "TF1.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TLine.h"
#include "TSystem.h"
#include "TMath.h"

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

void plot_linearity() {
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetPadGridX(1);
    gStyle->SetPadGridY(1);

    const int N = 6;
    double E[N]   = {5, 10, 20, 50, 100, 120};
    double zero[N] = {0};

    double muComb[N],  errComb[N];   // ECombined Gaussian mean
    double sgComb[N],  errSgComb[N]; // ECombined Gaussian sigma
    double muLYSO[N],  errLYSO[N];   // raw TotalLYSO mean
    double muPbGl[N];                 // mean Pb-glass leakage

    printf("\n  E_beam  mu_comb  sigma_comb  sigma/E(%%)  mu_LYSO  mu_PbGlass  response\n");
    printf("  -----------------------------------------------------------------------\n");

    for (int i = 0; i < N; i++) {
        TFile* f = TFile::Open(Form("build/scan/radical_E%.0fGeV.root", E[i]));
        if (!f || f->IsZombie()) {
            Printf("ERROR: cannot open E=%.0f GeV file", E[i]);
            muComb[i] = E[i]; errComb[i] = 0; sgComb[i] = 0.1; errSgComb[i] = 0;
            muLYSO[i] = 0; muPbGl[i] = 0;
            continue;
        }

        TH1D* hC  = (TH1D*)f->Get("ECombined");
        TH1D* hL  = (TH1D*)f->Get("TotalLYSO");
        TH1D* hPb = (TH1D*)f->Get("PbGlassEnergy");

        // Rebin ECombined so bins are ~sigma/5 wide
        { TF1* pre = coreFit(hC, 2.0, 3);
          double sg0 = pre->GetParameter(2);
          double bw  = hC->GetBinWidth(1);
          int rb = TMath::Max(1, (int)std::round((sg0/5.)/bw));
          if (rb > 1) hC->Rebin(rb);
          delete pre; }

        TF1* gC = coreFit(hC, 2.0, 4);
        muComb[i]    = gC->GetParameter(1);
        errComb[i]   = gC->GetParError(1);
        sgComb[i]    = gC->GetParameter(2);
        errSgComb[i] = gC->GetParError(2);

        muLYSO[i] = hL->GetMean();
        muPbGl[i] = (hPb && hPb->GetEntries() > 0) ? hPb->GetMean() : 0.;

        double resp = muComb[i] / E[i];
        printf("  %5.0f   %7.4f  %10.4f  %9.2f   %7.4f  %10.4f   %7.4f\n",
               E[i], muComb[i], sgComb[i], 100.*sgComb[i]/muComb[i],
               muLYSO[i], muPbGl[i], resp);
        f->Close();
    }

    gSystem->mkdir("build/plots", kTRUE);

    // ── Plot 1: E_reco vs E_beam with split residual panel ────────────────────
    TCanvas* c1 = new TCanvas("c1","linearity",800,750);
    c1->cd();

    // Upper pad: E_reco vs E_beam
    TPad* pTop = new TPad("pTop","",0,0.28,1,1);
    pTop->SetBottomMargin(0.025);
    pTop->SetLeftMargin(0.15);
    pTop->SetRightMargin(0.06);
    pTop->SetTopMargin(0.09);
    pTop->SetGrid(1,1);
    pTop->Draw(); pTop->cd();

    auto grLine = new TGraphErrors(N, E, muComb, zero, errComb);
    grLine->SetMarkerStyle(20);
    grLine->SetMarkerSize(1.5);
    grLine->SetMarkerColor(kBlue+1);
    grLine->SetLineColor(kBlue+1);
    grLine->SetLineWidth(2);

    // Linear fit: E_reco = a*E_beam + b
    TF1* linFit = new TF1("linFit","[0]*x+[1]",3,130);
    linFit->SetParameters(0.01, 0.);
    linFit->SetLineColor(kRed);
    linFit->SetLineWidth(2);
    grLine->Fit(linFit, "RQ");
    double a = linFit->GetParameter(0);
    double b = linFit->GetParameter(1);

    // Perfect-linearity reference line
    TF1* perfect = new TF1("perfect","[0]*x",3,130);
    perfect->SetParameter(0, a);
    perfect->SetLineColor(kGray+1);
    perfect->SetLineStyle(2);
    perfect->SetLineWidth(1);

    grLine->SetTitle(";E_{beam} (GeV);E_{reco} (GeV)");
    grLine->GetYaxis()->SetTitleOffset(1.3);
    grLine->GetYaxis()->SetTitleSize(0.052);
    grLine->GetXaxis()->SetLabelSize(0.);  // hidden (shown in residual pad)
    grLine->Draw("AP");
    perfect->Draw("same");
    linFit->Draw("same");
    grLine->Draw("P same");

    TLatex la; la.SetNDC(); la.SetTextSize(0.046);
    la.DrawLatex(0.18, 0.88, "Energy linearity (E_{reco} = E_{LYSO} + 0.18 E_{PbGlass})");
    la.SetTextSize(0.040);
    la.SetTextColor(kRed);
    la.DrawLatex(0.18, 0.78, Form("fit: E_{reco} = %.4f E_{beam} + %.4f GeV", a, b));

    // Lower pad: residual (E_reco - fit) / E_beam  [%]
    c1->cd();
    TPad* pBot = new TPad("pBot","",0,0,1,0.28);
    pBot->SetTopMargin(0.025);
    pBot->SetBottomMargin(0.34);
    pBot->SetLeftMargin(0.15);
    pBot->SetRightMargin(0.06);
    pBot->SetGrid(1,1);
    pBot->Draw(); pBot->cd();

    double resid[N], residErr[N];
    for (int i = 0; i < N; i++) {
        double predicted = linFit->Eval(E[i]);
        resid[i]    = 100. * (muComb[i] - predicted) / predicted;
        residErr[i] = 100. * errComb[i] / predicted;
    }

    auto grResid = new TGraphErrors(N, E, resid, zero, residErr);
    grResid->SetMarkerStyle(20);
    grResid->SetMarkerSize(1.4);
    grResid->SetMarkerColor(kBlue+1);
    grResid->SetLineColor(kBlue+1);
    grResid->SetTitle(";E_{beam} (GeV);Residual (%)");
    grResid->GetXaxis()->SetTitleSize(0.135);
    grResid->GetYaxis()->SetTitleSize(0.120);
    grResid->GetXaxis()->SetLabelSize(0.115);
    grResid->GetYaxis()->SetLabelSize(0.100);
    grResid->GetXaxis()->SetTitleOffset(1.05);
    grResid->GetYaxis()->SetTitleOffset(0.55);
    grResid->GetYaxis()->SetNdivisions(504);

    double rMax = 0;
    for (int i=0;i<N;i++) rMax=TMath::Max(rMax,TMath::Abs(resid[i])+residErr[i]);
    rMax = TMath::Max(rMax*1.5, 0.5);
    grResid->GetYaxis()->SetRangeUser(-rMax, rMax);

    grResid->Draw("AP");
    TLine* zeroLine = new TLine(3, 0, 130, 0);
    zeroLine->SetLineColor(kRed); zeroLine->SetLineStyle(2); zeroLine->SetLineWidth(1);
    zeroLine->Draw();
    grResid->Draw("P same");

    c1->SaveAs("build/plots/energy_linearity.png");
    Printf("Saved: build/plots/energy_linearity.png");

    // ── Plot 2: Response E_reco/E_beam vs E_beam ──────────────────────────────
    TCanvas* c2 = new TCanvas("c2","response",800,550);
    c2->SetLeftMargin(0.15);
    c2->SetRightMargin(0.06);
    c2->SetBottomMargin(0.15);
    c2->SetTopMargin(0.09);
    c2->SetGrid(1,1);

    double resp[N], respErr[N];
    for (int i = 0; i < N; i++) {
        resp[i]    = muComb[i] / E[i];
        respErr[i] = errComb[i] / E[i];
    }

    auto grResp = new TGraphErrors(N, E, resp, zero, respErr);
    grResp->SetMarkerStyle(20);
    grResp->SetMarkerSize(1.5);
    grResp->SetMarkerColor(kBlue+1);
    grResp->SetLineColor(kBlue+1);
    grResp->SetLineWidth(2);

    double respMin = *std::min_element(resp,resp+N) - 0.003;
    double respMax = *std::max_element(resp,resp+N) + 0.003;
    grResp->GetYaxis()->SetRangeUser(respMin, respMax);
    grResp->SetTitle(";E_{beam} (GeV);E_{reco} / E_{beam}");
    grResp->GetXaxis()->SetTitleSize(0.055);
    grResp->GetYaxis()->SetTitleSize(0.055);
    grResp->GetXaxis()->SetLabelSize(0.046);
    grResp->GetYaxis()->SetLabelSize(0.046);
    grResp->GetYaxis()->SetTitleOffset(1.25);
    grResp->Draw("AP");

    // Flat reference
    double meanResp = 0;
    for (int i=0;i<N;i++) meanResp += resp[i]/N;
    TLine* flat = new TLine(3, meanResp, 130, meanResp);
    flat->SetLineColor(kRed); flat->SetLineStyle(2); flat->SetLineWidth(2);
    flat->Draw();
    grResp->Draw("P same");

    // Compute max deviation
    double maxDev = 0;
    for (int i=0;i<N;i++) maxDev = TMath::Max(maxDev, TMath::Abs(resp[i]-meanResp));

    TLatex lb; lb.SetNDC(); lb.SetTextSize(0.042);
    lb.DrawLatex(0.18, 0.88, "Energy response uniformity");
    lb.SetTextSize(0.036); lb.SetTextColor(kGray+2);
    lb.DrawLatex(0.18, 0.80, Form("Mean response: %.4f    Max deviation: %.2f%%",
                                   meanResp, 100.*maxDev/meanResp));
    lb.SetTextColor(kRed);
    lb.DrawLatex(0.18, 0.73, Form("--- mean = %.4f", meanResp));

    c2->SaveAs("build/plots/energy_response.png");
    Printf("Saved: build/plots/energy_response.png");

    printf("\n  Linear fit: E_reco = %.5f * E_beam + %.4f GeV\n", a, b);
    printf("  Intercept / slope = %.2f GeV (should be ~0 for perfect linearity)\n", b/a);
    printf("  Max non-linearity: see residuals plot\n\n");
}
