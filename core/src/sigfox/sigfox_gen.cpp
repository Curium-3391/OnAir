// Sigfox test signal (see sigfox_gen.h).
#include "dect2/sigfox/sigfox_gen.h"
#include "dect2/sigfox/sigfox_proto.h"
#include "dect2/sigfox/sigfox_rx.h"
#include "dect2/dsp/gen_util.h"
#include "dect2/dsp/portable_rng.h"
#include <algorithm>
#include <cmath>
#include <deque>

namespace dect2::sigfox {

std::vector<TestDevice> testDevices() {
    return {
        {0x00C0FFEEu, {0x1F, 0x3A, 0x00, 0x7C}, false, false, false},                                           // 4 bytes
        {0x0BADF00Du, {}, true, true, false},                                                                 // a 1 bit message
        {0x00ABCDEFu, {0x48, 0x45, 0x4C, 0x4C, 0x4F, 0x20, 0x4F, 0x4E, 0x41, 0x49, 0x52, 0x21}, false, false, false},   // 12 bytes ("HELLO ONAIR!")
        {0x01234567u, {0x0A, 0x0B, 0x0C}, false, false, true},                                                 // 3 bytes, asks for a downlink
    };
}

}  // namespace dect2::sigfox

namespace dect2 {

namespace {

class SigfoxSynth : public ModeSynth {
public:
    SigfoxSynth(const SynthConfig& cfg, double rate) : rate_(rate), noise_(cfg.modeOpt[3] > 0 ? (uint32_t)cfg.modeOpt[3] : 1u), rng_(cfg.modeOpt[3] > 0 ? (uint64_t)cfg.modeOpt[3] : 1u) {
        baud_ = cfg.modeOpt[0] == 1 ? 600 : 100;
        reps_ = cfg.modeOpt[1] == 1 ? 1 : 3;
        drift_ = cfg.modeVal[0];
        gapSec_ = cfg.modeVal[1] > 0 ? cfg.modeVal[1] : 1.0;
        dial_ = cfg.cfoHz - sigfoxTuning().tuneOffsetHz;
        devs_ = sigfox::testDevices();
        amp_ = 0.3f; sigma_ = 0.f;
        if (cfg.snrDb > -100) {
            // noise per component sigma: carrier power amp^2 over the noise power in 100 Hz (2 sigma^2 * 100 / rate) is snrDb
            const double snr = std::pow(10.0, cfg.snrDb / 10.0);
            double a = 0.3, s = a / std::sqrt(2.0 * snr * 100.0 / rate);
            if (s > 0.2) { s = 0.2; a = s * std::sqrt(2.0 * snr * 100.0 / rate); }
            amp_ = (float)a; sigma_ = (float)s;
        }
        nextSec_ = 0.3;
    }
    double sampleRate() const override { return rate_; }
    void generate(cf32* out, size_t n) override {
        std::fill(out, out + n, cf32(0.f, 0.f));
        const double tEnd = (double)(pos_ + (int64_t)n) / rate_;
        while (nextSec_ < tEnd + 0.5) schedule();
        for (const SigfoxTx& t : live_) sigfoxAddBurst(out, n, pos_, t, rate_, dial_ + t.offsetHz, amp_);
        pos_ += (int64_t)n;
        const double now = (double)pos_ / rate_;
        while (!live_.empty() && live_.front().startSec + frameSec(live_.front()) < now - 0.1) live_.pop_front();
        if (sigma_ > 0) noise_.add(out, n, sigma_);
    }

private:
    double frameSec(const SigfoxTx& t) const {
        int container = 8, li = 0;
        sigfox::layoutFor((int)t.payload.size(), t.singleBit, t.bitValue, container, li);
        return (double)(sigfox::kPreambleBits + sigfox::frameBitsAfterPreamble(container) + 1) / t.baud;
    }
    void schedule() {
        const sigfox::TestDevice& d = devs_[idx_ % devs_.size()];
        SigfoxTx t;
        t.id = d.id; t.seq = (uint32_t)(counter_ & 0xFFF); t.payload = d.payload; t.singleBit = d.singleBit; t.bitValue = d.bitValue;
        t.downlinkRequest = d.downlinkRequest; t.baud = baud_; t.driftHzPerSec = drift_;
        double start = nextSec_, prevOff = 1e9;
        for (int r = 1; r <= reps_; r++) {
            t.rank = r;
            t.startSec = start;
            do t.offsetHz = std::round(rng_.uniform(-80e3, 80e3) / 100.0) * 100.0; while (std::fabs(t.offsetHz - prevOff) < 6e3);
            prevOff = t.offsetHz;
            live_.push_back(t);
            start += frameSec(t) + 0.5;
        }
        nextSec_ = start + gapSec_;
        idx_++;
        if (idx_ % devs_.size() == 0) counter_++;
    }

    double rate_;
    genutil::NoiseSource noise_;
    PortableRng rng_;
    int baud_ = 100, reps_ = 3;
    double drift_ = 0, gapSec_ = 1, dial_ = 0, nextSec_ = 0;
    std::vector<sigfox::TestDevice> devs_;
    std::deque<SigfoxTx> live_;
    int64_t pos_ = 0;
    size_t idx_ = 0;
    uint32_t counter_ = 0x123;
    float amp_ = 0.3f, sigma_ = 0.f;
};

}  // namespace

std::unique_ptr<ModeSynth> makeSigfoxSynth(const SynthConfig& cfg, double sampleRate) {
    if (sampleRate <= 0) return nullptr;
    return std::make_unique<SigfoxSynth>(cfg, sampleRate);
}

}  // namespace dect2
