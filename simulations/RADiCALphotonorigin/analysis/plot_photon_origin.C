// plot_photon_origin.C — where does the detected light originate in the LYSO
// transverse plane, and how does that map to SiPM (corner) excitation?
//
// From a random-beam run (run_origin_batch.sh) it writes five figures into
// build/plots/<elabel>/ and prints quantitative tables:
//
//   QUALITATIVE
//     corner_light_maps.png   4 maps (one per corner SiPM) of the beam (x,y)
//                             weighted by that corner's detected photons.
//     dominant_corner_map.png the transverse plane painted by WHICH corner sees
//                             the most light at each (x,y).
//
//   QUANTITATIVE
//     quadrant_vs_corner.png  beam quadrant vs detecting corner response matrix.
//     position_resolution.png raw barycentre reco vs true + residuals (uncorrected).
//     scurve_correction.png   S-curve fit, corrected reco vs true, and the
//                             side-by-side residual improvement.
//
//   root -l -b -q 'analysis/plot_photon_origin.C("build/radical_output.root")'

static const char* kTag[4]   = {"TR", "TL", "BR", "BL"};
static const char* kMapNm[4] = {"CornerLightMap_TR", "CornerLightMap_TL",
                                "CornerLightMap_BR", "CornerLightMap_BL"};

// ── S-curve correction helpers ───────────────────────────────────────────────
//
// The raw barycentre x_reco is bounded by the corner lever arm (±3.5 mm), so
// it compresses the true position into a tanh-shaped response:
//
//   x_reco ≈ A · tanh(B · x_true)
//
// A = saturation amplitude (~3.5 mm, set by corner positions)
// B = sensitivity scale (set by the Molière light-sharing radius)
//
// Inverse (the S-curve correction):
//   x_corrected = atanh(x_reco / A) / B
//
// We derive A and B in-situ by profiling PosRecoX_vs_TrueX (x_true on x-axis,
// x_reco on y-axis), then fitting. The same approach is applied to Y.

struct SCurve {
    double A = 3.5, B = 0.3;   // defaults; replaced by fit if statistics allow
    bool   ok = false;
    TF1*   fn = nullptr;
    double correct(double xreco) const {
        double r = xreco / A;
        if (r >= 1.) r = 0.9999; if (r <= -1.) r = -0.9999;
        return TMath::ATanH(r) / B;
    }
};

// Fit the S-curve from a ProfileX of the reco-vs-true H2.
// Returns fit quality (true = converged with reasonable parameters).
SCurve fitSCurve(TH2* h2, const char* axis) {
    SCurve sc;
    if (!h2 || h2->GetEntries() < 50) return sc;

    TProfile* prof = h2->ProfileX(Form("prof_%s_%lld", axis, (long long)h2));
    prof->SetDirectory(nullptr);

    sc.fn = new TF1(Form("scurve_%s_%lld", axis, (long long)h2),
                    "[0]*TMath::TanH([1]*x)", -7., 7.);
    sc.fn->SetParameters(3.2, 0.25);       // seed near expected values
    sc.fn->SetParLimits(0, 1.0, 6.0);      // A: must be positive, ≤ lever arm
    sc.fn->SetParLimits(1, 0.01, 5.0);     // B: must be positive
    TFitResultPtr r = prof->Fit(sc.fn, "QRNS");
    if (r.Get() && r->IsValid() && r->Status() == 0) {
        sc.A  = sc.fn->GetParameter(0);
        sc.B  = sc.fn->GetParameter(1);
        sc.ok = (sc.A > 0.5 && sc.B > 0.01);
    }
    return sc;
}

// Build a corrected-residual histogram by walking over the 2D (x_true, x_reco)
// histogram and mapping each reco bin through the S-curve inverse.
// Returns a new TH1D (caller owns it).
TH1D* correctedResidual(TH2* h2, const SCurve& sc, const char* name) {
    TH1D* hc = new TH1D(name,
        Form("%s (S-curve corrected);x_{corr} #minus x_{true} (mm);Events", name),
        120, -12., 12.);
    hc->SetDirectory(nullptr);
    int nx = h2->GetNbinsX(), ny = h2->GetNbinsY();
    for (int ix = 1; ix <= nx; ix++) {
        double xtrue = h2->GetXaxis()->GetBinCenter(ix);
        for (int iy = 1; iy <= ny; iy++) {
            double n = h2->GetBinContent(ix, iy);
            if (n <= 0) continue;
            double xreco = h2->GetYaxis()->GetBinCenter(iy);
            double xcorr = sc.correct(xreco);
            hc->Fill(xcorr - xtrue, n);
        }
    }
    return hc;
}

void plot_photon_origin(const char* fname = "build/radical_output.root",
                        const char* elabel = "") {
    TFile* f = TFile::Open(fname);
    if (!f || f->IsZombie()) { printf("ERROR: cannot open %s\n", fname); return; }
    gStyle->SetOptStat(0);

    TString plotDir = "build/plots";
    if (strlen(elabel) > 0) plotDir = TString("build/plots/") + elabel;
    gSystem->mkdir(plotDir, true);

    TH2* map[4];
    for (int i = 0; i < 4; i++) map[i] = (TH2*)f->Get(kMapNm[i]);

    // ── (1) QUALITATIVE: 4 per-corner light maps ─────────────────────────────
    const int pad[4] = {2, 1, 4, 3};
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
                    if (v > best) { best = v; who = k + 1; }
                }
                dom->SetBinContent(ix, iy, who);
            }
        Int_t colors[4] = {kRed, kYellow, kGreen + 1, kBlue};
        gStyle->SetPalette(4, colors);
        dom->SetMinimum(0.5); dom->SetMaximum(4.5);
        TCanvas* c2 = new TCanvas("c2", "Dominant corner", 720, 660);
        c2->SetRightMargin(0.04);
        dom->Draw("COL");
        TLegend* lg = new TLegend(0.01, 0.90, 0.99, 0.99);
        lg->SetNColumns(4);
        for (int k = 0; k < 4; k++) {
            TMarker* mk = new TMarker(0, 0, 21);
            mk->SetMarkerColor(colors[k]);
            lg->AddEntry(mk, Form("%s", kTag[k]), "p");
        }
        lg->Draw();
        c2->SaveAs(plotDir + "/dominant_corner_map.png");
        gStyle->SetPalette(kBird);
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

    // ── (4) QUANTITATIVE: raw position reconstruction + residuals ─────────────
    TH2* rx = (TH2*)f->Get("PosRecoX_vs_TrueX");
    TH2* ry = (TH2*)f->Get("PosRecoY_vs_TrueY");
    TH1* dx = (TH1*)f->Get("PosResidualX");
    TH1* dy = (TH1*)f->Get("PosResidualY");
    double rawSigX = -1, rawSigY = -1;
    if (rx && ry && dx && dy) {
        TCanvas* c4 = new TCanvas("c4", "Position reconstruction (raw)", 1000, 860);
        c4->Divide(2, 2);
        c4->cd(1); gPad->SetRightMargin(0.13); rx->Draw("COLZ");
        c4->cd(2); gPad->SetRightMargin(0.13); ry->Draw("COLZ");
        c4->cd(3);
        dx->SetLineColor(kAzure + 1); dx->SetLineWidth(2); dx->Draw();
        if (dx->GetEntries() > 0) dx->Fit("gaus", "Q");
        c4->cd(4);
        dy->SetLineColor(kAzure + 1); dy->SetLineWidth(2); dy->Draw();
        if (dy->GetEntries() > 0) dy->Fit("gaus", "Q");
        c4->SaveAs(plotDir + "/position_resolution.png");

        if (dx->GetFunction("gaus")) rawSigX = dx->GetFunction("gaus")->GetParameter(2);
        if (dy->GetFunction("gaus")) rawSigY = dy->GetFunction("gaus")->GetParameter(2);
        printf("\nRaw barycentre resolution:  sigma_x = %.2f mm,  sigma_y = %.2f mm\n",
               rawSigX, rawSigY);
    }

    // ── (5) S-CURVE CORRECTION ────────────────────────────────────────────────
    // Fit x_reco = A·tanh(B·x_true) from PosRecoX_vs_TrueX profile, derive
    // x_corrected = atanh(x_reco/A)/B, and show the residual improvement.
    if (rx && ry) {
        SCurve scX = fitSCurve(rx, "x");
        SCurve scY = fitSCurve(ry, "y");

        TCanvas* c5 = new TCanvas("c5", "S-curve correction", 1200, 860);
        c5->Divide(3, 2);

        // ── Row 1: the S-curves ──────────────────────────────────────────────
        // X S-curve
        c5->cd(1);
        TProfile* profX = rx->ProfileX("profX_disp");
        profX->SetTitle("X response S-curve: x_{reco} vs x_{true};"
                        "x_{true} (mm);x_{reco} (mm)");
        profX->SetMarkerStyle(20); profX->SetMarkerColor(kAzure + 1);
        profX->Draw("E");
        // diagonal reference (ideal linear)
        TF1* ideal = new TF1("ideal","x",-7,7);
        ideal->SetLineColor(kGray+1); ideal->SetLineStyle(2); ideal->Draw("same");
        if (scX.ok) {
            scX.fn->SetLineColor(kRed+1); scX.fn->SetLineWidth(2);
            scX.fn->Draw("same");
            TLegend* lg = new TLegend(0.14,0.72,0.60,0.88);
            lg->AddEntry(ideal,"ideal (x_{reco}=x_{true})","l");
            lg->AddEntry(scX.fn,
                Form("A#cdottanh(Bx):  A=%.2f mm, B=%.3f mm^{-1}",scX.A,scX.B),"l");
            lg->Draw();
        }

        // Y S-curve
        c5->cd(2);
        TProfile* profY = ry->ProfileX("profY_disp");
        profY->SetTitle("Y response S-curve: y_{reco} vs y_{true};"
                        "y_{true} (mm);y_{reco} (mm)");
        profY->SetMarkerStyle(20); profY->SetMarkerColor(kAzure + 1);
        profY->Draw("E");
        TF1* idealY = new TF1("idealY","x",-7,7);
        idealY->SetLineColor(kGray+1); idealY->SetLineStyle(2); idealY->Draw("same");
        if (scY.ok) {
            scY.fn->SetLineColor(kRed+1); scY.fn->SetLineWidth(2);
            scY.fn->Draw("same");
            TLegend* lg = new TLegend(0.14,0.72,0.60,0.88);
            lg->AddEntry(idealY,"ideal","l");
            lg->AddEntry(scY.fn,
                Form("A#cdottanh(Bx):  A=%.2f mm, B=%.3f mm^{-1}",scY.A,scY.B),"l");
            lg->Draw();
        }

        // ── Corrected reco-vs-true H2s (built by remapping the existing H2) ──
        // For each (x_true, x_reco) bin in the H2, apply correction and fill a
        // new H2 in (x_true, x_corrected) space.
        auto remapH2 = [](TH2* h2in, const SCurve& sc,
                          const char* name, const char* title) -> TH2D* {
            int nx = h2in->GetNbinsX(), ny = h2in->GetNbinsY();
            TH2D* hout = new TH2D(name, title, nx, -7., 7., 56, -7., 7.);
            hout->SetDirectory(nullptr);
            for (int ix = 1; ix <= nx; ix++)
                for (int iy = 1; iy <= ny; iy++) {
                    double n = h2in->GetBinContent(ix, iy);
                    if (n <= 0) continue;
                    double xt = h2in->GetXaxis()->GetBinCenter(ix);
                    double xr = h2in->GetYaxis()->GetBinCenter(iy);
                    hout->Fill(xt, sc.ok ? sc.correct(xr) : xr, n);
                }
            return hout;
        };

        TH2D* rxCorr = remapH2(rx, scX,
            "PosCorr_X", "X corrected: x_{corr} vs x_{true};x_{true} (mm);x_{corr} (mm)");
        TH2D* ryCorr = remapH2(ry, scY,
            "PosCorr_Y", "Y corrected: y_{corr} vs y_{true};y_{true} (mm);y_{corr} (mm)");

        c5->cd(3); gPad->SetRightMargin(0.13);
        rxCorr->Draw("COLZ");
        TF1* diagX = new TF1("diagX","x",-7,7);
        diagX->SetLineColor(kWhite); diagX->SetLineStyle(2); diagX->SetLineWidth(2);
        diagX->Draw("same");

        // ── Row 2: residual comparison (raw vs corrected) ────────────────────
        TH1D* dxCorr = correctedResidual(rx, scX, "dxCorr");
        TH1D* dyCorr = correctedResidual(ry, scY, "dyCorr");

        // X residuals
        c5->cd(4);
        if (dx) {
            dx->SetLineColor(kAzure + 1); dx->SetLineWidth(2);
            dx->SetTitle("X residual: raw vs S-curve corrected;"
                         "residual (mm);Events");
            double sc4 = dx->GetMaximum();
            dx->Draw("HIST");
            dxCorr->SetLineColor(kRed + 1); dxCorr->SetLineWidth(2);
            dxCorr->Scale(sc4 / (dxCorr->GetMaximum() > 0 ? dxCorr->GetMaximum() : 1));
            dxCorr->Draw("HIST same");
            if (dx->GetEntries() > 0)    dx->Fit("gaus","Q+");
            if (dxCorr->GetEntries() > 0) dxCorr->Fit("gaus","Q+");
            TLegend* lg = new TLegend(0.55,0.72,0.88,0.88);
            lg->AddEntry(dx,"raw","l");
            lg->AddEntry(dxCorr,"S-curve corrected","l");
            lg->Draw();
        }

        // Y residuals
        c5->cd(5);
        if (dy) {
            dy->SetLineColor(kAzure + 1); dy->SetLineWidth(2);
            dy->SetTitle("Y residual: raw vs S-curve corrected;"
                         "residual (mm);Events");
            double sc5 = dy->GetMaximum();
            dy->Draw("HIST");
            dyCorr->SetLineColor(kRed + 1); dyCorr->SetLineWidth(2);
            dyCorr->Scale(sc5 / (dyCorr->GetMaximum() > 0 ? dyCorr->GetMaximum() : 1));
            dyCorr->Draw("HIST same");
            if (dy->GetEntries() > 0)    dy->Fit("gaus","Q+");
            if (dyCorr->GetEntries() > 0) dyCorr->Fit("gaus","Q+");
            TLegend* lg = new TLegend(0.55,0.72,0.88,0.88);
            lg->AddEntry(dy,"raw","l");
            lg->AddEntry(dyCorr,"S-curve corrected","l");
            lg->Draw();
        }

        // ── Corrected reco-vs-true Y ─────────────────────────────────────────
        c5->cd(6); gPad->SetRightMargin(0.13);
        ryCorr->Draw("COLZ");
        TF1* diagY = new TF1("diagY","x",-7,7);
        diagY->SetLineColor(kWhite); diagY->SetLineStyle(2); diagY->SetLineWidth(2);
        diagY->Draw("same");

        c5->SaveAs(plotDir + "/scurve_correction.png");

        // print corrected sigma
        double corrSigX = -1, corrSigY = -1;
        if (dxCorr->GetFunction("gaus")) corrSigX = dxCorr->GetFunction("gaus")->GetParameter(2);
        if (dyCorr->GetFunction("gaus")) corrSigY = dyCorr->GetFunction("gaus")->GetParameter(2);
        printf("\nS-curve correction:   A_x=%.2f mm B_x=%.3f   A_y=%.2f mm B_y=%.3f\n",
               scX.A, scX.B, scY.A, scY.B);
        printf("  sigma_x: %.2f mm (raw)  ->  %.2f mm (corrected)\n", rawSigX, corrSigX);
        printf("  sigma_y: %.2f mm (raw)  ->  %.2f mm (corrected)\n", rawSigY, corrSigY);
    }

    printf("\nSaved -> %s/{corner_light_maps,dominant_corner_map,"
           "quadrant_vs_corner,position_resolution,scurve_correction}.png\n",
           plotDir.Data());
}
