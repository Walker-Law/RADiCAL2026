// plot_transverse.C — transverse (x-y) shower heatmaps at select layer depths.
//
//   root -l -b -q 'analysis/plot_transverse.C("build/optical_scan_1000/optical_E100GeV.root",100)'
//
// Draws the LYSO lateral energy deposit in the (x,y) plane at three depths that
// span the shower — early, shower max, and tail — illustrating the transverse
// spreading with depth (cf. arXiv:2401.01747 Fig 9, energy at shower max).
//
// Output: build/plots/transverse_heatmap_E<E>.png

#include "TFile.h"
#include "TH2D.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TSystem.h"
#include "TLatex.h"

void plot_transverse(const char* file="build/radical_output.root", double Ebeam=100) {
    gStyle->SetOptStat(0);
    gStyle->SetPalette(kBird);
    gStyle->SetNumberContours(99);

    TFile* f = TFile::Open(file);
    if (!f || f->IsZombie()) { printf("cannot open %s\n", file); return; }

    // Three depths spanning the shower (slice index, label).
    const int NS = 3;
    const char* names[NS]  = {"LateralProfile_Slice0",
                              "LateralProfile_Slice2",
                              "LateralProfile_Slice4"};
    const char* depth[NS]  = {"Layers 0#minus4  (early, z#approx#minus49 mm)",
                              "Layers 10#minus14  (shower max, z#approx#minus11 mm)",
                              "Layers 20#minus24  (tail, z#approx+29 mm)"};

    TCanvas* c = new TCanvas("ctrans", "transverse", 1650, 540);
    c->Divide(NS, 1);

    for (int i = 0; i < NS; i++) {
        c->cd(i+1);
        gPad->SetRightMargin(0.16);
        gPad->SetLeftMargin(0.13);
        gPad->SetBottomMargin(0.12);

        TH2D* h = (TH2D*)f->Get(names[i]);
        if (!h) { printf("missing %s\n", names[i]); continue; }
        h = (TH2D*)h->Clone(Form("trans%d", i));
        h->SetTitle(Form("%.0f GeV  #minus  %s;x (mm);y (mm)", Ebeam, depth[i]));
        h->GetZaxis()->SetTitle("Energy deposit (MeV)");
        h->GetXaxis()->SetTitleOffset(1.1);
        h->GetYaxis()->SetTitleOffset(1.3);
        h->Draw("COLZ");
    }

    gSystem->mkdir("build/plots", kTRUE);
    c->SaveAs(Form("build/plots/transverse_heatmap_E%.0f.png", Ebeam));
    printf("Saved: build/plots/transverse_heatmap_E%.0f.png\n", Ebeam);
}
