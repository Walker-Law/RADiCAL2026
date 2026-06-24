// plot_energy_summary.C — cross-energy comparison of photon-origin -> SiPM response.
//
// Reads build/radical_output_<E>GeV.root for each energy and extracts two
// physics quantities that show whether the SiPM position sensitivity changes
// with shower energy:
//
//   (1) Diagonal fraction (position sensitivity metric)
//       For each beam quadrant, the fraction of detected photons that hit the
//       NEAREST corner SiPM (the diagonal element of Quadrant_vs_Corner).
//       A high diagonal fraction (~100%) = the nearest corner dominates, perfect
//       position sensitivity. As energy rises, the wider Molière shower spreads
//       light across more corners -> diagonal fraction drops.
//
//   (2) Light-barycentre position resolution sigma_x, sigma_y (mm)
//       From PosResidualX/Y: how precisely the 4-corner barycentre reconstructs
//       the true beam impact. Wider shower -> more cross-talk -> worse (larger) sigma.
//
//   (3) Detected photons per event vs energy
//       Shows the photostatistics regime: whether high-energy events are photon-
//       starved or saturated at the 500-step cap.
//
//   (4) Per-energy dominant-corner partition maps side-by-side
//       A visual strip showing how the quadrant boundaries sharpen or blur with E.
//
// Output: build/plots/energy_summary/{diagonal_fraction, position_resolution,
//         photons_vs_energy, partition_strip}.png
//
//   root -l -b -q 'analysis/plot_energy_summary.C()'

static const int    kNE       = 7;
static const double kEnergies[kNE] = {5, 10, 20, 25, 50, 100, 120};
static const char*  kTag[4]   = {"TR", "TL", "BR", "BL"};

// Fit the S-curve A*tanh(B*x) from a profile of PosRecoX_vs_TrueX (or Y),
// return corrected sigma from the residual histogram by remapping the H2.
// Returns -1 if insufficient statistics or fit fails.
double sCurveSigma(TFile* f, bool isX) {
    const char* h2name = isX ? "PosRecoX_vs_TrueX" : "PosRecoY_vs_TrueY";
    TH2* h2 = (TH2*)f->Get(h2name);
    if (!h2 || h2->GetEntries() < 50) return -1;

    TProfile* prof = h2->ProfileX(Form("sc_prof_%s_%lld", isX?"x":"y", (long long)h2));
    prof->SetDirectory(nullptr);
    TF1 fn("scfn", "[0]*TMath::TanH([1]*x)", -7., 7.);
    fn.SetParameters(3.2, 0.25);
    fn.SetParLimits(0, 1.0, 6.0);
    fn.SetParLimits(1, 0.01, 5.0);
    TFitResultPtr r = prof->Fit(&fn, "QRNS");
    if (!r.Get() || !r->IsValid() || r->Status() != 0) return -1;
    double A = fn.GetParameter(0), B = fn.GetParameter(1);
    if (A < 0.5 || B < 0.01) return -1;

    // Walk the H2 and fill corrected residuals
    TH1D hc("hsc_tmp","",120,-12,12);
    int nx = h2->GetNbinsX(), ny = h2->GetNbinsY();
    for (int ix=1; ix<=nx; ix++) {
        double xt = h2->GetXaxis()->GetBinCenter(ix);
        for (int iy=1; iy<=ny; iy++) {
            double n = h2->GetBinContent(ix,iy);
            if (n<=0) continue;
            double xr = h2->GetYaxis()->GetBinCenter(iy);
            double rr = xr/A; if(rr>=1) rr=0.9999; if(rr<=-1) rr=-0.9999;
            hc.Fill(TMath::ATanH(rr)/B - xt, n);
        }
    }
    if (hc.GetEntries() < 10) return -1;
    hc.Fit("gaus","Q");
    TF1* g = hc.GetFunction("gaus");
    return g ? g->GetParameter(2) : hc.GetRMS();
}

// Returns the mean diagonal fraction (%) across the 4 quadrant rows and the
// per-row values, from the Quadrant_vs_Corner H2 in a file.
double diagonalFraction(TFile* f, double perRow[4]) {
    TH2* m = (TH2*)f->Get("Quadrant_vs_Corner");
    if (!m) { for(int i=0;i<4;i++) perRow[i]=0; return 0; }
    double sum = 0;
    for (int r = 0; r < 4; r++) {
        double tot = 0, diag = 0;
        for (int c = 0; c < 4; c++) tot += m->GetBinContent(r+1, c+1);
        diag = m->GetBinContent(r+1, r+1);
        perRow[r] = (tot > 0) ? 100.*diag/tot : 0;
        sum += perRow[r];
    }
    return sum / 4.;
}

void plot_energy_summary() {
    gStyle->SetOptStat(0);
    gSystem->mkdir("build/plots/energy_summary", true);

    // ── collect metrics from each energy file ────────────────────────────────
    double diagMean[kNE], diagTR[kNE], diagTL[kNE], diagBR[kNE], diagBL[kNE];
    double sigX[kNE], sigY[kNE], sigXe[kNE], sigYe[kNE];
    double sigXcorr[kNE], sigYcorr[kNE];   // S-curve corrected
    double nphMean[kNE], nphErr[kNE];
    int    nGood = 0;
    double goodE[kNE];

    for (int ie = 0; ie < kNE; ie++) {
        TString fname = Form("build/radical_output_%.0fGeV.root", kEnergies[ie]);
        TFile* f = TFile::Open(fname);
        if (!f || f->IsZombie()) {
            printf("WARNING: %s not found, skipping.\n", fname.Data());
            diagMean[ie] = sigX[ie] = sigY[ie] = nphMean[ie] = -1;
            continue;
        }
        double pr[4];
        diagMean[ie] = diagonalFraction(f, pr);
        diagTR[nGood]=pr[0]; diagTL[nGood]=pr[1]; diagBR[nGood]=pr[2]; diagBL[nGood]=pr[3];

        TH1* dx = (TH1*)f->Get("PosResidualX");
        TH1* dy = (TH1*)f->Get("PosResidualY");
        TH1* nph = (TH1*)f->Get("PhotonsDetected");
        if (dx && dx->GetEntries() > 0) { dx->Fit("gaus","Q"); TF1* g=dx->GetFunction("gaus");
            sigX[ie] = g ? g->GetParameter(2) : dx->GetRMS();
            sigXe[ie] = g ? g->GetParError(2) : 0; }
        else { sigX[ie] = -1; sigXe[ie] = 0; }
        if (dy && dy->GetEntries() > 0) { dy->Fit("gaus","Q"); TF1* g=dy->GetFunction("gaus");
            sigY[ie] = g ? g->GetParameter(2) : dy->GetRMS();
            sigYe[ie] = g ? g->GetParError(2) : 0; }
        else { sigY[ie] = -1; sigYe[ie] = 0; }
        if (nph) { nphMean[ie] = nph->GetMean(); nphErr[ie] = nph->GetMeanError(); }
        else { nphMean[ie] = -1; nphErr[ie] = 0; }

        sigXcorr[ie] = sCurveSigma(f, true);
        sigYcorr[ie] = sCurveSigma(f, false);

        goodE[nGood++] = kEnergies[ie];
        f->Close();
    }
    if (nGood == 0) { printf("ERROR: no energy files found in build/.\n"); return; }

    // ── (1) Diagonal fraction vs energy ─────────────────────────────────────
    // One curve per corner (TR,TL,BR,BL) + mean across all 4.
    TGraph* gMean = new TGraph(nGood);
    TGraph* gTR   = new TGraph(nGood);
    TGraph* gTL   = new TGraph(nGood);
    TGraph* gBR   = new TGraph(nGood);
    TGraph* gBL   = new TGraph(nGood);
    for (int i = 0; i < nGood; i++) {
        gMean->SetPoint(i, goodE[i], diagMean[i]);
        gTR->SetPoint(i, goodE[i], diagTR[i]);
        gTL->SetPoint(i, goodE[i], diagTL[i]);
        gBR->SetPoint(i, goodE[i], diagBR[i]);
        gBL->SetPoint(i, goodE[i], diagBL[i]);
    }
    auto styleG = [](TGraph* g, int col, int style, int width=2) {
        g->SetLineColor(col); g->SetMarkerColor(col);
        g->SetMarkerStyle(style); g->SetLineWidth(width); };
    styleG(gMean, kBlack,  20, 3);
    styleG(gTR,   kRed,    21);
    styleG(gTL,   kOrange, 22);
    styleG(gBR,   kGreen+2,23);
    styleG(gBL,   kAzure+1,24);

    TCanvas* c1 = new TCanvas("c1","Diagonal fraction",800,580);
    c1->SetLogx(); c1->SetGrid();
    TMultiGraph* mg1 = new TMultiGraph();
    mg1->Add(gTR,"LP"); mg1->Add(gTL,"LP"); mg1->Add(gBR,"LP"); mg1->Add(gBL,"LP");
    mg1->Add(gMean,"LP");
    mg1->Draw("A");
    mg1->SetTitle("Position sensitivity vs beam energy;"
                  "Beam energy (GeV);"
                  "Diagonal fraction (% light in nearest corner)");
    mg1->GetYaxis()->SetRangeUser(0, 105);
    TLegend* lg1 = new TLegend(0.55,0.18,0.88,0.45);
    lg1->AddEntry(gMean,"Mean (all quadrants)","lp");
    lg1->AddEntry(gTR,"TR quadrant","lp");
    lg1->AddEntry(gTL,"TL quadrant","lp");
    lg1->AddEntry(gBR,"BR quadrant","lp");
    lg1->AddEntry(gBL,"BL quadrant","lp");
    lg1->Draw();
    c1->SaveAs("build/plots/energy_summary/diagonal_fraction.png");

    // ── (2) Position resolution sigma vs energy: raw and S-curve corrected ───
    std::vector<double> eX, eY, sX, sY, eXe, eYe, sXe, sYe;
    std::vector<double> eXc, eYc, sXc, sYc;
    for (int i = 0; i < nGood; i++) {
        if (sigX[i] > 0)      { eX.push_back(goodE[i]);  sX.push_back(sigX[i]);
                                 eXe.push_back(0);         sXe.push_back(sigXe[i]); }
        if (sigY[i] > 0)      { eY.push_back(goodE[i]);  sY.push_back(sigY[i]);
                                 eYe.push_back(0);         sYe.push_back(sigYe[i]); }
        if (sigXcorr[i] > 0) { eXc.push_back(goodE[i]); sXc.push_back(sigXcorr[i]); }
        if (sigYcorr[i] > 0) { eYc.push_back(goodE[i]); sYc.push_back(sigYcorr[i]); }
    }
    TCanvas* c2 = new TCanvas("c2","Position resolution",800,580);
    c2->SetLogx(); c2->SetGrid();
    TMultiGraph* mg2 = new TMultiGraph();
    TLegend* lg2 = new TLegend(0.52,0.60,0.88,0.88);
    if (!eX.empty()) {
        auto gsx = new TGraphErrors(eX.size(),eX.data(),sX.data(),eXe.data(),sXe.data());
        styleG(gsx, kAzure+1, 20); mg2->Add(gsx,"LP");
        lg2->AddEntry(gsx,"#sigma_{x} raw","lp");
    }
    if (!eY.empty()) {
        auto gsy = new TGraphErrors(eY.size(),eY.data(),sY.data(),eYe.data(),sYe.data());
        styleG(gsy, kRed+1, 21); mg2->Add(gsy,"LP");
        lg2->AddEntry(gsy,"#sigma_{y} raw","lp");
    }
    if (!eXc.empty()) {
        auto gsxc = new TGraph(eXc.size(),eXc.data(),sXc.data());
        styleG(gsxc, kAzure+1, 24); gsxc->SetLineStyle(2); mg2->Add(gsxc,"LP");
        lg2->AddEntry(gsxc,"#sigma_{x} S-curve corr.","lp");
    }
    if (!eYc.empty()) {
        auto gsyc = new TGraph(eYc.size(),eYc.data(),sYc.data());
        styleG(gsyc, kRed+1, 25); gsyc->SetLineStyle(2); mg2->Add(gsyc,"LP");
        lg2->AddEntry(gsyc,"#sigma_{y} S-curve corr.","lp");
    }
    if (mg2->GetListOfGraphs() && mg2->GetListOfGraphs()->GetSize() > 0) {
        mg2->Draw("A");
        mg2->SetTitle("Position resolution vs energy: raw vs S-curve corrected;"
                      "Beam energy (GeV);#sigma (mm)");
        lg2->Draw();
    } else {
        TLatex t; t.SetNDC(); t.SetTextSize(0.05);
        t.DrawLatex(0.3,0.5,"insufficient statistics for resolution fit");
    }
    c2->SaveAs("build/plots/energy_summary/position_resolution_vs_energy.png");

    // ── (3) Detected photons per event vs energy ─────────────────────────────
    std::vector<double> eN, nN, eNe, nNe;
    for (int i = 0; i < nGood; i++)
        if (nphMean[i] > 0) { eN.push_back(goodE[i]); nN.push_back(nphMean[i]);
                               eNe.push_back(0); nNe.push_back(nphErr[i]); }
    if (!eN.empty()) {
        TCanvas* c3 = new TCanvas("c3","Photons vs energy",800,580);
        c3->SetLogx(); c3->SetLogy(); c3->SetGrid();
        auto gnph = new TGraphErrors(eN.size(),eN.data(),nN.data(),eNe.data(),nNe.data());
        styleG(gnph, kViolet+1, 20, 2);
        gnph->Draw("ALP");
        gnph->SetTitle("Detected photons per event vs beam energy (500-step cap);"
                       "Beam energy (GeV);Mean detected photons / event");
        c3->SaveAs("build/plots/energy_summary/photons_vs_energy.png");
    }

    // ── (4) Partition strip: dominant-corner maps for all energies side-by-side
    Int_t pcols[4] = {kRed, kYellow, kGreen+1, kBlue};
    gStyle->SetPalette(4, pcols);
    TCanvas* c4 = new TCanvas("c4","Partition strip",200*nGood, 220);
    c4->Divide(nGood, 1, 0.001, 0.001);
    int pad = 1;
    for (int ie = 0; ie < kNE; ie++) {
        TString fname = Form("build/radical_output_%.0fGeV.root", kEnergies[ie]);
        TFile* f = TFile::Open(fname);
        if (!f || f->IsZombie()) continue;
        TH2* maps[4];
        const char* nm[4]={"CornerLightMap_TR","CornerLightMap_TL","CornerLightMap_BR","CornerLightMap_BL"};
        bool ok = true;
        for (int k=0;k<4;k++) { maps[k]=(TH2*)f->Get(nm[k]); if(!maps[k]) { ok=false; break; } }
        if (ok) {
            int nx=maps[0]->GetNbinsX(), ny=maps[0]->GetNbinsY();
            TH2D* dom = new TH2D(Form("dom%d",ie),"",nx,-7,7,ny,-7,7);
            for(int ix=1;ix<=nx;ix++) for(int iy=1;iy<=ny;iy++){
                double best=0; int who=0;
                for(int k=0;k<4;k++){double v=maps[k]->GetBinContent(ix,iy); if(v>best){best=v;who=k+1;}}
                dom->SetBinContent(ix,iy,who);
            }
            dom->SetMinimum(0.5); dom->SetMaximum(4.5);
            c4->cd(pad++);
            gPad->SetTopMargin(0.18); gPad->SetBottomMargin(0.12);
            gPad->SetLeftMargin(0.01); gPad->SetRightMargin(0.01);
            dom->Draw("COL");
            TLatex tl; tl.SetNDC(); tl.SetTextSize(0.14); tl.SetTextAlign(22);
            tl.DrawLatex(0.5, 0.93, Form("%.0f GeV", kEnergies[ie]));
        }
        f->Close();
    }
    c4->SaveAs("build/plots/energy_summary/partition_strip.png");
    gStyle->SetPalette(kBird);

    // ── print summary table ──────────────────────────────────────────────────
    printf("\n%-8s  %8s  %8s  %8s  %8s  %8s  %8s  %8s  %9s  %9s\n",
           "E (GeV)", "diag(%)", "TR(%)", "TL(%)", "BR(%)", "BL(%)",
           "sx(mm)", "sy(mm)", "sx_corr", "sy_corr");
    printf("%s\n", std::string(90,'-').c_str());
    int j=0;
    for (int ie = 0; ie < kNE; ie++) {
        if (diagMean[ie] < 0) continue;
        printf("%-8.0f  %8.1f  %8.1f  %8.1f  %8.1f  %8.1f  %8.2f  %8.2f  %9.2f  %9.2f\n",
               kEnergies[ie], diagMean[ie],
               diagTR[j], diagTL[j], diagBR[j], diagBL[j],
               sigX[ie]>0?sigX[ie]:-1,    sigY[ie]>0?sigY[ie]:-1,
               sigXcorr[ie]>0?sigXcorr[ie]:-1, sigYcorr[ie]>0?sigYcorr[ie]:-1);
        j++;
    }
    printf("\nSaved -> build/plots/energy_summary/{diagonal_fraction,"
           "position_resolution_vs_energy,photons_vs_energy,partition_strip}.png\n");
}
