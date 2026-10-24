// Shared channeliser of the multi-band monitor (hfdig_multiband.cpp): ONE windowed FFT of the whole capture serves every dial frequency.
// Each block of N samples (Tukey window, hop 3N/4, the overlapping tapers add to one) is transformed once; a channel takes the K bins
// around its centre, tapers the outer bins and inverse-transforms them to K samples at fs K / N; the frames of a channel are overlap-added.
// Cost: one N-point FFT per 3N/4 input samples, whatever the number of channels (a per-band decimator costs the same again for every band).
#pragma once
#include "dect2/common/ring.h"
#include <cstddef>
#include <functional>
#include <vector>

namespace dect2 {

class ChanBank {
public:
    struct Out { int channel; double offsetHz; };   // which channel carries a dial and where the dial sits in it (dial - channel centre)
    // fs: input rate; dialsHz: dial frequencies relative to the capture centre; the audio of each (dial .. dial + audioHz) must be inside the
    // channel. Returns false when the rate is too low for a channel bank (then use a plain decimator).
    bool configure(double fs, const std::vector<double>& dialsHz, double audioHz, std::vector<Out>& map);
    double outRate() const { return fo_; }
    int channels() const { return (int)kc_.size(); }
    // input samples in; fn(channel, samples, n) for every block of output samples (24 per channel per hop at K = 32)
    void feed(const cf32* x, size_t n, const std::function<void(int, const cf32*, size_t)>& fn);
    void reset();
private:
    int N_ = 0, log2N_ = 0, K_ = 32, hopOut_ = 24;
    double fs_ = 0, fo_ = 0, binHz_ = 0;
    std::vector<int> kc_;                         // centre bin of each channel
    std::vector<float> win_, taper_, re_, im_;
    std::vector<cf32> buf_, acc_;                 // input FIFO; per channel the overlap (K - hopOut) samples still to be completed
    std::vector<float> kre_, kim_;
    uint64_t frame_ = 0;
};

} // namespace dect2
