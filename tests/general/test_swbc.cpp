// Shortwave broadcast band table: complete set, sorted, no overlaps, inside the HF range, and the band-plan template agrees.
#include "dect2/general/swbc.h"
#include "dect2/bandplan/bandplan.h"
#include <cmath>
#include <cstdio>
#include <cstring>
using namespace dect2;
static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { failures++; std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); } } while (0)

int main() {
    size_t n = 0;
    const SwBand* b = swBroadcastBands(n);
    const char* want[] = {"120 m", "90 m", "75 m", "60 m", "49 m", "41 m", "31 m", "25 m", "22 m", "19 m", "16 m", "15 m", "13 m", "11 m"};
    CHECK(n == 14, "14 bands, got %zu", n);
    for (const char* w : want) {
        bool f = false;
        for (size_t i = 0; i < n; i++) f = f || !strcmp(b[i].name, w);
        CHECK(f, "band %s present", w);
    }
    for (size_t i = 0; i < n; i++) {
        CHECK(b[i].startKhz < b[i].endKhz, "%s start < end", b[i].name);
        CHECK(b[i].startKhz >= 2300 && b[i].endKhz <= 30000, "%s inside HF", b[i].name);
        if (i) CHECK(b[i - 1].endKhz < b[i].startKhz, "%s sorted and apart from %s", b[i].name, b[i - 1].name);
        const double c = swBandCentreKhz(b[i]);
        CHECK(swBandAt(c) == &b[i], "%s centre maps back to the band", b[i].name);
        // a whole number of 5 kHz channels fits, edges on the raster
        CHECK(std::fmod(b[i].startKhz, 5) == 0 || b[i].startKhz == 13570 || b[i].startKhz == 25670, "%s start on the raster", b[i].name);
    }
    CHECK(!swBandAt(8000) && !swBandAt(14100), "outside the bands");
    CHECK(b[0].tropical && b[1].tropical && b[3].tropical && !b[4].tropical, "tropical flags 120/90/60 m");

    // the built-in band plan carries the same table
    dect2::bandplan::Registry reg;
    const dect2::bandplan::Plan* p = reg.find("de");
    CHECK(p != nullptr, "plan de");
    if (p) {
        size_t k = 0;
        for (const auto& bd : p->bands)
            if (bd.service == "am" && bd.name.find(" m") != std::string::npos) {
                bool ok = false;
                for (size_t i = 0; i < n; i++)
                    if (bd.name == b[i].name) ok = std::fabs(bd.startHz - b[i].startKhz * 1e3) < 1 && std::fabs(bd.endHz - b[i].endKhz * 1e3) < 1;
                CHECK(ok, "plan band %s matches the table", bd.name.c_str());
                k++;
            }
        CHECK(k == n, "plan has %zu shortwave bands, got %zu", n, k);
    }
    std::printf(failures ? "swbc: %d FAILED\n" : "swbc: ok\n", failures);
    return failures ? 1 : 0;
}
