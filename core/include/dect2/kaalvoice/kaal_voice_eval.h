// Objective speech-quality evaluator and test signals for the KAAL voice codecs (no listening test is implied by any number here).
//
// Metrics (reference vs decoded, time-aligned):
//   snr      waveform SNR in dB over the whole signal
//   segSnr   mean of per-20 ms SNRs clamped to [-10, 35] dB, over active frames only
//   lsd      log-spectral distance in dB (32 ms Hann frames, active frames), whole band; lsdLow below 3.7 kHz, lsdHigh above
//   bld      bark loudness disturbance: per frame, critical-band powers -> loudness (power^0.23), absolute difference with a
//            0.25 dead-zone, added energy weighted x2 against missing energy (PESQ-like idea, NOT calibrated to MOS); lower is better
// Test signals: a cascade formant synthesiser (glottal pulses, 5 formants with coarticulation, fricatives, stops, pauses, F0
// declination and jitter) for several voices; vowels and fricatives alone; background noise.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace dect2 {
namespace kaalvoice {
namespace eval {

struct Metrics {
    double snr = 0, segSnr = 0, lsd = 0, lsdLow = 0, lsdHigh = 0, bld = 0;
    int delay = 0;       // alignment used (samples)
    int frames = 0;      // active frames scored
};
// delay < 0: search the alignment in [0, maxSearch] by cross-correlation; otherwise use it.
Metrics compare(const std::vector<int16_t>& ref, const std::vector<int16_t>& dec, int fs, int delay = -1, int maxSearch = 1200);

// voice 0 male, 1 female, 2 high (child), 3 low male. noiseDb: background noise level relative to the speech level (-200 = none).
std::vector<int16_t> synthSpeech(int fs, int voice, double seconds, uint32_t seed, double noiseDb = -200.0);
// A sustained vowel ('a', 'e', 'i', 'o', 'u') or fricative ('s', 'S' (sh), 'f').
std::vector<int16_t> synthSound(int fs, char sound, double f0, double seconds, uint32_t seed = 1);

bool readWav(const std::string& path, std::vector<int16_t>& pcm, int& fs);   // 16-bit PCM, mono or first channel
bool writeWav(const std::string& path, const std::vector<int16_t>& pcm, int fs);
std::vector<int16_t> upsample2(const std::vector<int16_t>& x);     // 8 -> 16 kHz (windowed sinc)
std::vector<int16_t> downsample2(const std::vector<int16_t>& x);   // 16 -> 8 kHz

}  // namespace eval
}  // namespace kaalvoice
}  // namespace dect2
