// plot_shower_profile.C
// Reproduce Fig. 7 from Perez-Lara et al., NIM A 1068 (2024) 169737:
//   "Energy Deposited in LYSO Layers"  — Edep/E0 vs Layer Index
//
// Usage:
//   root -l -b -q analysis/plot_shower_profile.C
// Output:
//   build/plots/shower_profile_paper.png

void plot_shower_profile() {
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetPadLeftMargin(0.12);
    gStyle->SetPadBottomMargin(0.12);
    gStyle->SetPadRightMargin(0.04);
    gStyle->SetPadTopMargin(0.04);
    gStyle->SetGridColor(kGray);

    // ── open files ───────────────────────────────────────────────────────
    const int N = 3;
    double Ebeam[N] = {20., 50., 125.};
    const char* fname[N] = {
        "build/shower_profiles/shower_E20GeV.root",
        "build/shower_profiles/shower_E50GeV.root",
        "build/shower_profiles/shower_E125GeV.root"
    };

    // ── canvas ───────────────────────────────────────────────────────────
    auto c = new TCanvas("c_shower","Shower Profile",800,600);
    c->SetGrid(1,1);

    TH1D* hNorm[N];
    double yMax = 0.;

    for (int i = 0; i < N; i++) {
        TFile* f = TFile::Open(fname[i]);
        if (!f || f->IsZombie()) { Printf("Missing: %s",fname[i]); return; }
        TH1D* h = (TH1D*)f->Get("ShowerProfile");
        if (!h) { Printf("No ShowerProfile in %s",fname[i]); return; }

        // ShowerProfile is already summed over all events; divide by N_events * E0
        // to get fraction of beam energy per layer per event.
        // N_events = total entries spread over 29 bins → use Integral()
        double nEvt = h->Integral() / 29.;   // avg entries/bin × 29 bins = total events
        // Actually ShowerProfile is filled once per LYSO layer hit per event with
        // the energy deposited (in MeV). Integral() = sum of all Edep over all layers
        // and all events, in MeV. Per-layer mean = h->GetBinContent(b) / nEvt.
        // We want Edep_layer / E0 = (h->GetBinContent(b)) / (nEvt * Ebeam_MeV)

        double nEvents = h->GetEntries() / 29.;   // each event fills all 29 bins
        // Better: get nEvents from the TotalLYSO histogram entries
        TH1D* hE = (TH1D*)f->Get("TotalLYSO");
        if (hE) nEvents = hE->GetEntries();

        double Emev = Ebeam[i] * 1000.;  // GeV → MeV

        hNorm[i] = (TH1D*)h->Clone(Form("hN_%d",i));
        hNorm[i]->Scale(1.0 / (nEvents * Emev));
        if (hNorm[i]->GetMaximum() > yMax) yMax = hNorm[i]->GetMaximum();

        f->Close();
    }

    // ── draw styles matching paper ────────────────────────────────────────
    // Paper: 20 GeV = red filled circles, 50 GeV = blue open circles,
    //        125 GeV = blue stars.  All labeled "Geant4 Sim".
    int color[N]  = {kRed+1,   kBlue,      kBlue};
    int marker[N] = {20,        24,          29};    // filled circle, open circle, star
    double msize[N]= {1.2,      1.2,         1.5};
    const char* leg[N] = {"20 GeV ele","50 GeV ele","125 GeV ele"};

    hNorm[0]->SetMaximum(yMax * 1.20);
    hNorm[0]->SetMinimum(0.);
    hNorm[0]->GetXaxis()->SetTitle("Layer Index");
    hNorm[0]->GetYaxis()->SetTitle("Edep / E0");
    hNorm[0]->GetXaxis()->SetTitleSize(0.05);
    hNorm[0]->GetYaxis()->SetTitleSize(0.05);
    hNorm[0]->GetXaxis()->SetLabelSize(0.04);
    hNorm[0]->GetYaxis()->SetLabelSize(0.04);
    hNorm[0]->GetXaxis()->SetRangeUser(0, 28);

    auto leg_obj = new TLegend(0.62, 0.55, 0.94, 0.80);
    leg_obj->SetBorderSize(1);
    leg_obj->SetFillColor(0);
    leg_obj->SetTextSize(0.038);

    for (int i = 0; i < N; i++) {
        hNorm[i]->SetLineColor(color[i]);
        hNorm[i]->SetMarkerColor(color[i]);
        hNorm[i]->SetMarkerStyle(marker[i]);
        hNorm[i]->SetMarkerSize(msize[i]);
        hNorm[i]->SetLineWidth(1);
        hNorm[i]->Draw(i == 0 ? "P" : "P SAME");
        leg_obj->AddEntry(hNorm[i], leg[i], "p");
    }

    // "Geant4 Sim" label in legend matching paper
    leg_obj->AddEntry((TObject*)nullptr, "Geant4 Sim", "");
    leg_obj->Draw();

    // WLS region marker (paper Fig. 7 has an orange/salmon bar at the bottom
    // indicating the WLS capillary position — layers covered by the WLS)
    // WLS at 32.9–47.9 mm from upstream face; period = 4.4064 mm, layer L center at L*4.4064+0.75
    // Layer range: (32.9-0.75)/4.4064 ~ 7.3  to  (47.9-0.75)/4.4064 ~ 10.7
    double wlsL1 = 7.3, wlsL2 = 10.7;
    auto wlsBox = new TBox(wlsL1, 0., wlsL2, yMax*0.045);
    wlsBox->SetFillColor(kOrange-3);
    wlsBox->SetLineColor(kOrange-3);
    wlsBox->Draw("same");

    // title
    auto title = new TLatex();
    title->SetNDC();
    title->SetTextSize(0.048);
    title->SetTextFont(42);
    title->DrawLatex(0.25, 0.945, "Energy Deposited in LYSO Layers");

    // annotation
    auto ann = new TLatex();
    ann->SetNDC();
    ann->SetTextSize(0.032);
    ann->SetTextColor(kGray+2);
    ann->DrawLatex(0.14, 0.87, "RADiCALsim1: Tyvek 0.008\", WLS 15 mm (paper geometry)");

    gSystem->mkdir("build/plots",kTRUE);
    c->SaveAs("build/plots/shower_profile_paper.png");
    Printf("\nSaved: build/plots/shower_profile_paper.png");

    // ── print key metrics ─────────────────────────────────────────────────
    Printf("\n%-10s  %-8s  %-8s  %-8s","Energy","MaxLayer","COG","RMS");
    Printf("%-10s  %-8s  %-8s  %-8s","-------","-------","-----","----");
    for (int i = 0; i < N; i++) {
        TH1D* h = hNorm[i];
        int maxBin = h->GetMaximumBin() - 1;   // 0-indexed layer
        double cog = 0., wsum = 0.;
        for (int b=1; b<=h->GetNbinsX(); b++) {
            cog  += (b-1) * h->GetBinContent(b);
            wsum += h->GetBinContent(b);
        }
        cog /= wsum;
        double rms = 0.;
        for (int b=1; b<=h->GetNbinsX(); b++) {
            double d = (b-1) - cog;
            rms += d*d * h->GetBinContent(b);
        }
        rms = sqrt(rms/wsum);
        Printf("%-10.0f  %-8d  %-8.2f  %-8.2f", Ebeam[i], maxBin, cog, rms);
    }
    Printf("");
}
