// plot_shower_profile.C — reproduce paper Fig. 7
// Usage: root -l -b -q analysis/plot_shower_profile.C
// Output: build/plots/shower_profile_paper.png

void plot_shower_profile() {
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetPadLeftMargin(0.13);
    gStyle->SetPadBottomMargin(0.13);
    gStyle->SetPadRightMargin(0.05);
    gStyle->SetPadTopMargin(0.06);

    const int N = 3;
    const char* fname[N] = {
        "build/shower_profiles/shower_E20GeV.root",
        "build/shower_profiles/shower_E50GeV.root",
        "build/shower_profiles/shower_E125GeV.root"
    };
    double Ebeam_MeV[N] = {20000., 50000., 125000.};
    const char* leg[N]  = {"20 GeV ele","50 GeV ele","125 GeV ele"};
    int color[N]  = {kRed+1, kBlue, kBlue};
    int marker[N] = {20, 24, 29};
    double msize[N] = {1.2, 1.2, 1.5};

    // Extract normalised histograms
    TH1D* hNorm[N];
    double yMax = 0.;

    for (int i = 0; i < N; i++) {
        TFile* f = TFile::Open(fname[i]);
        if (!f || f->IsZombie()) { Printf("ERROR: can't open %s", fname[i]); return; }

        TH1D* hSP  = (TH1D*)f->Get("ShowerProfile");
        TH1D* hTot = (TH1D*)f->Get("TotalLYSO");
        if (!hSP || !hTot) { Printf("ERROR: missing histogram in %s", fname[i]); return; }

        double nEvt = hTot->GetEntries();
        if (nEvt < 1) { Printf("ERROR: zero events in %s", fname[i]); return; }

        // Clone before closing file so ROOT owns a copy
        hNorm[i] = (TH1D*)hSP->Clone(Form("hNorm_%d", i));
        hNorm[i]->SetDirectory(nullptr);   // detach from file
        f->Close();

        // Normalise: Edep(layer) / (N_events * E_beam_MeV)
        hNorm[i]->Scale(1.0 / (nEvt * Ebeam_MeV[i]));
        if (hNorm[i]->GetMaximum() > yMax) yMax = hNorm[i]->GetMaximum();
    }

    // Canvas
    auto c = new TCanvas("cSP","Shower Profile",800,600);
    c->SetGrid(1,1);
    gPad->SetGrid(1,1);

    // Reference frame from first histogram
    TH1D* hRef = hNorm[0];
    hRef->SetMaximum(yMax * 1.22);
    hRef->SetMinimum(0.);
    hRef->GetXaxis()->SetTitle("Layer Index");
    hRef->GetYaxis()->SetTitle("Edep / E0");
    hRef->GetXaxis()->SetTitleSize(0.050);
    hRef->GetYaxis()->SetTitleSize(0.050);
    hRef->GetXaxis()->SetLabelSize(0.043);
    hRef->GetYaxis()->SetLabelSize(0.043);
    hRef->GetXaxis()->SetRangeUser(-0.5, 28.5);

    auto legend = new TLegend(0.60, 0.54, 0.93, 0.79);
    legend->SetBorderSize(1);
    legend->SetFillColor(0);
    legend->SetTextSize(0.038);

    for (int i = 0; i < N; i++) {
        hNorm[i]->SetLineColor(color[i]);
        hNorm[i]->SetMarkerColor(color[i]);
        hNorm[i]->SetMarkerStyle(marker[i]);
        hNorm[i]->SetMarkerSize(msize[i]);
        hNorm[i]->Draw(i == 0 ? "P" : "P SAME");
        legend->AddEntry(hNorm[i], leg[i], "p");
    }
    legend->AddEntry((TObject*)0, "Geant4 Sim", "");
    legend->Draw();

    // Orange bar showing WLS capillary position (paper Fig. 7 style)
    // WLS from 32.9–47.9 mm from upstream face; period = 4.4064 mm/layer
    // Layer at position z: L = (z - 0.75) / 4.4064
    // L1 = (32.9 - 0.75)/4.4064 = 7.29,  L2 = (47.9 - 0.75)/4.4064 = 10.69
    double wlsBarH = yMax * 0.045;
    auto wlsBox = new TBox(7.29, 0., 10.69, wlsBarH);
    wlsBox->SetFillColor(kOrange-3);
    wlsBox->SetFillStyle(1001);
    wlsBox->SetLineColor(kOrange-4);
    wlsBox->Draw("same");

    // Title
    TLatex lat;
    lat.SetNDC();
    lat.SetTextFont(42);
    lat.SetTextSize(0.048);
    lat.DrawLatex(0.22, 0.955, "Energy Deposited in LYSO Layers");

    gSystem->mkdir("build/plots", kTRUE);
    c->SaveAs("build/plots/shower_profile_paper.png");
    Printf("\nSaved: build/plots/shower_profile_paper.png");

    // Print shower-max metrics
    Printf("\n%-8s  %-8s  %-8s  %-6s", "E(GeV)", "MaxLyr", "COG", "RMS");
    for (int i = 0; i < N; i++) {
        TH1D* h = hNorm[i];
        int maxBin = h->GetMaximumBin() - 1;
        double cog = 0., wsum = 0.;
        for (int b = 1; b <= h->GetNbinsX(); b++) {
            cog  += (b - 1) * h->GetBinContent(b);
            wsum += h->GetBinContent(b);
        }
        cog /= (wsum > 0 ? wsum : 1);
        double rms = 0.;
        for (int b = 1; b <= h->GetNbinsX(); b++) {
            double d = (b - 1) - cog;
            rms += d * d * h->GetBinContent(b);
        }
        rms = TMath::Sqrt(rms / (wsum > 0 ? wsum : 1));
        Printf("%-8.0f  %-8d  %-8.2f  %-6.2f", Ebeam_MeV[i]/1000., maxBin, cog, rms);
    }
    Printf("");
}
