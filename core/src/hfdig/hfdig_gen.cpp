// HF digital test signal (see hfdig_gen.h): a decoder's test audio on the upper sideband, over noise. The test audio itself comes from
// the decoders' own files.
#include "dect2/hfdig/hfdig_gen.h"
#include "dect2/hfdig/hfdig_rx.h"
#include "dect2/dsp/gen_util.h"
#include "hfdig_usb.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <memory>
#include <vector>

namespace dect2 {

std::unique_ptr<HfdigTestAudio> makeHfdigTestAudio(int which, const SynthConfig& cfg) {
    switch (which) {
    case 0: return makeRttyTestAudio(cfg);
    case 1: return makeSstvTestAudio(cfg);
    case 2: return makeFreedvTestAudio(cfg);
    case 3: case 4: case 5: case 6: case 21: case 22: case 23: case 24: case 25: case 32: return makeFtxTestAudio(cfg);
    case 7: return makeHamdrmTestAudio(cfg);
    case 26: case 27: case 28: case 29: case 30: case 31: return makeKbdTestAudio(cfg);
    default: return nullptr;
    }
}

namespace {

constexpr float kNoiseSigma = 0.02f;   // per component: about -31 dBFS of noise over the whole band

class HfdigSynth : public ModeSynth {
public:
    HfdigSynth(const SynthConfig& c, double rate, std::unique_ptr<HfdigTestAudio> audio, bool lsb)
        : rate_(rate), audio_(std::move(audio)), lsb_(lsb) {
        if (!audio_) return;
        const auto h = hfdig::usbTaps(kTaps, kHfdigAudioRate);
        taps_.resize(kTaps);
        for (int j = 0; j < kTaps; j++) taps_[(size_t)j] = h[(size_t)(kTaps - 1 - j)];   // the history is kept oldest first
        hist_.assign(2 * kTaps, 0.f);
        // 8 kHz -> 48 kHz: a polyphase low-pass at 4 kHz (Kaiser, about 80 dB), so that the copies of the sideband at 8, 16, 24 ... kHz that a linear
        // interpolation straight from 8 kHz left at -20 .. -35 dBc are gone: the transmitted channel is the sideband alone
        const auto lp = hfdig::lowpass(kUp * kPhaseTaps, 0.5 / kUp, 8.0);
        poly_.assign((size_t)(kUp * kPhaseTaps), 0.f);
        for (int ph = 0; ph < kUp; ph++)
            for (int k = 0; k < kPhaseTaps; k++) poly_[(size_t)(ph * kPhaseTaps + k)] = (float)(lp[(size_t)(k * kUp + ph)] * kUp);
        up_.assign(2 * kPhaseTaps, cf32(0.f, 0.f));
        // the noise in 3 kHz against audio with an rms of 0.35, whose upper sideband has twice its power
        const double noise3k = 2.0 * kNoiseSigma * kNoiseSigma * 3000.0 / rate_;
        amp_ = (float)std::min(0.9, std::sqrt(std::pow(10.0, c.snrDb / 10.0) * noise3k / (2 * 0.35 * 0.35)));
        noisy_ = c.snrDb < 150;   // a transmitter (cleanSynth: 200 dB) sends the sideband only, not -31 dBFS of noise over the whole span
        // the dial frequency sits at -tuneOffsetHz in the samples (the radio is tuned above it), plus the carrier offset
        const double w = 2 * hfdig::kPi * (c.cfoHz - hfdigTuning().tuneOffsetHz) / rate_;
        step_ = cf32((float)std::cos(w), (float)std::sin(w));
    }
    double sampleRate() const override { return rate_; }
    void generate(cf32* out, size_t n) override {
        for (size_t i = 0; i < n; i++) out[i] = cf32(0.f, 0.f);
        if (audio_) {
            const double adv = kUpRate / rate_;
            for (size_t i = 0; i < n; i++) {
                while (frac_ >= 1.0) { frac_ -= 1.0; prev_ = next_; next_ = nextUp(); }
                cf32 v = prev_ + (next_ - prev_) * (float)frac_;   // linear from 48 kHz: its images (near multiples of 48 kHz) are 40 dB and more down
                if (lsb_) v = cf32(v.real(), -v.imag());
                out[i] = v * rot_ * amp_;
                rot_ *= step_;
                frac_ += adv;
            }
            rot_ /= std::abs(rot_);
        }
        if (noisy_) noise_.add(out, n, kNoiseSigma);
    }

private:
    static constexpr int kTaps = 255;   // about 270 Hz of transition at 8 kHz
    static constexpr int kUp = 6, kPhaseTaps = 96;   // 8 kHz x 6 = 48 kHz, 576 taps in all
    static constexpr double kUpRate = kHfdigAudioRate * kUp;
    // the next 48 kHz sample of the sideband: phase upPh_ of the polyphase interpolator over the newest kPhaseTaps 8 kHz samples
    cf32 nextUp() {
        if (upPh_ == 0) {
            const cf32 a = nextSideband();
            up_[(size_t)upPos_] = a; up_[(size_t)(upPos_ + kPhaseTaps)] = a;
            if (++upPos_ >= kPhaseTaps) upPos_ = 0;
        }
        const cf32* x = &up_[(size_t)upPos_];   // x[kPhaseTaps - 1] is the newest 8 kHz sample, tap k goes with the one k steps back
        const float* h = &poly_[(size_t)(upPh_ * kPhaseTaps)];
        float re = 0.f, im = 0.f;
        for (int k = 0; k < kPhaseTaps; k++) { const cf32 v = x[kPhaseTaps - 1 - k]; re += h[k] * v.real(); im += h[k] * v.imag(); }
        if (++upPh_ >= kUp) upPh_ = 0;
        return cf32(re, im);
    }
    // the next 8 kHz sample of the upper sideband of the test audio
    cf32 nextSideband() {
        if (bufPos_ >= buf_.size()) { buf_.assign(256, 0.f); audio_->generate(buf_.data(), buf_.size()); bufPos_ = 0; }
        const float a = buf_[bufPos_++];
        hist_[(size_t)pos_] = a; hist_[(size_t)(pos_ + kTaps)] = a;
        if (++pos_ >= kTaps) pos_ = 0;
        const float* w = &hist_[(size_t)pos_];
        cf32 s(0.f, 0.f);
        for (int k = 0; k < kTaps; k++) s += taps_[(size_t)k] * w[k];
        return s * 2.f;   // a real tone is two halves, at plus and minus its frequency; the band-pass keeps one: back to the tone's amplitude
    }
    double rate_;
    std::unique_ptr<HfdigTestAudio> audio_;
    std::vector<cf32> taps_;
    std::vector<float> hist_, buf_, poly_;
    std::vector<cf32> up_;
    int upPos_ = 0, upPh_ = 0;
    bool noisy_ = true;
    size_t bufPos_ = 0;
    int pos_ = 0;
    double frac_ = 1.0;
    cf32 prev_{0.f, 0.f}, next_{0.f, 0.f}, rot_{1.f, 0.f}, step_{1.f, 0.f};
    float amp_ = 0;
    bool lsb_ = false;
    genutil::NoiseSource noise_{1};
};

} // namespace

std::unique_ptr<ModeSynth> makeHfdigSynth(const SynthConfig& cfg, double sampleRate) {
    if (sampleRate <= 0) return nullptr;
    return std::make_unique<HfdigSynth>(cfg, sampleRate, makeHfdigTestAudio(cfg.modeOpt[0], cfg), false);
}

std::unique_ptr<ModeSynth> makeHfdigSynthFromAudio(std::unique_ptr<HfdigTestAudio> audio, const SynthConfig& cfg, double sampleRate, bool lsb) {
    if (sampleRate <= 0) return nullptr;
    return std::make_unique<HfdigSynth>(cfg, sampleRate, std::move(audio), lsb);
}

} // namespace dect2
