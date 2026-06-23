// plot_photon_origin.C — how transverse beam (shower) position maps to SiPM
// (corner) excitation, the photonorigin measurement.
//
// Produces two figures from a random-beam run (run_origin_batch.sh):
//   build/plots/corner_light_maps.png   — 4 maps, one per corner SiPM, of the
//        beam (x,y) weighted by the photons that corner detected. Laid out in the
//        beam's-eye 2x2 so each map sits where its corner is. A map that lights up
//        only near its own corner => sharp position sensitivity; a broad blob =>
//        the shower (Moliere) spreads light across corners.
//   build/plots/quadrant_vs_corner.png  — beam quadrant (row) vs detecting corner
//        (col) response matrix, photon-weighted. Diagonal-dominant = the corner
//        nearest the shower sees the most light.
// Also prints the row-normalized response matrix (each beam quadrant's % split
// across the 4 corners).
//
//   root -l -b -q 'analysis/plot_photon_origin.C("build/radical_output.root")'

void plot_photon_origin(const char* fname = "build/radical_output.root") {
    TFile* f = TFile::Open(fname);
    if (!f || f->IsZombie()) { printf("ERROR: cannot open %s\n", fname); return; }
    gStyle->SetOptStat(0);
    gSystem->mkdir("build/plots", true);

    // --- 4 per-corner light maps, placed in beam's-eye view (TL TR / BL BR) ---
    const char* tag[4]   = {"TR", "TL", "BR", "BL"};
    const char* hname[4] = {"CornerLightMap_TR", "CornerLightMap_TL",
                            "CornerLightMap_BR", "CornerLightMap_BL"};
    const int   pad[4]   = {2, 1, 4, 3};   // TR->pad2, TL->pad1, BR->pad4, BL->pad3

    TCanvas* c1 = new TCanvas("c1", "Corner light maps", 900, 840);
    c1->Divide(2, 2, 0.002, 0.002);
    for (int i = 0; i < 4; i++) {
        TH2* h = (TH2*)f->Get(hname[i]);
        c1->cd(pad[i]);
        gPad->SetRightMargin(0.13);
        if (h) {
            h->SetTitle(Form("Corner %s SiPM: detected light vs beam (x,y)", tag[i]));
            h->Draw("COLZ");
        }
    }
    c1->SaveAs("build/plots/corner_light_maps.png");

    // --- quadrant -> corner response matrix ---
    TH2* m = (TH2*)f->Get("Quadrant_vs_Corner");
    if (m) {
        const char* q[4] = {"TR", "TL", "BR", "BL"};
        for (int i = 0; i < 4; i++) {
            m->GetXaxis()->SetBinLabel(i + 1, q[i]);
            m->GetYaxis()->SetBinLabel(i + 1, q[i]);
        }
        m->GetXaxis()->SetTitle("beam quadrant");
        m->GetYaxis()->SetTitle("detecting corner");
        TCanvas* c2 = new TCanvas("c2", "Quadrant vs corner", 660, 600);
        c2->SetRightMargin(0.13);
        gStyle->SetPaintTextFormat(".0f");
        m->SetMarkerSize(1.8);
        m->Draw("COLZ TEXT");
        c2->SaveAs("build/plots/quadrant_vs_corner.png");

        // row-normalized table: each beam quadrant's % split across the 4 corners
        printf("\nBeam quadrant -> detecting corner (%% of detected photons, per row):\n");
        printf("  beam\\corner     TR      TL      BR      BL\n");
        printf("  ---------------------------------------------\n");
        for (int r = 0; r < 4; r++) {
            double tot = 0;
            for (int cc = 0; cc < 4; cc++) tot += m->GetBinContent(r + 1, cc + 1);
            printf("  %-10s", q[r]);
            for (int cc = 0; cc < 4; cc++)
                printf("  %6.1f", tot > 0 ? 100. * m->GetBinContent(r + 1, cc + 1) / tot : 0.);
            printf("\n");
        }
    } else {
        printf("WARNING: Quadrant_vs_Corner histogram not found.\n");
    }

    printf("\nSaved -> build/plots/corner_light_maps.png, build/plots/quadrant_vs_corner.png\n");
}
