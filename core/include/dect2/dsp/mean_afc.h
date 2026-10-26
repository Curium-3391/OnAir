// Frequency capture for the 4-level FSK receivers (P25, NXDN): the mean of the FM discriminator output over 20 ms, taken only while a carrier with
// a modulation-sized spread is there (the spread of noise is several times larger), is the carrier's distance from the oscillator. The balanced
// symbols of a scrambled or coded frame average to nothing, so the mean is the tuning error; the receiver moves its oscillator by it. Only errors beyond what the receivers' own per-frame offset estimate
// follows (4.5 kHz) are corrected: a smaller move would only disturb the first call.
//
// Before this the receivers relied on the channel filter alone: a carrier more than about 4.5 kHz off was cut by the filter and nothing decoded,
// although a radio without a good reference is 5 kHz off on the 800 MHz band (6 ppm) and a handset can be 2.5 ppm off on its own.
#pragma once
#include <cmath>

namespace dect2 {

class MeanAfc {
public:
    // f: the discriminator output (Hz) at `rate` samples per second. Returns the correction in Hz when one is due, else 0.
    double push(float f, double rate) {
        if (hold_ > 0) { hold_--; return 0; }
        s_ += f; q_ += (double)f * f;
        if (++n_ < (int)(0.02 * rate)) return 0;
        const double m = s_ / n_, var = q_ / n_ - m * m;
        n_ = 0; s_ = q_ = 0;
        if (var > kMaxSpreadHz * kMaxSpreadHz || std::fabs(m) < kMinHz) return 0;
        hold_ = (int)(0.008 * rate);        // the channel filter's delay: the next window sees the moved signal
        return m;
    }
    void reset() { n_ = 0; s_ = q_ = 0; hold_ = 0; }
    static constexpr double kMaxSpreadHz = 3500.0, kMinHz = 4700.0, kMaxTotalHz = 7500.0;
private:
    int n_ = 0, hold_ = 0;
    double s_ = 0, q_ = 0;
};

} // namespace dect2
