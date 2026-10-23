// Status strip helpers: frequency digit split, CPU level thresholds, and the cell collapse at many window widths.
#include "../../app/ui/status_strip.h"
#include <cstdio>
using namespace statusstrip;
static int fails = 0;
static void check(bool ok, const char* w) { if (!ok) { printf("FAIL: %s\n", w); fails++; } }
int main() {
    check(splitFreq(851.013).lead == "851" && splitFreq(851.013).rest == ".013", "851.013");
    check(splitFreq(93.9).lead == "93.9" && splitFreq(93.9).rest == "00", "93.900 keeps three digits in the accent");
    check(splitFreq(1090.0).lead == "109" && splitFreq(1090.0).rest == "0.000", "1090");
    check(splitFreq(0.5).lead == "0.50" && splitFreq(0.5).rest == "0", "0.500");
    check(splitFreq(-3).lead + splitFreq(-3).rest == "0.000", "negative clamps");
    check(cpuLevel(0) == 0 && cpuLevel(150) == 0 && cpuLevel(150.1f) == 1 && cpuLevel(250) == 1 && cpuLevel(250.1f) == 2, "cpu thresholds");
    check(cpuFill(-5, 4) == 0 && cpuFill(200, 4) == 0.5f && cpuFill(900, 4) == 1.f && cpuFill(50, 0) == 0.5f, "cpu fill");
    const Widths w{{150, 120}, {70, 50}, {60, 20}, {90, 70}, {110, 90}, 60, 24, 14};
    Plan p = plan(1000, w);
    check(p.showCpuLabel && p.showRx && p.captions && p.showCpu && p.showState && p.showMode, "wide shows all");
    bool sawNoLabel = false, sawNoRx = false, sawNoCap = false;
    int lastLevel = 0;
    for (float a = 1000; a >= 40; a -= 5) {
        p = plan(a, w);
        const int level = !p.showCpuLabel + !p.showRx + !p.captions + !p.showCpu + !p.showState + !p.showMode;
        check(level >= lastLevel, "monotonic as the window narrows");
        lastLevel = level;
        if (!p.showCpuLabel) sawNoLabel = true;
        if (!p.showRx) { sawNoRx = true; check(!p.showCpuLabel, "label goes before receiver"); }
        if (!p.captions) { sawNoCap = true; check(!p.showRx && !p.showCpuLabel, "captions go after receiver"); }
        if (!p.showCpu) check(!p.captions, "cpu goes after captions");
        if (a >= 300) check(p.used <= a, "fits at 300+");
    }
    check(sawNoLabel && sawNoRx && sawNoCap, "all collapse steps reached");
    if (!fails) printf("ok\n");
    return fails;
}
