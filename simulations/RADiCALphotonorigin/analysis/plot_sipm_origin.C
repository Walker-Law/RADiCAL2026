// plot_sipm_origin.C — draw the optical cross-talk graph from the locator output.
//
// For each of the 8 SiPMs (4 corners x upstream/downstream) it shows how many
// detected photons came from each origin corner, as a stacked bar coloured by
// the same scheme as vis_corners.mac:
//   corner0 top-right = red, corner1 top-left = yellow,
//   corner2 bottom-right = green, corner3 bottom-left = blue, elsewhere = grey.
// A near-diagonal result (each SiPM dominated by its own corner colour) means the
// capillaries guide their own light with little cross-talk. Also prints the
// quantitative percentage composition per SiPM.
//
//   root -l -b -q 'analysis/plot_sipm_origin.C("build/radical_output.root")'

void plot_sipm_origin(const char* fname = "build/radical_output.root") {
    TFile* f = TFile::Open(fname);
    if (!f || f->IsZombie()) { printf("ERROR: cannot open %s\n", fname); return; }
    TH2* m = (TH2*)f->Get("SiPM_vs_OriginCorner");
    if (!m) { printf("ERROR: histogram SiPM_vs_OriginCorner not found in %s\n", fname); return; }

    const int NS = 8, NO = 5;
    const char* sipmLbl[NS] = {"C0 U","C0 D","C1 U","C1 D","C2 U","C2 D","C3 U","C3 D"};
    const char* origLbl[NO] = {"corner0 (TR)","corner1 (TL)","corner2 (BR)","corner3 (BL)","elsewhere"};
    int col[NO] = {kRed, kYellow, kGreen + 1, kBlue, kGray + 1};

    gStyle->SetOptStat(0);
    THStack* hs = new THStack("hs",
        "Detected photons: which corner's light hits which SiPM;SiPM (corner #, Up/Down);detected photons");
    TLegend* leg = new TLegend(0.60, 0.66, 0.88, 0.88);
    leg->SetHeader("origin corner");

    TH1D* hO[NO];
    for (int o = 0; o < NO; o++) {
        hO[o] = new TH1D(Form("hO%d", o), "", NS, 0, NS);
        for (int s = 0; s < NS; s++) {
            hO[o]->SetBinContent(s + 1, m->GetBinContent(s + 1, o + 1));
            hO[o]->GetXaxis()->SetBinLabel(s + 1, sipmLbl[s]);
        }
        hO[o]->SetFillColor(col[o]);
        hO[o]->SetLineColor(kBlack);
        hs->Add(hO[o]);
        leg->AddEntry(hO[o], origLbl[o], "f");
    }

    gSystem->mkdir("build/plots", true);
    TCanvas* c1 = new TCanvas("c1", "SiPM origin", 950, 600);
    hs->Draw("hist");
    hs->GetXaxis()->SetLabelSize(0.05);
    leg->Draw();
    c1->SaveAs("build/plots/sipm_origin.png");

    // Quantitative composition table
    printf("\nSiPM cross-talk composition (%% of detected photons by origin corner):\n");
    printf("  SiPM   c0(TR)  c1(TL)  c2(BR)  c3(BL)  elsew.    total\n");
    printf("  ----------------------------------------------------------\n");
    for (int s = 0; s < NS; s++) {
        double tot = 0;
        for (int o = 0; o < NO; o++) tot += m->GetBinContent(s + 1, o + 1);
        printf("  %-5s", sipmLbl[s]);
        for (int o = 0; o < NO; o++) {
            double c = m->GetBinContent(s + 1, o + 1);
            printf("  %6.1f", tot > 0 ? 100.0 * c / tot : 0.0);
        }
        printf("  %8.0f\n", tot);
    }
    printf("\nSaved graph -> build/plots/sipm_origin.png\n");
}
