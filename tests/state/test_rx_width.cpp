// The receive-bandwidth plan (#274): Auto changes nothing, a width gives a rate that carries it, only VDL2 / TETRA / TETRAPOL take it.
#include "../../app/state/rx_width.h"
#include <cstdio>
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
int main() {
    RxWidthPlan a = rxWidthPlan(0, 1.8, 2e6, 1.75e6, 2e5);
    CHECK(a.bandwidthMhz == 1.8 && a.sampleRate == 2e6 && a.basebandHz == 1.75e6);   // Auto: the mode's own values
    RxWidthPlan n = rxWidthPlan(-3, 0.4, 2e6, 1.75e6, 250000);
    CHECK(n.sampleRate == 2e6);
    RxWidthPlan w = rxWidthPlan(2.0, 1.8, 2e6, 1.75e6, 2e5);
    CHECK(w.bandwidthMhz == 2.0 && w.sampleRate == 2.25e6 && w.sampleRate >= w.bandwidthMhz * 1e6 && w.basebandHz < w.sampleRate);
    RxWidthPlan big = rxWidthPlan(9, 1.8, 2e6, 1.75e6, 2e5);
    CHECK(big.bandwidthMhz == 2.0);                                                   // clamped to 2 MHz
    RxWidthPlan s = rxWidthPlan(0.01, 0.2, 250000, 200000, 64000);
    CHECK(s.bandwidthMhz == 0.05 && s.sampleRate == 250000 * 1.0 * 1 && s.sampleRate >= 64000);   // 0.05 / 0.9 -> 0.25 Msps
    RxWidthPlan floor = rxWidthPlan(0.25, 0.4, 2e6, 1.75e6, 800000);
    CHECK(floor.sampleRate == 800000);                                               // never below what the mode needs
    CHECK(rxWidthMode(29) && rxWidthMode(48) && rxWidthMode(81));
    CHECK(!rxWidthMode(28) && !rxWidthMode(47) && !rxWidthMode(8) && !rxWidthMode(0) && !rxWidthMode(30));   // every other mode: untouched
    CHECK(!bwCustomTicked(-1, 0) && bwCustomTicked(-1, 0.5) && bwCustomTicked(-1, 2.0));   // never saved: a saved width keeps working, Auto stays standard
    CHECK(bwCustomTicked(1, 0) && !bwCustomTicked(0, 0.5) && bwCustomTicked(1, 0.5));      // a saved tick wins over the width
    CHECK(bwEffective(true, 0.5) == 0.5 && bwEffective(false, 0.5) == 0 && bwEffective(true, 0) == 0);   // unticked: the standard value, the width is kept for later
    std::printf(fails ? "rx_width FAILED\n" : "rx_width ok\n");
    return fails ? 1 : 0;
}
