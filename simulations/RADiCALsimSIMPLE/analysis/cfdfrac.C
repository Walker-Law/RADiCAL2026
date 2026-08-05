// cfdfrac.C — OFFLINE estimator scan on the stored photon waveform.
// Usage:  root -l -b -q 'analysis/cfdfrac.C("build/rootfiles/run_a_f05/E120GeV.root")'
//
// This is the payoff of the 2026-08-02 light-recorder architecture: because
// every event stores its complete photon record (phT/phId/phWls), ANY timing
// estimator can be evaluated here, offline, with no cluster time. This macro
// answers the two questions the CFD switch opened (ROADMAP gaps A1b, A1c):
//
//  1. FRACTION SCAN — recompute the quantile trigger at several fractions
//     (first photon, 0.5%, 1%, 2%, 5%, 10%, 20%) and core-fit sigma_t for
//     each. Shows whether the in-sim 5% choice is anywhere near optimal.
//
//  2. PROMPT CONTAMINATION (A1b) — for each fraction, what part of the
//     photons at-or-before the threshold photon are prompt Cherenkov
//     (phWls==0) rather than WLS-shifted? If the threshold sits inside the
//     prompt precursor, the "CFD" is timing on Cherenkov, not on the WLS
//     pulse — DSB's documented failure mode. Small fractions are MORE
//     exposed, since the prompt photons are the earliest ones.
//
// Same fiducial + core-fit conventions as scan.C so numbers are comparable.
// The stored dTcfd column is refit too, as a cross-check: the recomputed 5%
// row must reproduce it (same photons, same quantile).

#include <vector>
#include <algorithm>
#include <cmath>

// Distance to the nearest of the 5 fiber holes (center + 4 corners at
// (+-3.5, +-3.5) mm) — C++ twin of the kHoleDist cut string in scan.C.
static double holeDist(double x, double y) {
    double d = std::sqrt(x*x + y*y);
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2)
            d = std::min(d, std::sqrt((x - 3.5*sx)*(x - 3.5*sx) +
                                      (y - 3.5*sy)*(y - 3.5*sy)));
    return d;
}

// Two-pass Gaussian core fit with adaptive binning (~8 events/bin, clamped
// [20,300] — D9: a fixed-bin fit on low stats once faked a result).
static void coreFit(std::vector<double>& v, double& sg, double& sgErr) {
    sg = sgErr = 0;
    const int n = v.size();
    if (n < 50) return;
    double m = 0, r = 0;
    for (double d : v) m += d;
    m /= n;
    for (double d : v) r += (d - m)*(d - m);
    r = std::sqrt(r / n);
    if (r <= 0) return;
    const int nb = std::min(300, std::max(20, n/8));
    TH1D h("h", "", nb, m - 5*r, m + 5*r);
    for (double d : v) h.Fill(d);
    h.Fit("gaus", "Q0", "", m - 3*r, m + 3*r);
    TF1* g = h.GetFunction("gaus");
    if (!g) return;
    double mu = g->GetParameter(1); sg = g->GetParameter(2);
    if (sg <= 0) return;
    h.Fit("gaus", "Q0", "", mu - 2*sg, mu + 2*sg);
    g = h.GetFunction("gaus");
    if (!g) return;
    sg = g->GetParameter(2); sgErr = g->GetParError(2);
}

void cfdfrac(const char* file = "build/rootfiles/run_a_f05/E120GeV.root",
             double rMax = 3.5) {
    TFile f(file);
    if (f.IsZombie()) { printf("cannot open %s\n", file); return; }
    TTree* t = (TTree*)f.Get("ev");
    if (!t || !t->GetBranch("phT")) {
        printf("no photon waveform in %s (pre-2026-08-02 file?)\n", file);
        return;
    }

    double x, y, dTcfd;
    std::vector<double> *phT = nullptr, *phId = nullptr, *phWls = nullptr;
    t->SetBranchAddress("x", &x);
    t->SetBranchAddress("y", &y);
    t->SetBranchAddress("dTcfd", &dTcfd);
    t->SetBranchAddress("phT", &phT);
    t->SetBranchAddress("phId", &phId);
    t->SetBranchAddress("phWls", &phWls);

    // fraction 0 = plain first photon (the retired dTwls-style estimator,
    // here on ALL light — included as the end point of the scan).
    const std::vector<double> fracs = {0., 0.005, 0.01, 0.02, 0.05, 0.10, 0.20};
    const int nf = fracs.size();
    std::vector<std::vector<double>> dT(nf);   // per-fraction event values
    std::vector<double> promptSum(nf, 0.);     // prompt share below threshold
    std::vector<long>   promptN(nf, 0);
    std::vector<double> vDTcfd;                // stored-column cross-check

    const Long64_t N = t->GetEntries();
    long nFid = 0;
    for (Long64_t i = 0; i < N; ++i) {
        t->GetEntry(i);
        if (holeDist(x, y) <= 1.5 || std::sqrt(x*x + y*y) >= rMax) continue;
        ++nFid;
        if (dTcfd > -999.) vDTcfd.push_back(dTcfd);

        // split the flat photon record back into per-end time lists
        // (ends 0-3 corner up, 4-7 corner down; 8/9 center, unused here)
        std::vector<double> T[8];
        std::vector<char>   W[8];
        const size_t np = phT->size();
        for (size_t p = 0; p < np; ++p) {
            const int e = (int)(*phId)[p];
            if (e < 0 || e > 7) continue;
            T[e].push_back((*phT)[p]);
            W[e].push_back((*phWls)[p] > 0.5);
        }
        // sort each end once (times + tags together) — every fraction then
        // reads its quantile straight out of the sorted list
        for (int e = 0; e < 8; ++e) {
            const size_t n = T[e].size();
            std::vector<size_t> idx(n);
            for (size_t j = 0; j < n; ++j) idx[j] = j;
            std::sort(idx.begin(), idx.end(),
                      [&](size_t a, size_t b){ return T[e][a] < T[e][b]; });
            std::vector<double> Ts(n); std::vector<char> Ws(n);
            for (size_t j = 0; j < n; ++j) { Ts[j]=T[e][idx[j]]; Ws[j]=W[e][idx[j]]; }
            T[e].swap(Ts); W[e].swap(Ws);
        }

        for (int jf = 0; jf < nf; ++jf) {
            double sum = 0; int nc = 0;
            for (int c = 0; c < 4; ++c) {
                if (T[c].empty() || T[c+4].empty()) continue;
                auto kOf = [&](size_t n) {
                    size_t k = fracs[jf] > 0 ? (size_t)std::ceil(fracs[jf]*n) : 1;
                    return (k > 0 ? k : 1) - 1;   // 0-based index
                };
                const size_t ku = kOf(T[c].size()), kd = kOf(T[c+4].size());
                sum += T[c+4][kd] - T[c][ku];
                ++nc;
                // prompt share among the k+1 photons at/below the upstream
                // threshold (upstream end is the timing-defining one)
                long npr = 0;
                for (size_t j = 0; j <= ku; ++j) if (!W[c][j]) ++npr;
                promptSum[jf] += (double)npr / (ku + 1);
                ++promptN[jf];
            }
            if (nc > 0) dT[jf].push_back(sum / nc);
        }
    }

    printf("\n%s\n", file);
    printf("events: %lld total, %ld fiducial\n\n", N, nFid);

    double sg, err;
    coreFit(vDTcfd, sg, err);
    printf("stored dTcfd column (cross-check):  sigma_t = %6.1f +- %4.1f ps\n\n",
           1000*sg/2, 1000*err/2);

    printf("%-12s %-22s %s\n", "fraction", "sigma_t (ps)",
           "prompt share below threshold");
    for (int jf = 0; jf < nf; ++jf) {
        coreFit(dT[jf], sg, err);
        const double ps = promptN[jf] ? promptSum[jf]/promptN[jf] : 0;
        printf("%-12s %6.1f +- %-12.1f %5.1f%%%s\n",
               fracs[jf] == 0 ? "1st photon" : Form("%.1f%%", 100*fracs[jf]),
               1000*sg/2, 1000*err/2, 100*ps,
               ps > 0.5 ? "   <-- timing on CHERENKOV, not WLS" : "");
    }
    printf("\nsigma_t = sigma(dT)/2, same convention as scan.C. Prompt share\n"
           "= among the photons at/before the threshold photon (upstream\n"
           "ends), the fraction that are prompt Cherenkov (phWls==0).\n");
}
