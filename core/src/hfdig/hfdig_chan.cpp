#include "hfdig_chan.h"
#include "dect2/dsp/dsp_compat.h"
#include "dect2/common/mathconst.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace dect2 {

bool ChanBank::configure(double fs, const std::vector<double>& dials, double audioHz, std::vector<Out>& map) {
    fs_ = fs;
    kc_.clear(); map.clear();
    // N: the largest power of two that still leaves the channel rate above 25 kHz with K = 32 bins
    int lg = 6;
    while (lg < 20 && fs * K_ / (double)(1 << (lg + 1)) >= 25000.0) lg++;
    N_ = 1 << lg; log2N_ = lg;
    if (fs * K_ / N_ < 20000.0) return false;
    binHz_ = fs / N_;
    fo_ = fs * K_ / N_;
    hopOut_ = K_ * 3 / 4;
    const double usable = (K_ / 2 - 5) * binHz_;   // flat part of a channel: +- 11 bins
    std::vector<int> order(dials.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
    std::sort(order.begin(), order.end(), [&](int a, int b) { return dials[(size_t)a] < dials[(size_t)b]; });
    map.assign(dials.size(), Out{0, 0});
    std::vector<double> centreHz;
    for (int i : order) {
        const double d = dials[(size_t)i];
        int ch = -1;
        for (size_t c = 0; c < centreHz.size(); c++)
            if (d - 300.0 >= centreHz[c] - usable && d + audioHz <= centreHz[c] + usable) ch = (int)c;
        if (ch < 0) {
            const int k = (int)std::lround((d + 0.5 * audioHz) / binHz_);
            centreHz.push_back(k * binHz_);
            kc_.push_back(((k % N_) + N_) % N_);
            ch = (int)centreHz.size() - 1;
        }
        map[(size_t)i] = Out{ch, d - centreHz[(size_t)ch]};
    }
    // Tukey window, tapers N/4 long in total at each end... alpha = 0.5: flat in the middle half, raised cosine over N/4 at both ends;
    // blocks advance by 3N/4, so the overlapping N/4 of two blocks add to one
    win_.resize((size_t)N_);
    const int T = N_ / 4;
    for (int n = 0; n < N_; n++) {
        double w = 1;
        if (n < T) w = 0.5 * (1 - std::cos(math::kPi * (n + 0.5) / T));
        else if (n >= N_ - T) w = 0.5 * (1 - std::cos(math::kPi * (N_ - n - 0.5) / T));
        win_[(size_t)n] = (float)w;
    }
    taper_.assign((size_t)K_, 1.f);
    for (int j = 0; j < 5; j++) {   // the five outer bins at each end of a channel fall off
        const float t = (float)(0.5 * (1 + std::cos(math::kPi * (5 - j - 0.5) / 5)));
        taper_[(size_t)j] = t; taper_[(size_t)(K_ - 1 - j)] = t;
    }
    re_.assign((size_t)N_, 0.f); im_.assign((size_t)N_, 0.f);
    kre_.assign((size_t)K_, 0.f); kim_.assign((size_t)K_, 0.f);
    reset();
    return true;
}

void ChanBank::reset() {
    buf_.clear();
    acc_.assign(kc_.size() * (size_t)(K_ - hopOut_), cf32(0.f, 0.f));
    frame_ = 0;
}

void ChanBank::feed(const cf32* x, size_t n, const std::function<void(int, const cf32*, size_t)>& fn) {
    if (kc_.empty()) return;
    buf_.insert(buf_.end(), x, x + n);
    const size_t hop = (size_t)N_ * 3 / 4;
    size_t used = 0;
    const int ov = K_ - hopOut_;
    std::vector<cf32> out((size_t)K_);
    const float scale = 1.f / (float)N_;
    while (buf_.size() - used >= (size_t)N_) {
        const cf32* b = &buf_[used];
        for (int i = 0; i < N_; i++) { const float w = win_[(size_t)i]; re_[(size_t)i] = b[i].real() * w; im_[(size_t)i] = b[i].imag() * w; }
        fftSplit(re_.data(), im_.data(), log2N_, false);
        for (size_t c = 0; c < kc_.size(); c++) {
            // bins kc - K/2 .. kc + K/2 - 1 arranged so that bin kc is index 0 of the inverse transform: j - K/2 mod K
            for (int j = 0; j < K_; j++) {
                const int src = ((kc_[c] + j - K_ / 2) % N_ + N_) % N_;
                const int dst = (j - K_ / 2 + K_) % K_;
                kre_[(size_t)dst] = re_[(size_t)src] * taper_[(size_t)j];
                kim_[(size_t)dst] = im_[(size_t)src] * taper_[(size_t)j];
            }
            fftSplit(kre_.data(), kim_.data(), 5, true);
            // the phase of the channel centre at the start of this block: kc * (frame * 3N/4) / N = kc * frame * 3 / 4 turns
            const int q = (int)(((uint64_t)kc_[c] * 3ull * frame_) & 3ull);   // quarter turns
            // mixing the block down by e^{-j 2 pi kc n / N}: multiply by e^{-j pi q / 2}
            const float cr = q == 0 ? 1.f : q == 2 ? -1.f : 0.f, ci = q == 1 ? -1.f : q == 3 ? 1.f : 0.f;
            cf32* carry = &acc_[c * (size_t)ov];
            for (int m = 0; m < K_; m++) {
                const float r = kre_[(size_t)m] * scale, im = kim_[(size_t)m] * scale;
                out[(size_t)m] = cf32(r * cr - im * ci, r * ci + im * cr);
            }
            for (int m = 0; m < ov; m++) out[(size_t)m] += carry[m];
            for (int m = 0; m < ov; m++) carry[m] = out[(size_t)(hopOut_ + m)];
            fn((int)c, out.data(), (size_t)hopOut_);
        }
        used += hop;
        frame_++;
    }
    if (used) buf_.erase(buf_.begin(), buf_.begin() + (long)used);
}

} // namespace dect2
