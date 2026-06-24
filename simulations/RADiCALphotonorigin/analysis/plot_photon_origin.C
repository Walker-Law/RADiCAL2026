// plot_photon_origin.C — where does the detected light originate in the LYSO
// transverse plane, and how does that map to SiPM (corner) excitation?
//
// From a random-beam run (run_origin_batch.sh) it writes four figures into
// build/plots/ and prints two quantitative tables:
//
//   QUALITATIVE
//     corner_light_maps.png   4 maps (one per corner SiPM) of the beam (x,y)
//                             weighted by that corner's detected photons, laid out
//                             in beam's-eye view -> where the shower must sit to
//                             light each corner.
//     dominant_corner_map.png the transverse plane painted by WHICH corner sees
//                             the most light at each (x,y) -> the detector's
//                             effective position partition (red/yellow/green/blue).
//
//   QUANTITATIVE
//     quadrant_vs_corner.png  beam quadrant vs detecting corner response matrix
//                             (+ printed row-normalized % table).
//     position_resolution.png light-barycentre reco vs true (x and y) and the
//                             residuals with a Gaussian sigma -> the transverse
//                             position resolution of the 4-corner readout.
//
//   root -l -b -q 'analysis/plot_photon_origin.C("build/radical_output.root")'

static const char* kTag[4]   = {"TR", "TL", "BR", "BL"};
static const char* kMapNm[4] = {"CornerLightMap_TR", "CornerLightMap_TL",
                                "CornerLightMap_BR", "CornerLightMap_BL"};

void plot_photon_origin(const char* fname = "build/radical_output.root",
                        const char* elabel = "") {
    TFile* f = TFile::Open(fname);
    if (!f || f->IsZombie()) { printf("ERROR: cannot open %s\n", fname); return; }
    gStyle->SetOptStat(0);

    // Per-energy plots go into build/plots/<elabel>/ if a label is given,
    // otherwise straight into build/plots/.
    TString plotDir = "build/plots";
    if (strlen(elabel) > 0) plotDir = TString("build/plots/") + elabel;
    gSystem->mkdir(plotDir, true);

    TH2* map[4];
    for (int i = 0; i < 4; i++) map[i] = (TH2*)f->Get(kMapNm[i]);

    // ── (1) QUALITATIVE: 4 per-corner light maps, in beam's-eye 2x2 layout ────
    const int pad[4] = {2, 1, 4, 3};   // TR->2, TL->1, BR->4, BL->3
    TCanvas* c1 = new TCanvas("c1", "Corner light maps", 900, 840);
    c1->Divide(2, 2, 0.002, 0.002);
    for (int i = 0; i < 4; i++) {
        c1->cd(pad[i]);
        gPad->SetRightMargin(0.13);
        if (map[i]) {
            map[i]->SetTitle(Form("Corner %s SiPM: detected light vs beam (x,y)", kTag[i]));
            map[i]->Draw("COLZ");
        }
    }
    c1->SaveAs(plotDir + "/corner_light_maps.png");

    // ── (2) QUALITATIVE: dominant-corner partition map ────────────────────────
    // For each (x,y) bin, paint the index (1..4) of the corner with the most light.
    if (map[0] && map[1] && map[2] && map[3]) {
        int nx = map[0]->GetNbinsX(), ny = map[0]->GetNbinsY();
        TH2D* dom = new TH2D("dom",
            "Dominant corner SiPM vs beam position;beam x (mm);beam y (mm)",
            nx, -7., 7., ny, -7., 7.);
        for (int ix = 1; ix <= nx; ix++)
            for (int iy = 1; iy <= ny; iy++) {
                double best = 0; int who = 0;
                for (int k = 0; k < 4; k++) {
                    double v = map[k]->GetBinContent(ix, iy);
                    if (v > best) { best = v; who = k + 1; }   // 1=TR,2=TL,3=BR,4=BL
                }
                dom->SetBinContent(ix, iy, who);   // 0 = no light (stays white)
            }
        Int_t colors[4] = {kRed, kYellow, kGreen + 1, kBlue};
        gStyle->SetPalette(4, colors);
        dom->SetMinimum(0.5);
        dom->SetMaximum(4.5);
        TCanvas* c2 = new TCanvas("c2", "Dominant corner", 720, 660);
        c2->SetRightMargin(0.04);
        dom->Draw("COL");
        // legend tying colour -> corner
        TLegend* lg = new TLegend(0.01, 0.90, 0.99, 0.99);
        lg->SetNColumns(4);
        for (int k = 0; k < 4; k++) {
            TMarker* mk = new TMarker(0, 0, 21);
            mk->SetMarkerColor(colors[k]);
            lg->AddEntry(mk, Form("%s", kTag[k]), "p");
        }
        lg->Draw();
        c2->SaveAs(plotDir + "/dominant_corner_map.png");
        gStyle->SetPalette(kBird);   // restore default for later canvases
    }

    // ── (3) QUANTITATIVE: beam quadrant -> detecting corner response matrix ───
    TH2* m = (TH2*)f->Get("Quadrant_vs_Corner");
    if (m) {
        for (int i = 0; i < 4; i++) {
            m->GetXaxis()->SetBinLabel(i + 1, kTag[i]);
            m->GetYaxis()->SetBinLabel(i + 1, kTag[i]);
        }
        m->GetXaxis()->SetTitle("beam quadrant");
        m->GetYaxis()->SetTitle("detecting corner");
        TCanvas* c3 = new TCanvas("c3", "Quadrant vs corner", 660, 600);
        c3->SetRightMargin(0.13);
        gStyle->SetPaintTextFormat(".0f");
        m->SetMarkerSize(1.8);
        m->Draw("COLZ TEXT");
        c3->SaveAs(plotDir + "/quadrant_vs_corner.png");

        printf("\nBeam quadrant -> detecting corner (%% of detected photons, per row):\n");
        printf("  beam\\corner     TR      TL      BR      BL\n");
        printf("  ---------------------------------------------\n");
        for (int r = 0; r < 4; r++) {
            double tot = 0;
            for (int cc = 0; cc < 4; cc++) tot += m->GetBinContent(r + 1, cc + 1);
            printf("  %-10s", kTag[r]);
            for (int cc = 0; cc < 4; cc++)
                printf("  %6.1f", tot > 0 ? 100. * m->GetBinContent(r + 1, cc + 1) / tot : 0.);
            printf("\n");
        }
    }

    // ── (4) QUANTITATIVE: transverse position reconstruction + resolution ─────
    TH2* rx = (TH2*)f->Get("PosRecoX_vs_TrueX");
    TH2* ry = (TH2*)f->Get("PosRecoY_vs_TrueY");
    TH1* dx = (TH1*)f->Get("PosResidualX");
    TH1* dy = (TH1*)f->Get("PosResidualY");
    if (rx && ry && dx && dy) {
        TCanvas* c4 = new TCanvas("c4", "Position reconstruction", 1000, 860);
        c4->Divide(2, 2);
        c4->cd(1); gPad->SetRightMargin(0.13); rx->Draw("COLZ");
        c4->cd(2); gPad->SetRightMargin(0.13); ry->Draw("COLZ");
        c4->cd(3); dx->SetLineColor(kAzure + 1); dx->SetLineWidth(2); dx->Draw();
        if (dx->GetEntries() > 0) dx->Fit("gaus", "Q");
        c4->cd(4); dy->SetLineColor(kAzure + 1); dy->SetLineWidth(2); dy->Draw();
        if (dy->GetEntries() > 0) dy->Fit("gaus", "Q");
        c4->SaveAs("build/plots/position_resolution.png");

        double sx = dx->GetFunction("gaus") ? dx->GetFunction("gaus")->GetParameter(2) : dx->GetRMS();
        double sy = dy->GetFunction("gaus") ? dy->GetFunction("gaus")->GetParameter(2) : dy->GetRMS();
        printf("\nTransverse position resolution (light barycentre of 4 corner SiPMs):\n");
        printf("  sigma_x = %.2f mm,  sigma_y = %.2f mm   (%.0f reconstructed events)\n",
               sx, sy, dx->GetEntries());
    }

    printf("\nSaved -> build/plots/{corner_light_maps,dominant_corner_map,"
           "quadrant_vs_corner,position_resolution}.png\n");
}
