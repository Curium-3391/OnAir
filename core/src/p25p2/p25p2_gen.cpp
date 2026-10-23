// P25 Phase 2 test signal (see p25p2_gen.h).
#include "dect2/p25p2/p25p2_gen.h"
#include "dect2/p25p2/p25p2_proto.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace dect2 {

namespace {
uint64_t mix(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

void putDibits(std::vector<uint8_t>& out, const uint8_t* d, int n) { out.insert(out.end(), d, d + n); }

void putVoice(std::vector<uint8_t>& out, int channel, int& idx) {
    uint8_t bits[72];
    p25p2VoiceBits(channel, idx++, bits);
    for (int i = 0; i < 36; i++) out.push_back((uint8_t)((bits[2 * i] << 1) | bits[2 * i + 1]));
}

void putFiller(std::vector<uint8_t>& out, int n, uint64_t& rs) {
    for (int i = 0; i < n; i++) { rs = mix(rs); out.push_back((uint8_t)(rs & 3)); }
}

const P25p2GenCall* callAt(const P25p2GenConfig& c, int channel, int sf) {
    for (const auto& k : c.calls)
        if (k.channel == channel && sf >= k.startSf && sf < k.startSf + k.nSf) return &k;
    return nullptr;
}
} // namespace

void p25p2VoiceBits(int channel, int index, uint8_t bits[72]) {
    uint64_t x = mix(((uint64_t)channel << 32) ^ (uint64_t)index);
    for (int i = 0; i < 72; i++) {
        if (i % 64 == 0 && i) x = mix(x);
        bits[i] = (uint8_t)((x >> (i % 64)) & 1);
    }
}

int p25p2VoiceFrames(const P25p2GenConfig& c, int channel) {
    int n = 0;
    for (const auto& k : c.calls) {
        if (k.channel != channel) continue;
        for (int sf = k.startSf; sf < k.startSf + k.nSf && sf < c.superframes; sf++)
            for (int p = 0; p < 6; p++) {
                if (sf == k.startSf && p == 0) continue;
                if (sf == k.startSf + k.nSf - 1 && p == 5) continue;
                if (p < 4) n += 4; else if (p == 4) n += 2;
            }
    }
    return n;
}

std::vector<uint8_t> p25p2Dibits(const P25p2GenConfig& c) {
    std::vector<uint8_t> out;
    uint64_t rs = mix(c.seed);
    putFiller(out, c.leadDibits, rs);
    int vidx[3] = {0, 0, 0};
    size_t sysIdx = 0;
    for (int sf = 0; sf < c.superframes; sf++) {
        for (int s = 0; s < p25p2::kSlotsPerSuperframe; s++) {
            const int channel = (s & 1) + 1, p = s / 2;
            uint8_t isch[p25p2::kIschDibits];
            if (p25p2::slotCarriesSync(s)) p25p2::ischSync(isch); else p25p2::ischInfo(s, sf & 3, isch);
            const size_t start = out.size();
            putDibits(out, isch, p25p2::kIschDibits);
            const P25p2GenCall* call = callAt(c, channel, sf);
            auto sig = [&](bool facch, const std::vector<uint8_t>& mac) {
                uint8_t d[4]; p25p2::duidEncode(facch ? p25p2::kDuidFacch : p25p2::kDuidSacch, d);
                putDibits(out, d, 4);
                uint8_t cw[p25p2::kFacchDibits];
                p25p2::macEncode(facch, mac.data(), cw);
                const int n = facch ? p25p2::kFacchDibits : p25p2::kSacchDibits;
                putDibits(out, cw, n);
                putFiller(out, p25p2::kPayloadDibits - n, rs);
            };
            if (call) {
                const bool first = sf == call->startSf, last = sf == call->startSf + call->nSf - 1;
                if (first && p == 0) sig(true, p25p2::macPtt(p25p2::kFacchMacBytes, call->src, call->group, call->algId));
                else if (last && p == 5) sig(true, p25p2::macEndPtt(p25p2::kFacchMacBytes, call->src, call->group, c.colorCode));
                else if (p == 5)
                    sig(false, call->unitToUnit ? p25p2::macActiveUu(p25p2::kSacchMacBytes, call->svcOpts, call->group, call->src) : p25p2::macActive(p25p2::kSacchMacBytes, call->svcOpts, call->group, call->src));
                else {
                    uint8_t d[4]; p25p2::duidEncode(p < 4 ? p25p2::kDuid4V : p25p2::kDuid2V, d);
                    putDibits(out, d, 4);
                    if (p < 4) { for (int k = 0; k < 4; k++) putVoice(out, channel, vidx[channel]); putFiller(out, 12, rs); }
                    else { for (int k = 0; k < 2; k++) putVoice(out, channel, vidx[channel]); putFiller(out, 84, rs); }   // the ESS area: filler (never decoded)
                }
            } else {
                const bool facch = p != 5;
                const int n = facch ? p25p2::kFacchMacBytes : p25p2::kSacchMacBytes;
                std::vector<uint8_t> mac = c.sysMsgs.empty() ? p25p2::macIdle(n) : p25p2::macSignal(n, {c.sysMsgs[sysIdx++ % c.sysMsgs.size()]});
                sig(facch, mac);
            }
            (void)start;
        }
    }
    return out;
}

std::vector<cf32> p25p2Modulate(const std::vector<uint8_t>& dibits, const P25p2GenConfig& c) {
    const double pi = 3.14159265358979323846;
    const int sps = std::max(2, (int)std::lround(c.rate / p25p2::kSymbolRate));
    const double fs = sps * p25p2::kSymbolRate;
    const int span = 6;
    std::vector<double> g((size_t)(2 * span * sps + 1));
    for (size_t i = 0; i < g.size(); i++) g[i] = p25p2::rrcImpulse(((double)i - span * sps) / sps, 0.2);
    const size_t nSym = dibits.size();
    std::vector<std::complex<double>> sym(nSym);
    int phase8 = 0;   // multiples of pi/4
    for (size_t k = 0; k < nSym; k++) {
        phase8 += p25p2::dibitStep(dibits[k]);
        sym[k] = std::polar(1.0, phase8 * pi / 4);
    }
    const size_t nOut = (nSym + 2 * span) * (size_t)sps;
    std::vector<std::complex<double>> y(nOut);
    for (size_t k = 0; k < nSym; k++) {
        const size_t centre = (k + span) * (size_t)sps;
        for (int j = -span * sps; j <= span * sps; j++) y[centre + (size_t)(j + 0)] += sym[k] * g[(size_t)(j + span * sps)];
    }
    double pw = 0;
    for (const auto& v : y) pw += std::norm(v);
    const double scale = pw > 0 ? 1.0 / std::sqrt(pw / (double)nOut) : 1.0;
    std::mt19937_64 rng(c.seed * 977 + 13);
    std::normal_distribution<double> nd(0.0, 1.0);
    const double nsig = std::sqrt(std::pow(10.0, -c.snrDb / 10.0) * (fs / p25p2::kSymbolRate) / 2.0);
    std::vector<cf32> out(nOut);
    for (size_t n = 0; n < nOut; n++) {
        std::complex<double> v = y[n] * scale * std::polar(1.0, 2 * pi * c.cfoHz * (double)n / fs);
        if (c.snrDb < 200) v += std::complex<double>(nd(rng) * nsig, nd(rng) * nsig);
        out[n] = cf32((float)v.real(), (float)v.imag());
    }
    return out;
}

} // namespace dect2
