// plot_holescan.C — RADiCALsimHoleScan: light output at the capillary ENDS vs
// the diameter of the holes drilled through the tiles. All five holes are set to
// a common diameter D and every capillary scales to FILL its hole, so a larger D
// means a larger WLS fiber / EJ309 bore. For each hole_D<D>.root this reads the
// MEAN detected p.e. per event:
//     H1[38] Light_Corners  — 4 corner WLS timing capillaries (both ends)
//     H1[39] Light_Center   — center EJ309 energy capillary  (both ends)
//     H1[40] Light_Total    — all five capillaries
// and plots mean light output vs D (error bars = error on the mean = RMS/sqrt(N)).
//
//   root -l -b -q 'analysis/plot_holescan.C("build/scan/hole_scan_2000")'

#include <vector>
#include <algorithm>

void plot_holescan(const char* dir = "build/scan/hole_scan_2000") {
  gStyle->SetOptStat(0);
  const double D[] = {1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0};
  const int ND = sizeof(D) / sizeof(D[0]);
  std::vector<double> vD, vCorn, vCornE, vCent, vCentE, vTot, vTotE;

  printf("\n  D(mm)     Corners (p.e.)      Center (p.e.)       Total (p.e.)      N\n");
  printf("  ---------------------------------------------------------------------------\n");
  for (int i = 0; i < ND; i++) {
    TString f = Form("%s/hole_D%.1f.root", dir, D[i]);
    TFile* fp = TFile::Open(f);
    if (!fp || fp->IsZombie()) { printf("  %.1f    -- file missing --\n", D[i]); if (fp) fp->Close(); continue; }
    TH1* hc = (TH1*)fp->Get("Light_Corners");
    TH1* he = (TH1*)fp->Get("Light_Center");
    TH1* ht = (TH1*)fp->Get("Light_Total");
    if (!hc || !he || !ht || hc->GetEntries() < 1) { printf("  %.1f    -- no data --\n", D[i]); fp->Close(); continue; }
    vD.push_back(D[i]);
    vCorn.push_back(hc->GetMean()); vCornE.push_back(hc->GetMeanError());
    vCent.push_back(he->GetMean()); vCentE.push_back(he->GetMeanError());
    vTot.push_back(ht->GetMean());  vTotE.push_back(ht->GetMeanError());
    printf("  %.1f   %8.1f +- %-6.1f  %8.1f +- %-6.1f  %8.1f +- %-6.1f  %.0f\n",
           D[i], hc->GetMean(), hc->GetMeanError(), he->GetMean(), he->GetMeanError(),
           ht->GetMean(), ht->GetMeanError(), hc->GetEntries());
    fp->Close();
  }
  if (vD.size() < 2) { printf("\n  Need >=2 hole sizes with data — run run_hole_scan.sh first.\n"); return; }

  const int N = vD.size();
  std::vector<double> ex(N, 0.);
  auto gC = new TGraphErrors(N, vD.data(), vCorn.data(), ex.data(), vCornE.data());
  auto gE = new TGraphErrors(N, vD.data(), vCent.data(), ex.data(), vCentE.data());
  auto gT = new TGraphErrors(N, vD.data(), vTot.data(),  ex.data(), vTotE.data());
  gC->SetName("Light_Corners"); gE->SetName("Light_Center"); gT->SetName("Light_Total");

  TCanvas* c = new TCanvas("c", "holescan", 900, 650);
  c->SetGrid();
  gT->SetTitle("RADiCAL light output at capillary ends vs tile hole diameter;"
               "hole diameter D (mm);mean light output (p.e./event)");
  gT->SetMarkerStyle(20); gT->SetMarkerSize(1.3); gT->SetMarkerColor(kBlack);  gT->SetLineColor(kBlack);
  gC->SetMarkerStyle(21); gC->SetMarkerSize(1.3); gC->SetMarkerColor(kBlue+1); gC->SetLineColor(kBlue+1);
  gE->SetMarkerStyle(22); gE->SetMarkerSize(1.3); gE->SetMarkerColor(kRed+1);  gE->SetLineColor(kRed+1);
  gT->Draw("APL"); gC->Draw("PL same"); gE->Draw("PL same");
  double ymax = *std::max_element(vTot.begin(), vTot.end());
  gT->GetYaxis()->SetRangeUser(0., ymax * 1.15);

  auto leg = new TLegend(0.15, 0.72, 0.52, 0.88);
  leg->AddEntry(gT, "All five capillaries", "pl");
  leg->AddEntry(gC, "4 corner WLS timing caps", "pl");
  leg->AddEntry(gE, "Center EJ309 energy cap", "pl");
  leg->Draw();

  gSystem->mkdir("build/plots", true);
  c->SaveAs("build/plots/holescan_light_vs_diameter.png");
  TFile out(Form("%s/holescan_curves.root", dir), "RECREATE");
  gC->Write(); gE->Write(); gT->Write(); out.Close();
  printf("\n  Saved build/plots/holescan_light_vs_diameter.png\n");
  printf("  Saved %s/holescan_curves.root\n\n", dir);
}
