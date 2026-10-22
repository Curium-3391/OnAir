// Argos test signal (see argos_gen.h).
#include "dect2/argos/argos_gen.h"
#include "dect2/argos/argos_rx.h"
#include "dect2/common/mathconst.h"
#include "dect2/dsp/gen_util.h"
#include "dect2/dsp/portable_rng.h"
#include <algorithm>
#include <cmath>
#include <deque>

namespace dect2::argos {

using dect2::math::kPi;

std::vector<TestPtt> testPtts() {
    return {
        {0x0ABCDE1u, {0x12, 0x34, 0x56}, 24, -12000.0},
        {0x1234567u, {0xCA, 0xFE, 0x00, 0x42, 0x13, 0x37, 0x99}, 56, 3000.0},
        {0x7FEDCBAu, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                      0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F}, 248, 21000.0},
    };
}

std::vector<std::complex<float>> a2BurstSamples(const std::vector<uint8_t>& bits, double rate, double carrierHz, double drift) {
    const double bitT = 1.0 / kBitRate, pre = kCarrierSec, edge = 0.0003;
    const double total = pre + (double)bits.size() * bitT;
    const size_t n = (size_t)std::llround((total + 0.002) * rate);
    std::vector<std::complex<float>> out(n);
    auto level = [&](long half) -> double {
        if (half < 0 || half >= (long)bits.size() * 2) return 0.0;
        const bool one = bits[(size_t)(half / 2)] != 0;
        return ((one == (half % 2 == 0)) ? 1.0 : -1.0) * kPhaseDevRad;   // a 1: +1.1 rad, then -1.1 rad
    };
    for (size_t i = 0; i < n; i++) {
        const double t = (double)i / rate;
        double m = 0, amp = 1;
        if (t >= pre) {
            const double u = (t - pre) / (bitT / 2);
            const long h = (long)std::floor(u);
            const double f = (u - (double)h) * (bitT / 2), halfT = bitT / 2;
            m = level(h);
            if (f < edge / 2) m = level(h - 1) + (m - level(h - 1)) * (0.5 + 0.5 * std::sin(kPi * f / edge));
            else if (halfT - f < edge / 2) m = m + (level(h + 1) - m) * (0.5 - 0.5 * std::sin(kPi * (halfT - f) / edge));
        }
        if (t > total) amp = std::max(0.0, 1.0 - (t - total) / 0.0005);
        if (t < 0.0005) amp = t / 0.0005;
        const double a = 2 * kPi * (carrierHz * t + 0.5 * drift * t * t) + m;
        out[i] = std::complex<float>((float)(amp * std::cos(a)), (float)(amp * std::sin(a)));
    }
    return out;
}

std::vector<std::vector<uint8_t>> testDownlinkFrames() {
    return {
        {0x4F, 0x4E, 0x41, 0x49, 0x52, 0x20, 0x54, 0x45, 0x53, 0x54},                        // "ONAIR TEST"
        {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xFF, 0xFF, 0x7E, 0x7E, 0x00},      // flags and runs of ones inside: zero-bit insertion
        {0xA5, 0x5A, 0x00, 0x11, 0x22, 0x33},
    };
}

double testDoppler(double t, double maxHz, double tau, double tca) {
    if (maxHz == 0) return 0;
    const double d = t - tca;
    return -maxHz * d / std::sqrt(d * d + tau * tau);
}

}  // namespace dect2::argos

namespace dect2 {

namespace {

using dect2::math::kPi;

class ArgosSynth : public ModeSynth {
public:
    ArgosSynth(const SynthConfig& cfg, double rate) : rate_(rate), noise_(cfg.modeOpt[3] > 0 ? (uint32_t)cfg.modeOpt[3] : 1u), rng_(cfg.modeOpt[3] > 0 ? (uint64_t)cfg.modeOpt[3] : 1u) {
        down_ = cfg.modeOpt[0] == 1;
        dial_ = cfg.cfoHz - argosTuning().tuneOffsetHz;
        amp_ = 0.45f; sigma_ = 0.f;
        if (cfg.snrDb > -100) {
            const double sigRel = std::sqrt(rate / (2.0 * 2400.0 * std::pow(10.0, cfg.snrDb / 10.0)));
            amp_ = (float)std::min(0.45, 0.85 / (4.0 * sigRel));
            sigma_ = (float)(sigRel * amp_);
        }
        if (down_) {
            bpsk_ = cfg.modeOpt[1] == 1;
            nrzi_ = cfg.modeOpt[2] == 1;
            dopMax_ = cfg.modeVal[0];
            tau_ = cfg.modeVal[1] > 0 ? cfg.modeVal[1] : 60.0;
            tca_ = cfg.modeVal[3] > 0 ? cfg.modeVal[3] : 40.0;
        } else {
            other_ = cfg.modeOpt[2] == 1;
            drift_ = cfg.modeVal[0];
            gap_ = (int64_t)std::llround((cfg.modeVal[1] > 0 ? cfg.modeVal[1] : 2.0) * rate);
            step_ = cfg.modeVal[2];
            ptts_ = argos::testPtts();
            next_ = (int64_t)(0.3 * rate);
        }
    }
    double sampleRate() const override { return rate_; }
    void generate(cf32* out, size_t n) override {
        if (down_) downlink(out, n); else uplink(out, n);
        if (sigma_ > 0) noise_.add(out, n, sigma_);
    }

private:
    struct Burst { int64_t start; std::vector<std::complex<float>> s; };

    void schedule() {
        const size_t np = ptts_.size() + (other_ ? 1 : 0);
        const size_t i = idx_ % np;
        Burst b;
        b.start = next_;
        if (i < ptts_.size()) {
            const argos::TestPtt& p = ptts_[i];
            const int k = (p.dataBits - 24) / 32;
            const std::vector<uint8_t> bits = argos::buildA2(argos::kTestSync, k, p.id, p.data, p.dataBits);
            const double f = dial_ + p.offsetHz + step_ * (double)(idx_ / np);
            b.s = argos::a2BurstSamples(bits, rate_, f, drift_);
        } else {   // another modulation: carrier, then QPSK symbols at 400 symbol/s (not an Argos-3 waveform, only "not A2")
            const double f = dial_ - 20000.0, symT = 1.0 / 400.0;
            const size_t ns = (size_t)((argos::kCarrierSec + 0.4) * rate_);
            std::vector<int> sym(200);
            for (auto& v : sym) v = (int)(rng_() & 3);
            b.s.resize(ns);
            for (size_t j = 0; j < ns; j++) {
                const double t = (double)j / rate_;
                const double m = t < argos::kCarrierSec ? 0.0 : kPi / 4 + kPi / 2 * sym[(size_t)((t - argos::kCarrierSec) / symT) % sym.size()];
                const double a = 2 * kPi * f * t + m;
                b.s[j] = std::complex<float>((float)std::cos(a), (float)std::sin(a));
            }
        }
        next_ = b.start + (int64_t)b.s.size() + gap_;
        idx_++;
        bursts_.push_back(std::move(b));
    }

    void uplink(cf32* out, size_t n) {
        std::fill(out, out + n, cf32(0.f, 0.f));
        while (next_ < pos_ + (int64_t)n) schedule();
        for (const Burst& b : bursts_) {
            const int64_t k0 = std::max<int64_t>(0, b.start - pos_), k1 = std::min<int64_t>((int64_t)n, b.start + (int64_t)b.s.size() - pos_);
            for (int64_t k = k0; k < k1; k++) out[k] += b.s[(size_t)(pos_ + k - b.start)] * amp_;
        }
        pos_ += (int64_t)n;
        while (!bursts_.empty() && bursts_.front().start + (int64_t)bursts_.front().s.size() < pos_) bursts_.pop_front();
    }

    void refill() {
        const std::vector<std::vector<uint8_t>> fr = argos::testDownlinkFrames();
        auto push = [&](int b) {
            if (nrzi_) { if (!b) lvl_ ^= 1; bits_.push_back((uint8_t)lvl_); } else bits_.push_back((uint8_t)b);
        };
        for (int f = 0; f < 6; f++) for (int i = 0; i < 8; i++) push((0x7E >> i) & 1);   // filler: flags
        for (int b : argos::hdlcFrameBits(fr[fidx_ % fr.size()])) push(b);
        fidx_++;
    }

    void downlink(cf32* out, size_t n) {
        const double dev = bpsk_ ? kPi / 2 : argos::kPhaseDevRad;
        for (size_t i = 0; i < n; i++) {
            const double t = (double)(pos_ + (int64_t)i) / rate_;
            const double f = dial_ + argos::testDoppler(t, dopMax_, tau_, tca_);
            ph_ += 2 * kPi * f / rate_;
            if (ph_ > kPi) ph_ -= 2 * kPi; else if (ph_ < -kPi) ph_ += 2 * kPi;
            const uint64_t h = (uint64_t)(t * 2 * argos::kBitRate);
            while (bits_.size() <= (size_t)(h / 2 - bitBase_)) refill();
            const int b = bits_[(size_t)(h / 2 - bitBase_)];
            const double m = ((b != 0) == (h % 2 == 0) ? 1.0 : -1.0) * dev;
            const double a = ph_ + m;
            out[i] = cf32((float)(amp_ * std::cos(a)), (float)(amp_ * std::sin(a)));
        }
        pos_ += (int64_t)n;
        const uint64_t done = (uint64_t)((double)pos_ / rate_ * argos::kBitRate);
        while (bitBase_ + 1 < done && !bits_.empty()) { bits_.pop_front(); bitBase_++; }
    }

    double rate_;
    genutil::NoiseSource noise_;
    PortableRng rng_;
    bool down_ = false, other_ = false, bpsk_ = false, nrzi_ = false;
    double dial_ = 0, drift_ = 0, step_ = 0, dopMax_ = 0, tau_ = 60, tca_ = 40, ph_ = 0;
    std::vector<argos::TestPtt> ptts_;
    std::deque<Burst> bursts_;
    std::deque<uint8_t> bits_;
    uint64_t bitBase_ = 0;
    int lvl_ = 0;
    size_t fidx_ = 0, idx_ = 0;
    int64_t gap_ = 0, next_ = 0, pos_ = 0;
    float amp_ = 0.45f, sigma_ = 0.f;
};

}  // namespace

std::unique_ptr<ModeSynth> makeArgosSynth(const SynthConfig& cfg, double sampleRate) {
    if (sampleRate <= 0) return nullptr;
    return std::make_unique<ArgosSynth>(cfg, sampleRate);
}

}  // namespace dect2
