#pragma once
#include "dect2/common/ring.h"
#include <cstdint>
#include <vector>

namespace dect2 {

struct SignalStats {
    float peak = 0;        // linear, full scale = 1
    float rmsDbfs = -120;
    float clipFraction = 0; // fraction of samples within 1 LSB of full scale
    float dcI = 0, dcQ = 0;
    uint32_t hist[64] = {}; // |I| and |Q| magnitude histogram (ADC occupancy)
};

// 10*log10(p) for 1e-14 <= p, the fast form the analyser uses (error below 0.0001 dB)
float spectrumDb(float p);

struct SpectrumFrame {
    std::vector<float> dbfs; // fftSize bins, DC-centred (-fs/2 .. +fs/2)
    SignalStats stats;
    uint64_t seq = 0;
};

class SpectrumAnalyzer {
public:
    explicit SpectrumAnalyzer(size_t fftSize = 4096);
    ~SpectrumAnalyzer();
    size_t fftSize() const { return n_; }
    static constexpr size_t kMaxFft = 262144;   // the finest manual choice (#243): 2^18 bins, about 9 Hz at 2.4 Msps
    static size_t autoSize(double sampleRate);   // the size that gives bins of about 125 Hz or finer, 8192..65536

    // consume samples, averages up to 32 blocks per frame
    void feed(const cf32* x, size_t n);
    bool takeFrame(SpectrumFrame& out); // false if nothing accumulated
    void setTransform(bool on);         // off = stats only
    // Blocks averaged into one frame (1..64). Few blocks give each waterfall row the true grain of the noise (an FFT bin of noise is a
    // chi-square variable; 16 averaged blocks flatten it to +-1.4 dB, a featureless floor) and cost far less CPU at high rates; the app
    // smooths the trace itself (a time constant). Was 16 before 3.0.
    static constexpr size_t kDefaultBlocks = 2;
    void setBlocksPerFrame(size_t n);
    size_t blocksPerFrame() const;
    void resize(size_t fftSize);        // power of two, 1024..kMaxFft: a new transform size (drops the accumulated data)
    void reset();                       // forget accumulated data (after retuning)

private:
    struct Impl;
    Impl* p_;
    size_t n_;
};

} // namespace dect2
