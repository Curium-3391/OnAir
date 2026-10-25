// User-set receive bandwidth (#274) for the modes that listen to several channels in one capture: VDL2, TETRA and TETRAPOL.
// Automatic (0) leaves the mode's own tuning untouched; a width (0.05 - 2 MHz) sets the analysis passband, the radio's sample rate
// that carries it and the radio's analogue filter. Only these three modes read it, so no other mode changes. The plan is pure
// (tests/state/test_rx_width.cpp); the saved width and the control are in rx_width.cpp.
#pragma once
#include <algorithm>
#include <cmath>

struct RxWidthPlan { double bandwidthMhz = 0, sampleRate = 0, basebandHz = 0; };

constexpr double kRxWidthMinMhz = 0.05, kRxWidthMaxMhz = 2.0;

// engine standards that take the setting: 29 VDL Mode 2, 48 TETRA, 81 TETRAPOL
inline bool rxWidthMode(int stdMode) { return stdMode == 29 || stdMode == 48 || stdMode == 81; }

// widthMhz <= 0: automatic (the mode's own values, passed in). Otherwise the width clamped to 0.05 - 2 MHz; the rate is the
// width over 0.9 (the flat part of a radio's decimation filter), rounded up to 0.25 Msps and never below what the mode needs.
inline RxWidthPlan rxWidthPlan(double widthMhz, double autoBwMhz, double autoRate, double autoBaseband, double minRate) {
    RxWidthPlan p{autoBwMhz, autoRate, autoBaseband};
    if (!(widthMhz > 0)) return p;
    const double bw = std::min(kRxWidthMaxMhz, std::max(kRxWidthMinMhz, widthMhz));
    p.bandwidthMhz = bw;
    p.sampleRate = std::max(minRate, std::ceil(bw / 0.9 / 0.25) * 0.25 * 1e6);
    p.basebandHz = 0.9 * p.sampleRate;
    return p;
}
