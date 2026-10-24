// FT8 multi-signal slot benchmark: K stations at SNRs spread over -25 .. 0 dB (2500 Hz), DT -0.5 .. +1.5 s, random frequencies (some
// overlapping), several slots. Prints the share decoded per SNR bin, false decodes and the CPU seconds per slot (user+sys of the process).
// FTXMULTI_SLOTS (default 2), FTXMULTI_K (default 40), FTXMULTI_MODE (0 FT8, 1 FT4, 2 FT2) set the size; FTXMULTI_LO / _HI the SNR range.
#include "hfdig_wsjt_testutil.h"
#include "../common/test_util.h"
#include <sys/resource.h>
using namespace dect2;
static std::string callOf(int i) {
    static const char* pre[] = {"K", "W", "N", "DL", "G", "JA", "VK", "F", "OH", "SM"};
    char b[16];
    snprintf(b, sizeof b, "%s%d%c%c", pre[i % 10], (i / 10) % 10, 'A' + (i / 100) % 26, 'A' + (i * 7) % 26);
    return b;
}
static double cpu() { rusage r; getrusage(RUSAGE_SELF, &r); return r.ru_utime.tv_sec + r.ru_stime.tv_sec + 1e-6 * (r.ru_utime.tv_usec + r.ru_stime.tv_usec); }
static double env(const char* n, double d) { const char* s = getenv(n); return s ? atof(s) : d; }
int main() {
    const int slots = (int)env("FTXMULTI_SLOTS", 2), K = (int)env("FTXMULTI_K", 40), mode = (int)env("FTXMULTI_MODE", 0);
    const double lo = env("FTXMULTI_LO", -25), hi = env("FTXMULTI_HI", 0);
    const int nb = 5;   // bins of the SNR range
    int ok[nb] = {}, all[nb] = {}, wrong = 0, total = 0;
    double cpuSum = 0, msSum = 0;
    for (int t = 0; t < slots; t++) {
        ftxtest::Gauss g(900 + t * 17);
        std::vector<wsjttest::St> st;
        for (int i = 0; i < K; i++) {
            const std::string a = callOf(i + 13 * t), b = callOf(i + 13 * t + 37);
            const std::string msg = i % 3 == 0 ? "CQ " + a + " FN42" : i % 3 == 1 ? a + " " + b + " -" + std::to_string(10 + i % 9) : a + " " + b + " RR73";
            st.push_back({msg, 300 + 2500 * g.u(), lo + (hi - lo) * (i + 0.5) / K, -0.5 + 2.0 * g.u()});
        }
        wsjttest::Imp imp;
        const auto a = wsjttest::audio(mode, st, 1, imp, 5000 + t);
        HfdigFtxTelemetry tel;
        const double c0 = cpu();
        const auto d = wsjttest::decode(a, mode, imp, ftxtest::kT0, &tel);
        cpuSum += cpu() - c0;
        msSum += tel.decodeMs[mode];
        for (const auto& s : st) {
            const int b = std::min(nb - 1, (int)((s.snrDb - lo) / (hi - lo) * nb));
            all[b]++; ok[b] += wsjttest::find(d, s.msg) != nullptr;
        }
        for (const auto& x : d) { bool sent = false; for (const auto& s : st) sent |= x.msg == s.msg; if (!sent) { wrong++; printf("  false \"%s\" %.1f dB\n", x.msg.c_str(), x.snrDb); } }
        total += (int)d.size();
    }
    int O = 0, A = 0;
    for (int b = 0; b < nb; b++) {
        printf("SNR %5.1f .. %5.1f dB: %d of %d\n", lo + (hi - lo) * b / nb, lo + (hi - lo) * (b + 1) / nb, ok[b], all[b]);
        O += ok[b]; A += all[b];
    }
    printf("TOTAL %d of %d decoded, %d false, cpu %.2f s/slot, decodeMs %.0f\n", O, A, wrong, cpuSum / slots, msSum / slots);
    return 0;
}
