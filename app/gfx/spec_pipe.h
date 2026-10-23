// The spectrum-to-picture pipeline's pure steps, shared by the app and the offscreen render harness (tools/spec_render.cpp):
//  - the noise floor of a spectrum (the median of its lowest fifth of bins) and a tracker that follows it with a time constant,
//  - the waterfall's auto range anchored on that floor (the floor sits at a fixed place of the colour map, the range a fixed width),
//  - a spectrum row onto the history columns (the highest bin of each column) as levels 0..255.
// No ImGui, no App. Tested in tests/app/test_spec_look.cpp.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace spipe {

// The noise floor of n bins: the median of the lowest 20 % (about the 10th percentile), from at most ~2048 bins evenly picked (cheap at
// 256k bins). A carrier or a TV block covering most of the span still leaves its gaps as the floor. scratch is reused (no allocation).
inline float noiseFloor(const float* s, size_t n, std::vector<float>& scratch) {
    if (!s || n == 0) return NAN;
    const size_t step = std::max<size_t>(1, n / 2048);
    scratch.clear();
    for (size_t i = 0; i < n; i += step) if (std::isfinite(s[i])) scratch.push_back(s[i]);
    if (scratch.empty()) return NAN;
    const size_t k = scratch.size() / 10;
    std::nth_element(scratch.begin(), scratch.begin() + (long)k, scratch.end());
    return scratch[k];
}

// Follows the floor with a time constant (seconds): the colours never jump with one frame, and a retune settles in about tau.
// A jump of more than `snap` dB (a gain change, a new band) is taken at once.
struct FloorTracker {
    float floor = NAN;
    float tau = 2.f, snap = 15.f;
    float update(float f, float dt) {
        if (!std::isfinite(f)) return floor;
        if (!std::isfinite(floor) || std::fabs(f - floor) > snap) floor = f;
        else floor += (f - floor) * (1.f - std::exp(-std::max(0.f, dt) / std::max(0.01f, tau)));
        return floor;
    }
};

// The auto range: the colour map starts `below` dB under the floor and spans `span` dB, so the floor always lands at below / span of
// the map (its dark end, with the grain of the noise visible) and a signal `span - below` dB over the floor reaches the top.
inline void floorRange(float floorDb, float below, float span, float& lo, float& hi) {
    span = std::max(10.f, span);
    lo = floorDb - below;
    hi = lo + span;
}

// One spectrum row of n bins onto C history columns as levels 0..255 over [minDb, maxDb]: each column the highest bin it covers (no
// carrier lost between columns). The usual case (n a multiple of C) runs a fixed-length max the compiler vectorises.
inline void levelsRow(const float* d, size_t n, int C, float minDb, float maxDb, uint8_t* l) {
    const float inv = 255.f / std::max(1.f, maxDb - minDb);
    if (n >= (size_t)C && n % (size_t)C == 0) {
        const size_t k = n / (size_t)C;
        for (int x = 0; x < C; x++) {
            const float* c = d + (size_t)x * k;
            float m = c[0];
            for (size_t j = 1; j < k; j++) m = std::max(m, c[j]);
            const float v = (m - minDb) * inv;
            l[x] = (uint8_t)(v <= 0.f ? 0 : v >= 255.f ? 255 : (int)(v + 0.5f));
        }
        return;
    }
    for (int x = 0; x < C; x++) {
        const size_t a = (size_t)x * n / (size_t)C, b = std::max(a + 1, (size_t)(x + 1) * n / (size_t)C);
        float m = d[std::min(a, n - 1)];
        for (size_t k = a + 1; k < b && k < n; k++) m = std::max(m, d[k]);
        const float v = (m - minDb) * inv;
        l[x] = (uint8_t)(v <= 0.f ? 0 : v >= 255.f ? 255 : (int)(v + 0.5f));
    }
}

// The averaged trace: an exponential average with time constant tau seconds (0: the raw frame), so the look does not depend on the frame rate
inline void emaStep(float* sm, const float* in, size_t n, float dt, float tau) {
    const float alpha = tau <= 0.001f ? 1.f : 1.f - std::exp(-dt / tau);
    for (size_t i = 0; i < n; i++) sm[i] += alpha * (in[i] - sm[i]);
}

} // namespace spipe
