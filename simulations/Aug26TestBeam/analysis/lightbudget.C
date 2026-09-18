// lightbudget.C — where does the detected light COME FROM?
//
// Every detected photon carries an origin code, and the ntuple stores the
// per-event totals, so the light can be split into the four physical processes
// that produce it. Per material and energy this prints the mean number of
// detected photons in each class:
//
//   Cherenkov    prompt, produced by the shower particles themselves in the
//                quartz/filament. Sub-percent in yield but the FASTEST light
//                there is, so it dominates the leading edge of the pulse.
//   direct       LYSO scintillation (420 nm, 36 ns) that reached the sensor
//                WITHOUT being shifted — it got into the fibre and stayed blue.
//   shifted      LYSO light absorbed by the filament and re-emitted at longer
//                wavelength (the process the capillary exists for). Carries
//                LYSO's 36 ns rise PLUS the shifter's own time constant.
//   filament     the filament's OWN scintillation, from shower energy deposited
//                in it. LuAG:Ce only (25000 ph/MeV, 60 ns); DSB1 does not
//                scintillate at all in this model.
//
// Why it matters: the yield and the timing are set by DIFFERENT classes. Total
// light is dominated by "shifted" plus (for LuAG) "filament", but the timing
// resolution is set by how much PROMPT light exists, because the trigger fires
// on the first few percent of photons. A material can therefore be bright and
// slow at the same time — which is exactly what the simulation says LuAG is.
//
// Usage:  root -l -b -q analysis/lightbudget.C

void lightbudget(const char* base = "build/rootfiles") {
    const char* MATS[2]  = {"dsb1", "luag"};
    const double ENER[6] = {1, 3, 5, 7, 9, 11};
    double npeTot[2][6] = {};

    for (int im = 0; im < 2; ++im) {
        printf("\n=== %s ===\n", MATS[im]);
        printf("%-4s %7s %10s %9s   %10s %10s %10s %10s   %s\n", "E", "events", "<Npe>",
               "Npe/GeV", "Cherenkov", "direct", "shifted", "filament", "fractions Ch/di/sh/fi");
        for (int ie = 0; ie < 6; ++ie) {
            TString fn = Form("%s/%s/E%.0fGeV.root", base, MATS[im], ENER[ie]);
            if (gSystem->AccessPathName(fn)) continue;
            TFile f(fn);
            TTree* t = (TTree*)f.Get("ev");
            if (!t) continue;
            const long n = t->GetEntries();
            // means of the per-event scalar totals (only these branches are read)
            auto mean = [&](const char* col) {
                t->Draw(Form("%s>>h_%s", col, col), "", "goff");
                return t->GetHistogram()->GetMean();
            };
            const double all = mean("Npe"),  ch = mean("NpeCher"),
                         di  = mean("NpeDirect"), sh = mean("NpeWLS"), fi = mean("NpeFil");
            npeTot[im][ie] = all;
            printf("%-4.0f %7ld %10.0f %9.0f   %10.1f %10.1f %10.0f %10.0f   %.4f/%.4f/%.4f/%.4f\n",
                   ENER[ie], n, all, all/ENER[ie], ch, di, sh, fi,
                   all > 0 ? ch/all : 0, all > 0 ? di/all : 0,
                   all > 0 ? sh/all : 0, all > 0 ? fi/all : 0);
            f.Close();
        }
    }

    // The ratio the beam test measures directly: DSB1 response over LuAG response.
    printf("\n=== light-yield ratio, DSB1 / LuAG ===\n");
    printf("%-4s %12s %12s %10s   %s\n", "E", "DSB1 <Npe>", "LuAG <Npe>", "ratio", "measured ratio (SumLG peak)");
    const double mD[6] = {210, 3763, 6769, 9045, 9946, 10721};   // EnergyScanDSB1_summary.txt
    const double mL[6] = {210, 1686, 2962, 4814, 5782, 5696};    // EnergyScan_summary.txt
    for (int ie = 0; ie < 6; ++ie) {
        if (npeTot[0][ie] <= 0 || npeTot[1][ie] <= 0) continue;
        printf("%-4.0f %12.0f %12.0f %10.3f   %.3f%s\n", ENER[ie], npeTot[0][ie], npeTot[1][ie],
               npeTot[0][ie]/npeTot[1][ie], mD[ie]/mL[ie],
               ie == 0 ? "   (1 GeV: both measurements are a fit rail, not a real peak)" : "");
    }
    printf("\nA ratio below 1 means the simulation makes LuAG:Ce the brighter material.\n"
           "The beam test measured the opposite at every energy above 1 GeV.\n");
}
