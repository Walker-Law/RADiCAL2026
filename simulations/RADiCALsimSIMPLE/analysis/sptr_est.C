// sptr_est.C — does a Cherenkov-cluster estimator survive real SPTR noise?
// Usage:  root -l -b -q 'analysis/sptr_est.C("build/rootfiles/run_a_f05/E120GeV.root")'
//
// CONTEXT (ROADMAP gaps A1d, A3). cfdfrac.C found the first photon at each
// SiPM end is essentially pure prompt Cherenkov (D12) and times BETTER than
// the in-sim 5% CFD (18 vs 31 ps at 120 GeV) -- but a single photon can't be
// a real trigger, since SPTR (~60 ps single-photon jitter, NOT modeled
// anywhere else in this sim) would swamp it. At true light the prompt burst
// is O(10^3) photons/end, not one -- averaging over the whole burst divides
// SPTR down by 1/sqrt(N). This macro tests whether that survives.
//
// Every estimator below is recomputed from the stored phT/phId/phWls
// waveform with SPTR added as a one-time Gaussian smear per photon, so all
// estimators see the SAME noisy data -- including a re-smeared 5% CFD, so
// the comparison to "the CFD" is apples-to-apples (the stored dTcfd column
// has NO smearing and is not what a real device would report either).
//
// Three estimators per SPTR value:
//   CFD5        the in-sim trigger, recomputed on smeared times (the
//               realistic version of what we've been quoting)
//   TRUTH-mean  mean of smeared times where phWls==0 (pure Cherenkov) --
//               uses the truth tag, NOT achievable by a real device; this
//               is the CEILING on what population-separation could buy
//   EARLY-mean  mean of the smeared-sorted photons in an early fraction of
//               each end's light (0.5/1/2%) -- REALIZABLE: no truth used,
//               just "average the first X% of collected light"

#include <vector>
#include <algorithm>
#include <cmath>

static double holeDist(double x, double y) {
    double d = std::sqrt(x*x + y*y);
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2)
            d = std::min(d, std::sqrt((x - 3.5*sx)*(x - 3.5*sx) +
                                      (y - 3.5*sy)*(y - 3.5*sy)));
    return d;
}

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

void sptr_est(const char* file = "build/rootfiles/run_a_f05/E120GeV.root",
              double rMax = 3.5) {
    TFile f(file);
    if (f.IsZombie()) { printf("cannot open %s\n", file); return; }
    TTree* t = (TTree*)f.Get("ev");
    if (!t || !t->GetBranch("phT")) {
        printf("no photon waveform in %s (pre-2026-08-02 file?)\n", file);
        return;
    }

    double x, y;
    std::vector<double> *phT = nullptr, *phId = nullptr, *phWls = nullptr;
    t->SetBranchAddress("x", &x);
    t->SetBranchAddress("y", &y);
    t->SetBranchAddress("phT", &phT);
    t->SetBranchAddress("phId", &phId);
    t->SetBranchAddress("phWls", &phWls);

    const Long64_t N = t->GetEntries();

    // pre-select fiducial events once (avoid re-scanning x/y per SPTR value)
    std::vector<Long64_t> fid;
    for (Long64_t i = 0; i < N; ++i) {
        t->GetEntry(i);
        if (holeDist(x, y) > 1.5 && std::sqrt(x*x + y*y) < rMax) fid.push_back(i);
    }
    printf("\n%s\n", file);
    printf("events: %lld total, %zu fiducial\n", N, fid.size());

    const std::vector<double> sptrList = {0., 30., 60., 100.};   // ps
    const std::vector<int> earlyK = {10, 30, 100, 300, 1000};   // FIXED photon
                                        // count, not a fraction -- a genuine
                                        // early burst is set by particle
                                        // multiplicity, not by total light
    long avgN = 0;

    for (double sptrPs : sptrList) {
        gRandom->SetSeed(1);   // same photons get the same smear draw across
                                // SPTR values -- isolates the noise effect
        const double sptrNs = sptrPs / 1000.0;

        std::vector<double> dCFD, dTruth, dBurst;
        std::vector<std::vector<double>> dEarly(earlyK.size());

        for (Long64_t ev : fid) {
            t->GetEntry(ev);
            std::vector<double> T[8];
            std::vector<char>   W[8];
            const size_t np = phT->size();
            for (size_t p = 0; p < np; ++p) {
                const int e = (int)(*phId)[p];
                if (e < 0 || e > 7) continue;
                double tt = (*phT)[p];
                if (sptrNs > 0) tt += gRandom->Gaus(0., sptrNs);
                T[e].push_back(tt);
                W[e].push_back((*phWls)[p] > 0.5);
                if (sptrPs == 0.) ++avgN;
            }
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

            double sumCFD = 0, sumTruth = 0, sumBurst = 0;
            std::vector<double> sumEarly(earlyK.size(), 0.);
            int nc = 0; bool truthOk = true, burstOk = true;
            for (int c = 0; c < 4; ++c) {
                const size_t nu = T[c].size(), nd = T[c+4].size();
                if (nu == 0 || nd == 0) { truthOk = burstOk = false; continue; }
                ++nc;

                // CFD5, on smeared+resorted times
                auto q05 = [&](std::vector<double>& v) {
                    size_t k = (size_t)std::ceil(0.05 * v.size());
                    return v[(k > 0 ? k : 1) - 1];
                };
                sumCFD += q05(T[c+4]) - q05(T[c]);

                // TRUTH-mean (WRONG, kept as the cautionary number): mean of
                // ALL Cherenkov-tagged photons in the whole event, no time
                // cut. Late-shower Cherenkov drags this to nanoseconds.
                auto chMeanAll = [&](std::vector<double>& v, std::vector<char>& w) -> double {
                    double s = 0; long n = 0;
                    for (size_t j = 0; j < v.size(); ++j) if (!w[j]) { s += v[j]; ++n; }
                    return n > 0 ? s/n : NAN;
                };
                double mu = chMeanAll(T[c], W[c]), md = chMeanAll(T[c+4], W[c+4]);
                if (std::isnan(mu) || std::isnan(md)) truthOk = false;
                else sumTruth += md - mu;

                // BURST-mean (the fix): mean of Cherenkov-tagged photons that
                // arrive BEFORE the first WLS-tagged photon at that end --
                // "before any WLS light shows up" is a genuine early cluster,
                // not a truth-wide tag or a fixed fraction of total light.
                // Still uses the truth tag (ceiling, not realizable) but
                // isolates the right population this time.
                auto burstMean = [&](std::vector<double>& v, std::vector<char>& w) -> double {
                    size_t firstWls = v.size();
                    for (size_t j = 0; j < v.size(); ++j) if (w[j]) { firstWls = j; break; }
                    double s = 0; long n = 0;
                    for (size_t j = 0; j < firstWls; ++j) if (!w[j]) { s += v[j]; ++n; }
                    return n > 0 ? s/n : NAN;
                };
                double bu = burstMean(T[c], W[c]), bd = burstMean(T[c+4], W[c+4]);
                if (std::isnan(bu) || std::isnan(bd)) burstOk = false;
                else sumBurst += bd - bu;

                // EARLY-mean: mean of the first FIXED K smeared-sorted
                // photons (not a fraction -- see earlyK comment above)
                for (size_t jk = 0; jk < earlyK.size(); ++jk) {
                    auto eMean = [&](std::vector<double>& v) {
                        size_t k = std::min((size_t)earlyK[jk], v.size());
                        if (k < 1) return v.empty() ? 0.0 : v[0];
                        double s = 0; for (size_t j = 0; j < k; ++j) s += v[j];
                        return s / k;
                    };
                    sumEarly[jk] += eMean(T[c+4]) - eMean(T[c]);
                }
            }
            if (nc == 4) {
                dCFD.push_back(sumCFD / nc);
                if (truthOk) dTruth.push_back(sumTruth / nc);
                if (burstOk) dBurst.push_back(sumBurst / nc);
                for (size_t jk = 0; jk < earlyK.size(); ++jk)
                    dEarly[jk].push_back(sumEarly[jk] / nc);
            }
        }

        printf("\n--- SPTR = %.0f ps %s---\n", sptrPs,
               sptrPs == 0. ? "(no smearing -- sanity check vs stored dTcfd) " : "");
        double sg, err;
        coreFit(dCFD, sg, err);
        printf("  %-28s sigma_t = %6.1f +- %4.1f ps  (n=%zu)\n",
               "CFD 5% (recomputed)", 1000*sg/2, 1000*err/2, dCFD.size());
        coreFit(dTruth, sg, err);
        printf("  %-28s sigma_t = %6.1f +- %4.1f ps  (n=%zu)  [not realizable -- uses truth tag]\n",
               "TRUTH Cherenkov-mean", 1000*sg/2, 1000*err/2, dTruth.size());
        for (size_t jf = 0; jf < earlyFracs.size(); ++jf) {
            coreFit(dEarly[jf], sg, err);
            printf("  EARLY-mean %4.1f%% of light      sigma_t = %6.1f +- %4.1f ps  (n=%zu)\n",
                   100*earlyFracs[jf], 1000*sg/2, 1000*err/2, dEarly[jf].size());
        }
    }

    printf("\navg photons/end/event (unsmeared): %.0f\n",
           avgN / (double)(fid.size() * 8));
    printf("sigma_t = sigma(dT)/2, same convention as scan.C/cfdfrac.C.\n"
           "SPTR=0 CFD row should match cfdfrac.C's stored-dTcfd cross-check\n"
           "(same photons, same quantile, zero noise).\n");
}
