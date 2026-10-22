// EDACS whole-system test signal (see edacs_gen.h).
#include "dect2/edacs/edacs_gen.h"
#include "dect2/edacs/edacs_proto.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <random>

namespace dect2 {

std::vector<cf32> edacsSystemSignal(const EdacsGenConfig& c) {
    const double pi = 3.14159265358979323846;
    const size_t total = (size_t)std::llround(c.durSec * c.rate);
    // ---- control channel: random lead-in bits (the slicer thresholds settle on them), then a frame every 30 ms
    std::vector<uint8_t> bits;
    std::mt19937 rng(c.seed);
    for (int i = 0; i < 400; i++) bits.push_back((uint8_t)(rng() & 1));
    const double leadSec = 400.0 / edacs::kBaud, frameSec = (double)edacs::kFrameBits / edacs::kBaud;
    const int nFrames = (int)((c.durSec - leadSec) / frameSec) - 1;
    auto word = [&](edacs::Msg m) { return edacs::build(m, c.layout); };
    for (int k = 0; k < nFrames; k++) {
        const double t = leadSec + k * frameSec;
        uint32_t a, b;
        edacs::Msg s;
        if (k % 10 == 0) { s.type = edacs::MsgType::SiteId; s.site = c.siteId; s.system = c.systemId; a = word(s); s = edacs::Msg(); s.type = edacs::MsgType::ControlChannel; s.lcn = c.controlLcn; s.nChannels = c.nChannels; b = word(s); }
        else {
            // an idle filler word pair that the layout does not interpret (type 0), different every frame
            a = (uint32_t)(rng() & 0x0FFFFFF); b = (uint32_t)(rng() & 0x0FFFFFF);
        }
        for (const auto& call : c.calls) {
            if (!call.announce) continue;
            const double t0 = call.startSec - call.announceLeadSec;
            if (t >= t0 && t < call.endSec && k % 3 == 1) {   // the assignment repeats every third frame while the call lasts
                edacs::Msg m;
                m.type = call.group ? edacs::MsgType::GroupAssign : edacs::MsgType::IndividualAssign; m.lcn = call.lcn; m.gid = call.gid; m.emergency = call.emergency;
                a = word(m);
                edacs::Msg l; l.type = edacs::MsgType::LidInfo; l.lid = call.lid;
                b = word(l);
            }
            if (call.sendEnd && t >= call.endSec && t < call.endSec + 3 * frameSec && k % 3 == 2) {
                edacs::Msg m; m.type = edacs::MsgType::CallEnd; m.lcn = call.lcn;
                a = word(m);
            }
        }
        const auto f = edacs::encodeFrame(a, b, (a * 2654435761u) >> 20, (b * 2246822519u) >> 20);
        bits.insert(bits.end(), f.begin(), f.end());
    }
    for (int i = 0; i < 80; i++) bits.push_back((uint8_t)(rng() & 1));
    const double ctlOff = c.controlHz - c.centreHz;
    std::vector<cf32> sig = edacs::modulate(bits, c.rate, ctlOff, c.controlDevHz, c.controlAmp);
    sig.resize(total, cf32(0, 0));
    // ---- voice channels
    for (const auto& call : c.calls) {
        if (call.lcn < 1 || (size_t)call.lcn > c.lcnFreqHz.size()) continue;
        const double off = c.lcnFreqHz[(size_t)call.lcn - 1] - c.centreHz;
        const size_t i0 = (size_t)std::llround(call.startSec * c.rate), i1 = std::min(total, (size_t)std::llround(call.endSec * c.rate));
        double ph = 0;
        for (size_t i = i0; i < i1; i++) {
            const double t = (double)i / c.rate;
            ph += 2 * pi * (off + call.devHz * std::sin(2 * pi * call.toneHz * t)) / c.rate;
            if (ph > pi) ph -= 2 * pi; else if (ph < -pi) ph += 2 * pi;
            const double ramp = std::min({1.0, (double)(i - i0) / (0.005 * c.rate), (double)(i1 - i) / (0.005 * c.rate)});   // a 5 ms key-up / key-down ramp
            const float a = call.amp * (float)(0.5 - 0.5 * std::cos(pi * ramp));
            sig[i] += cf32(a * (float)std::cos(ph), a * (float)std::sin(ph));
        }
    }
    // ---- carrier offset and noise
    std::mt19937_64 nr(c.seed * 31 + 7);
    std::normal_distribution<double> nd(0.0, 1.0);
    for (size_t i = 0; i < total; i++) {
        std::complex<double> v(sig[i].real(), sig[i].imag());
        if (c.cfoHz != 0) v *= std::polar(1.0, 2 * pi * c.cfoHz * (double)i / c.rate);
        if (c.noise > 0) v += std::complex<double>(nd(nr) * c.noise, nd(nr) * c.noise);
        sig[i] = cf32((float)v.real(), (float)v.imag());
    }
    return sig;
}

} // namespace dect2
