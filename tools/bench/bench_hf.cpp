// CPU cost and decode count of the HF weak-signal slot decoders (FT8 FT4 FT2 WSPR JT65 JT9 Q65 JT4 FST4 MSK144) on synthetic slots with N stations.
//   bench_hf <mode> [stations] [slots] [snr_lo] [snr_hi]    prints: CPU s, s of signal, cost (CPU s per s of signal), decodes, digest of the decoded set, peak RSS
// Built by tools/bench/bench_hf.sh (needs the unit-test helpers in tests/hfdig).
#include "../../tests/hfdig/hfdig_wsjt_testutil.h"
#include <sys/resource.h>
#include <cstdlib>
#include <set>
using namespace dect2;

static double cpuSec() {
    struct rusage r; getrusage(RUSAGE_SELF, &r);
    return r.ru_utime.tv_sec + r.ru_stime.tv_sec + 1e-6 * (r.ru_utime.tv_usec + r.ru_stime.tv_usec);
}
static std::string callOf(int i) {
    static const char* pre[] = {"K", "W", "N", "DL", "G", "JA", "VK", "F", "OH", "SM"};
    char b[16];
    snprintf(b, sizeof b, "%s%d%c%c", pre[i % 10], (i / 10) % 10, 'A' + (i / 100) % 26, 'A' + (i * 7) % 26);
    return b;
}
int main(int argc, char** argv) {
    if (argc < 2) { puts("bench_hf <ft8|ft4|ft2|wspr|jt65|jt9|q65|fst4|msk144|jt4> [stations=20] [slots=2] [snrLo=-18] [snrHi=-4]"); return 1; }
    const char* names[] = {"ft8", "ft4", "ft2", "wspr", "jt65", "jt9", "q65", "fst4", "msk144", "fst4w", "jt4"};
    int mode = -1;
    for (int i = 0; i < 11; i++) if (!strcmp(argv[1], names[i])) mode = i;
    if (mode < 0) return 1;
    const int N = argc > 2 ? atoi(argv[2]) : 20, slots = argc > 3 ? atoi(argv[3]) : 2;
    const double lo = argc > 4 ? atof(argv[4]) : -18, hi = argc > 5 ? atof(argv[5]) : -4;
    const double bw = mode <= 2 ? 2600 : mode == 3 ? 180 : 2400;
    const double f0 = mode == 3 ? 1420 : 300;
    std::vector<wsjttest::St> st;
    ftxtest::Gauss g(7);
    for (int i = 0; i < N; i++) {
        std::string a = callOf(i), b = callOf(i + 37);
        std::string msg;
        if (mode == 3) msg = a + " FN42 37";
        else switch (i % 3) { case 0: msg = "CQ " + a + " FN42"; break; case 1: msg = a + " " + b + " -" + std::to_string(5 + i % 15); break; default: msg = a + " " + b + " RR73"; }
        double hz = f0 + (N > 1 ? bw * i / N : 0) + (mode == 3 ? 0 : 3.0 * (i % 4));
        st.push_back({msg, hz, lo + (hi - lo) * g.u(), -0.3 + 0.6 * g.u()});
    }
    wsjttest::Imp imp;
    auto a = wsjttest::audio(mode, st, slots, imp);
    const double sig = (double)a.size() / 8000.0 - 4.0;
    const double c0 = cpuSec();
    auto d = wsjttest::decode(a, mode, imp);
    const double c1 = cpuSec();
    std::set<std::string> uniq;
    uint64_t h = 1469598103934665603ull;
    int right = 0;
    for (const auto& x : d) {
        char b[256]; snprintf(b, sizeof b, "%s|%.1f|%.2f|%.0f", x.msg.c_str(), x.hz, x.dt, x.snrDb);
        uniq.insert(b);
    }
    for (const auto& s : uniq) for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
    for (const auto& s : st) for (const auto& x : d) if (x.msg == s.msg) { right++; break; }
    struct rusage r; getrusage(RUSAGE_SELF, &r);
    printf("%-6s N=%-3d slots=%d  cpu %.3f s  signal %.1f s  cost %.4f  decodes %zu (sent msgs decoded %d/%d)  digest %016llx  rss %.0f MB\n", names[mode], N, slots, c1 - c0, sig,
           (c1 - c0) / sig, d.size(), right, N, (unsigned long long)h, r.ru_maxrss / 1048576.0);
    return 0;
}
