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
    HfdigSynth(const SynthConfig& c, double rate, std::unique_ptr<HfdigTestAudio> audio, bool lsb, float maxAmp = 0)
        : rate_(rate), audio_(std::move(audio)), lsb_(lsb) {
        if (!audio_) return;
        const auto h = hfdig::usbTaps(kTaps, kHfdigAudioRate);
        taps_.resize(kTaps);
        for (int j = 0; j < kTaps; j++) taps_[(size_t)j] = h[(size_t)(kTaps - 1 - j)];   // the history is kept oldest first
        hist_.assign(2 * kTaps, 0.f);
        // the noise in 3 kHz against audio with an rms of 0.35, whose upper sideband has twice its power
        const double noise3k = 2.0 * kNoiseSigma * kNoiseSigma * 3000.0 / rate_;
        amp_ = (float)std::min(0.9, std::sqrt(std::pow(10.0, c.snrDb / 10.0) * noise3k / (2 * 0.35 * 0.35)));
        if (maxAmp > 0) amp_ = std::min(amp_, maxAmp);
        clean_ = c.snrDb >= 99;   // the transmitter's clean signal (TxSession): no noise floor across the whole span
        // the dial frequency sits at -tuneOffsetHz in the samples (the radio is tuned above it), plus the carrier offset
        const double w = 2 * hfdig::kPi * (c.cfoHz - hfdigTuning().tuneOffsetHz) / rate_;
        step_ = cf32((float)std::cos(w), (float)std::sin(w));
        // 8 kHz -> 8 kHz x up_ with a polyphase band-pass (centre 2 kHz, flat over 200 .. 3800 Hz, stop beyond 6.2 kHz from the centre), then
        // linear interpolation to the output rate. Linear interpolation straight from 8 kHz left the images at 8 kHz +- the audio only
        // 25 .. 30 dB down; from 256 kHz they are about 70 dB down.
        up_ = std::max(1, std::min(32, (int)std::floor(rate_ / kHfdigAudioRate)));
        if (up_ > 1) {
            const int n = kPhaseTaps * up_;
            const double fs = kHfdigAudioRate * up_;
            const auto lp = hfdig::lowpass(n, 4000.0 / fs, 8.0);
            const double w0 = 2 * hfdig::kPi * 2000.0 / fs, c0 = (n - 1) * 0.5;
            poly_.assign((size_t)n, cf32(0.f, 0.f));
            for (int p = 0; p < up_; p++)
                for (int k = 0; k < kPhaseTaps; k++) {   // phase p, tap k: prototype index k * up_ + p, applied to the k-th newest 8 kHz sample
                    const int j = k * up_ + p;
                    const double g = lp[(size_t)j] * up_;
                    poly_[(size_t)(p * kPhaseTaps + k)] = cf32((float)(g * std::cos(w0 * (j - c0))), (float)(g * std::sin(w0 * (j - c0))));
                }
            ring_.assign(2 * kPhaseTaps, cf32(0.f, 0.f));
        }
    }
    double sampleRate() const override { return rate_; }
    void generate(cf32* out, size_t n) override {
        for (size_t i = 0; i < n; i++) out[i] = cf32(0.f, 0.f);
        if (audio_) {
            const double adv = kHfdigAudioRate * up_ / rate_;
            for (size_t i = 0; i < n; i++) {
                while (frac_ >= 1.0) { frac_ -= 1.0; prev_ = next_; next_ = nextUp(); }
                cf32 v = prev_ + (next_ - prev_) * (float)frac_;
                if (lsb_) v = cf32(v.real(), -v.imag());
                out[i] = v * rot_ * amp_;
                rot_ *= step_;
                frac_ += adv;
            }
            rot_ /= std::abs(rot_);
        }
        if (!clean_) noise_.add(out, n, kNoiseSigma);
    }

private:
    static constexpr int kTaps = 255;   // about 270 Hz of transition at 8 kHz
    static constexpr int kPhaseTaps = 12;
    // the next sample at 8 kHz x up_
    cf32 nextUp() {
        if (up_ <= 1) return nextSideband();
        if (phase_ == 0) {
            const cf32 s = nextSideband();
            ringPos_ = ringPos_ == 0 ? kPhaseTaps - 1 : ringPos_ - 1;   // newest first
            ring_[(size_t)ringPos_] = s; ring_[(size_t)(ringPos_ + kPhaseTaps)] = s;
        }
        const cf32* h = &poly_[(size_t)(phase_ * kPhaseTaps)];
        const cf32* x = &ring_[(size_t)ringPos_];
        float re = 0, im = 0;
        for (int k = 0; k < kPhaseTaps; k++) {
            re += h[k].real() * x[k].real() - h[k].imag() * x[k].imag();
            im += h[k].real() * x[k].imag() + h[k].imag() * x[k].real();
        }
        if (++phase_ >= up_) phase_ = 0;
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
    std::vector<float> hist_, buf_;
    size_t bufPos_ = 0;
    int pos_ = 0;
    double frac_ = 1.0;
    cf32 prev_{0.f, 0.f}, next_{0.f, 0.f}, rot_{1.f, 0.f}, step_{1.f, 0.f};
    float amp_ = 0;
    bool lsb_ = false, clean_ = false;
    int up_ = 1, phase_ = 0, ringPos_ = 0;
    std::vector<cf32> poly_, ring_;
    genutil::NoiseSource noise_{1};
};

} // namespace

std::unique_ptr<ModeSynth> makeHfdigSynth(const SynthConfig& cfg, double sampleRate) {
    if (sampleRate <= 0) return nullptr;
    return std::make_unique<HfdigSynth>(cfg, sampleRate, makeHfdigTestAudio(cfg.modeOpt[0], cfg), false);
}

std::unique_ptr<ModeSynth> makeHfdigSynthFromAudio(std::unique_ptr<HfdigTestAudio> audio, const SynthConfig& cfg, double sampleRate, bool lsb, float maxAmp) {
    if (sampleRate <= 0) return nullptr;
    return std::make_unique<HfdigSynth>(cfg, sampleRate, std::move(audio), lsb, maxAmp);
}

} // namespace dect2
