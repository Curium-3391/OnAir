// Pure helpers of the receiver status strip (ui2_shell.cpp, statusLine): the frequency digit split, the CPU level colour and
// the collapse order of the labelled cells on a narrow window. No ImGui here, so tests/app/test_status_strip.cpp covers them.
#pragma once
#include <algorithm>
#include <cstdio>
#include <string>

namespace statusstrip {
struct FreqParts { std::string lead, rest; };
// "851.013" -> {"851", ".013"}: the first three digits (the dot does not count) are the accent part, the rest is bright
inline FreqParts splitFreq(double mhz) {
    char b[48];
    snprintf(b, sizeof b, "%.3f", mhz < 0 ? 0 : mhz);
    const std::string s = b;
    size_t i = 0;
    int digits = 0;
    while (i < s.size() && digits < 3) { if (s[i] != '.') digits++; i++; }
    return {s.substr(0, i), s.substr(i)};
}
// 0 = normal (amber), 1 = busy (orange), 2 = overloaded (red); pct is the process CPU, 100 = one core
constexpr float kCpuBusy = 150.f, kCpuHot = 250.f;
inline int cpuLevel(float pct) { return pct > kCpuHot ? 2 : pct > kCpuBusy ? 1 : 0; }
// the bar fill 0..1 for a machine with `cores` cores (at least 1)
inline float cpuFill(float pct, int cores) { return std::clamp(pct / (100.f * (float)std::max(1, cores)), 0.f, 1.f); }

struct Cell { float cap, bare; };   // width with its caption row and without (a cell is as wide as the wider of caption and value)
struct Widths { Cell freq, mode, rx, state, cpu; float cpuNoLabel, mute, gap; };
struct Plan { bool showCpuLabel = true, showRx = true, captions = true, showCpu = true, showState = true, showMode = true; float used = 0; };
// total width of the visible cells (gaps between them, the mute button at the right edge)
inline float usedWidth(const Widths& w, const Plan& p) {
    float t = w.mute;
    auto add = [&](bool on, float cw) { if (on) t += cw + w.gap; };
    const bool c = p.captions;
    add(true, c ? w.freq.cap : w.freq.bare);
    add(p.showMode, c ? w.mode.cap : w.mode.bare);
    add(p.showRx, c ? w.rx.cap : w.rx.bare);
    add(p.showState, c ? w.state.cap : w.state.bare);
    add(p.showCpu, !p.showCpuLabel ? w.cpuNoLabel : c ? w.cpu.cap : w.cpu.bare);
    return t;
}
// what to drop until it fits: the CPU label, then the receiver number, then the captions, then (last resort) the CPU, the state, the mode
inline Plan plan(float avail, const Widths& w) {
    Plan p;
    auto fits = [&]() { p.used = usedWidth(w, p); return p.used <= avail; };
    if (fits()) return p;
    p.showCpuLabel = false; if (fits()) return p;
    p.showRx = false; if (fits()) return p;
    p.captions = false; if (fits()) return p;
    p.showCpu = false; if (fits()) return p;
    p.showState = false; if (fits()) return p;
    p.showMode = false; fits();
    return p;
}
}  // namespace statusstrip
