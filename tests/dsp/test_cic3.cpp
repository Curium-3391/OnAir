// The exact CIC decimator core (dsp/cic3.h): it matches the plain moving sums, and integrators that have wrapped many times (hours at 30 Msps)
// give the same output as fresh ones. The double integrators it replaced decayed: at D = 307 the output was below the error within a minute.
#include "dect2/dsp/cic3.h"
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>
using namespace dect2;
#include "../common/test_util.h"

int main() {
    std::mt19937_64 rng(3);
    std::normal_distribution<double> nd(0, 0.3);
    for (int D : {4, 307, 614}) {
        const int n = D * 40;
        std::vector<std::complex<double>> x((size_t)n);
        for (auto& v : x) v = {nd(rng), nd(rng)};
        // reference: the cascade of three length-D moving sums, from zero
        auto ref = [&](int end) {   // output after sample end (exclusive), three nested boxcars
            std::complex<double> s = 0;
            for (int a = 0; a < D; a++)
                for (int b = 0; b < D; b++)
                    for (int c = 0; c < D && D <= 8; c++) { const int i = end - 1 - a - b - c; if (i >= 0) s += x[(size_t)i]; }
            return s;
        };
        Cic3 fresh, aged;
        for (int k = 0; k < 3; k++) {   // integrators far past 64 bits of history: arbitrary wrapped states
            aged.ir[k] = rng(); aged.ii[k] = rng(); aged.cr[k] = rng(); aged.ci[k] = rng();
        }
        double worst = 0, worstRef = 0;
        int outs = 0;
        for (int i = 0; i < n; i++) {
            fresh.integrate(x[(size_t)i]);
            aged.integrate(x[(size_t)i]);
            if ((i + 1) % D) continue;
            const std::complex<double> a = fresh.comb(), b = aged.comb();
            if (++outs > 3) worst = std::max(worst, std::abs(a - b));   // after the combs have flushed the old states
            if (D <= 8 && i + 1 >= 3 * D) worstRef = std::max(worstRef, std::abs(a - ref(i + 1)) / (double)(D * D * D));
        }
        CHECK(worst == 0, "D %d: aged integrators differ from fresh ones by %g", D, worst);
        if (D <= 8) CHECK(worstRef < 1e-5, "D %d: differs from the moving sums by %g", D, worstRef);
        printf("D %d: aged vs fresh %g, vs reference %g\n", D, worst, worstRef);
    }
    // NaN and huge inputs do not break it
    Cic3 c;
    c.integrate({NAN, 1e30});
    for (int i = 0; i < 10; i++) c.integrate({0, 0});
    const std::complex<double> o = c.comb();
    CHECK(std::isfinite(o.real()) && std::isfinite(o.imag()), "NaN / huge input");
    return testSummary("cic3");
}
