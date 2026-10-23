# KAAL voice codec 4: decoder-side speech enhancer

Status: branch kaal-enhance-030 (2026-10-23), step 1 of the owner's plan: a **classical** enhancer (no neural network, no training data,
no downloads). Receiver only: **the KV4 bit stream does not change** and costs no extra bits; an old decoder and a new one read the
same frames. Tested in simulation only: **no listening test has been possible**; every number below is an objective proxy.

Code: `core/src/kaalvoice/kv_enhance.{h,cpp}` (the stages), `kv_codec.cpp` (where the decoder calls them), the switches in
`core/include/dect2/kaalvoice/kaal_voice.h` (`Config::enhance`, `Config::enhanceStages`, `ExternalPostFilter`), the evaluation in
`tools/kaalvoice_bench.cpp --enhance [--ablate] [--persignal]`, the tests in `tests/kaalvoice/test_kaalvoice_enhance.cpp`.

## 1. Settings

```cpp
kaalvoice::Config c;           // c.enhance = -1 (auto): on for 3.2 / 4.8 / 6.0 kbit/s, off for 8.0 / HQ12 / HQ16
c.enhance = 1;                 // force on (any profile), 0 = the plain KV4 decoder (bit-exact with the codec before this change)
c.enhanceStages = kaalvoice::kEnhDefault & ~kaalvoice::kEnhBwe;   // A/B: any stage can be switched off on its own
```

| bit | stage | default |
|---|---|---|
| `kEnhPitch` | adaptive long-term (pitch) post-filter | on |
| `kEnhFormant` | adaptive formant post-filter + spectral-tilt compensation | on |
| `kEnhBwe` | high band: harmonic extension from the low band for voiced speech, noise for unvoiced | on |
| `kEnhHbSmooth` | high band: temporal smoothing of the envelope, low-fluctuation noise | on |
| `kEnhLoudness` | energy (loudness) restoration of the sparse pulse excitation | on |
| `kEnhComfort` | comfort-noise shaping | on |
| `kEnhDeclick` | de-clicking at the first good frame after a loss | on |
| `kEnhNoiseReduce` | gentle Wiener noise reduction with the decoder's own noise estimate | **off** (adds 5 ms delay) |

When the enhancer runs, its post-filter replaces the core postfilter of KAAL-CODEC.md section 3.2 (never both). `Config::postfilter =
false` also switches the enhancer's two post-filter stages off. The noise-reduction stage adds one 5 ms hop of delay, and
`info().delaySamples` includes it (encoder and decoder share the `Config`, so both report the same delay).

## 2. Where it sits

```
bits -> unpack -> core decoder (8 kHz CELP synthesis, core postfilter OFF) -> [loudness restoration] -> [pitch + formant post-filter] -+
                  side info: LPC, lag, gp, gc, pulse energy, excitation                                                              |
                  high-band decoder: band levels per 5 ms block -> [envelope smoothing] -> [harmonic (voiced) / noise synthesis] ---+
                  -> interpolate 8 -> 16 kHz, add the high band -> [de-click] -> [noise reduction] -> [external post-filter] -> int16
comfort noise (SID): [LPC bandwidth expansion 0.92, level smoothing] in the low band; high band through the same smoothing/synthesis
```

All stages work frame by frame with no look-ahead, so the enhancer adds **no delay** (except the optional noise reduction, 5 ms). The
adaptive codebook and every other piece of decoder state that the encoder mirrors are untouched: the enhancer has its own filter
memories and noise generators, so the decoder stays in step with the encoder whatever the enhancer does.

## 3. Algorithm and parameters

Per-profile strengths are in `kEnhTiers` (kv_enhance.cpp):

| profile | gn | gd | pitch strength | tilt | noise fill | harmonic share | restoration |
|---|---|---|---|---|---|---|---|
| 3.2 | 0.55 | 0.72 | 0.50 | 0.45 | 0.30 | 0.30 | +2.0 dB |
| 4.8 | 0.55 | 0.71 | 0.45 | 0.45 | 0 | 0.30 | +1.0 dB |
| 6.0 | 0.55 | 0.70 | 0.40 | 0.45 | 0 | 0.30 | +0.8 dB |
| 8.0 | 0.60 | 0.75 | 0.40 | 0.80 | 0 | 0.30 | +0.5 dB |
| HQ12 | 0.65 | 0.75 | 0.25 | 0.80 | 0 | 0.30 | +0.4 dB |
| HQ16 | 0.70 | 0.75 | 0.15 | 0.80 | 0 | 0.30 | +0.3 dB |

- **Loudness restoration** (`kEnhLoudness`). The joint, waveform-matching gain search of a sparse pulse codebook scales the coded
  signal down (it minimises the error, not the level): the plain decoder is 0.2 to 1.7 dB too quiet on average (signed level error in
  the table of section 4). The stage applies the measured average loss of the profile as a fixed gain (column "restoration"), and at
  3.2 kbit/s also adds noise of 0.3 x (1 - gp)^2 times the pulse power, shaped by the subframe's LPC, for the noise-like part that one
  pulse per 10 ms cannot carry. The noise fill helps the loudness scores but lowers the STOI-like score a little, so the higher profiles
  do not use it.
- **Pitch post-filter** (`kEnhPitch`). On the residual of A(z/gn): y = (r(n) + g r(n - T)) / (1 + g), T searched over the decoded
  integer lag +-1, g = strength x min(1, c/e) x ramp(normalised correlation 0.5 .. 0.8); only for subframes with a decoded pitch gain
  above 0.3; halved on concealed frames and off after the second lost frame.
- **Formant post-filter** (`kEnhFormant`). 1 / A(z/gd) after the residual; gd moves towards gn (no emphasis) on unvoiced subframes
  (weight 0.55 .. 1 with the pitch gain) and to 60 % on concealed frames, so noise and fricatives are not coloured. Tilt compensation
  1 - mu z^-1 with mu = tilt x r1/r0 of the filter's impulse response, then gain control to the energy before the filter. The lower
  tilt factor (0.45) at the low profiles raised the STOI-like score.
- **High band, envelope smoothing** (`kEnhHbSmooth`). The band levels per 5 ms block (dB) follow a one-pole smoother, 0.9 rising and 0.7
  falling per block, so the 6 dB steps of the 4-bit level no longer jump at frame edges ("chirping"). The noise is a random sign at the
  band's level in every MDCT bin (constant magnitude instead of uniform values): no random spectral holes changing every 5 ms
  ("musical noise").
- **High band, harmonic extension** (`kEnhBwe`). The decoded excitation is interpolated to 16 kHz with the low band's filter (so it is
  time-aligned with the output) and full-wave rectified: the rectifier makes harmonics of F0 that reach into 3.6 .. 8 kHz and keep the
  timing of the glottal pulses. Its MDCT over the high band's 5 ms blocks (the 80 samples past the frame by pitch repetition) is
  normalised per band and mixed with the noise in power: share = 0.3 x ramp(gp 0.35 .. 0.85) x (1 - 0.1 x band index). The transmitted
  levels are kept exactly (nothing is invented above what the encoder sent). Unvoiced speech stays noise. A share of 0.5 .. 0.9 raised
  the output peaks (pulse-synchronous high band) without changing any proxy, so it stays at 0.3.
- **Comfort noise** (`kEnhComfort`). The SID's coarse LPC is bandwidth-expanded (A(z/0.92)) and the frame level smoothed (0.7 / 0.3,
  gain limited to 0.7 .. 1.4), so the background does not pump or whistle.
- **De-click** (`kEnhDeclick`). On the first good frame after a loss: a step larger than 4 times the mean sample-to-sample change of the
  last millisecond (+64) is removed by an offset decaying in about 1 ms; the first 10 ms are limited to 12 dB above the level of the last
  10 ms (2.5 ms gain ramps, at most -12 dB), only when the loss was at most 120 ms (after a longer one the concealment has faded out and
  a loud restart is real speech).
- **Noise reduction** (`kEnhNoiseReduce`, off). 10 ms sqrt-Hann frames, 50 % overlap, decision-directed Wiener gain with a -8 dB floor;
  the noise spectrum is learnt from the comfort noise when SIDs arrive, otherwise tracked as the minimum of the decoded speech.

## 4. Measurements (objective proxies only)

No listening test was possible. Every column is a proxy: segSNRlo (waveform SNR below 3.8 kHz; any post-filter or gain change lowers
it by design, so it is not a target here), LSD (log-spectral distance, dB), LSDhi (the same above 3.7 kHz), BLD (bark loudness
disturbance, PESQ-like idea, not calibrated), STOI~ (a simplified STOI: envelope correlation of 15 third-octave bands from 150 Hz to
4 kHz over 384 ms; not the reference implementation), lvl / abs lvl (signed / absolute frame level error in dB: the loudness
distortion). The intelligibility proxies of the checks are STOI~ and BLD.

`kaalvoice_bench --wav voice_listen/00_original.wav --enhance --ablate`, means over 10 signals: the real sample (8 kHz, interpolated,
so it has no high band), four synthetic voices, the male voice at 15 dB SNR, vowels a (120 Hz) and i (220 Hz), fricatives s and sh. The
sustained sounds have little envelope modulation, so their STOI~ is low for every codec (it pulls the means down equally).

| codec | enhancer | segSNRlo | LSD | LSDhi | BLD | STOI~ | lvl | abs lvl | decoder CPU |
|---|---|---|---|---|---|---|---|---|---|
| KV4 3.2 / 40 ms | off | 5.94 | 8.27 | 5.88 | 0.203 | 0.545 | -1.70 | 1.92 | 0.0016 |
| KV4 3.2 / 40 ms | on | 5.09 | 7.87 | 5.86 | **0.187** | 0.544 | -0.28 | **1.64** | 0.0032 |
| KV4 4.8 / 40 ms | off | 8.14 | 7.59 | 5.88 | 0.153 | 0.593 | -0.85 | 1.13 | 0.0016 |
| KV4 4.8 / 40 ms | on | 7.61 | 7.44 | 5.86 | **0.150** | 0.592 | -0.15 | **1.07** | 0.0031 |
| KV4 6.0 / 40 ms | off | 9.42 | 7.13 | 5.36 | 0.124 | 0.653 | -0.75 | 0.94 | 0.0016 |
| KV4 6.0 / 40 ms | on | 8.98 | 6.95 | 5.29 | **0.120** | 0.654 | -0.18 | 0.95 | 0.0032 |
| KV4 4.8 / 20 ms | off | 7.65 | 7.69 | 5.88 | 0.157 | 0.604 | -0.92 | 1.20 | 0.0016 |
| KV4 4.8 / 20 ms | on | 7.23 | 7.52 | 5.86 | **0.153** | 0.606 | -0.21 | **1.06** | 0.0031 |
| KV4 6.0 / 20 ms | off | 9.10 | 7.17 | 5.36 | 0.127 | 0.652 | -0.82 | 1.00 | 0.0017 |
| KV4 6.0 / 20 ms | on | 8.75 | 6.99 | 5.30 | **0.124** | 0.653 | -0.25 | 0.95 | 0.0031 |
| KV4 8.0 / 40 ms, forced on | off / on | 10.48 / 10.30 | 6.68 / 6.52 | 5.36 / 5.28 | 0.106 / 0.103 | 0.682 / 0.682 | -0.54 / -0.18 | 0.71 / 0.70 | |
| KV4 HQ12 / 20 ms, forced on | off / on | 13.01 / 12.77 | 6.17 / 6.05 | 5.36 / 5.28 | 0.089 / 0.088 | 0.731 / 0.733 | -0.28 / +0.01 | 0.46 / 0.51 | |
| KV4 HQ16 / 20 ms, forced on | off / on | 15.11 / 14.91 | 5.78 / 5.66 | 5.35 / 5.27 | 0.083 / 0.082 | 0.779 / 0.782 | -0.19 / +0.03 | 0.38 / 0.41 | |
| old KaalCodec3 16 kbit/s | - | 13.55 | 6.21 | 5.42 | 0.125 | 0.741 | -0.17 | 0.50 | 0.034 (enc + dec) |

3 % of the payloads lost (same signals), BLD off -> on: 3.2 0.334 -> 0.316, 4.8 0.298 -> 0.296, 6.0 0.291 -> 0.288; abs lvl 4.22 -> 3.61,
3.34 -> 3.19, 3.49 -> 3.37 dB.

Ablation (one stage removed; 3.2 / 4.8 / 6.0 at 40 ms; BLD and STOI~): without pitch 0.201 / 0.545, 0.167 / 0.595, 0.130 / 0.658;
without formant 0.179 / 0.536, 0.143 / 0.588, 0.113 / 0.650; without harmonic extension 0.187 / 0.545, 0.150 / 0.594, 0.120 / 0.655;
without smoothing 0.188 / 0.544, 0.153 / 0.591, 0.122 / 0.654; without loudness 0.206 / 0.546, 0.155 / 0.592, 0.124 / 0.654. The
pitch post-filter buys the loudness disturbance and the formant post-filter the STOI-like score (each costs a little of the other); the
restoration removes most of the level error; the smoothing helps BLD and LSDhi; the harmonic extension, comfort and de-click stages do
not move these averages (they act on buzz versus hiss and on transients, which these proxies do not see).

**Honest reading**
- The enhancer lowers BLD, LSD, LSDhi and the level error at every profile. STOI~ moves by -0.001 .. +0.003: at 3.2 and 4.8 / 40 ms it
  is 0.001 lower on this set (inside its noise); on the held-out signals of the test it is higher at every profile (3.2 0.599 -> 0.602,
  4.8 0.682 -> 0.684, 6.0 0.725 -> 0.728, HQ16 0.834 -> 0.837).
- By these proxies it does **not** make 3.2 .. 6.0 kbit/s sound like 32 kbit/s: 6.0 kbit/s with the enhancer has a lower BLD than the old
  codec at 16 kbit/s (0.120 vs 0.125) but a much lower STOI~ (0.654 vs 0.741) and a higher LSD (6.95 vs 6.21). A classical post-filter
  cannot put back what the bits did not carry; that is step 2 (a neural post-filter, section 7).
- segSNRlo falls by 0.2 .. 0.9 dB, as any gain or post-filter makes it fall.
- Tuned on the bench set (the real sample, synthetic seeds 11..14, 21, 77); the test checks other seeds (41..44, 51). The synthetic
  speech is a formant synthesiser; one real sample, narrow-band.

Stability (test_kaalvoice_enhance): 10 % losses with a 400 ms burst, damaged classes B/C, garbage frames, DTX with lost SIDs and the
noise reduction on: output bounded, no burst after the long loss; the first speech frame after comfort noise has no larger step than
with the plain decoder; comfort-noise level within 3 % of the plain decoder; de-clicking lowers the summed boundary steps after a loss
followed by an undetected bad frame (54024 -> 50617) and the level of the first 10 ms (15512 -> 13596).

## 5. CPU

Decoder CPU time per second of speech, one stream, on this Mac (Apple silicon, other jobs running): 0.0016 without, **0.0031 with** the
enhancer (the harmonic extension is about 40 % of it); the noise-reduction stage adds about 0.0025. Far under the 0.15 budget (the test
checks below 0.15). Raspberry Pi 4 (Cortex-A72 1.5 GHz) not measured: with the 5 .. 8 x factor of KAAL-CODEC.md section 5, one stream
would need 0.016 .. 0.025 of a core for the decoder with the enhancer, four slots 0.06 .. 0.10 (the encoders come on top).

## 6. Determinism

Only + - * / and sqrt and the codec's own sine, cosine, log and exp, built with -ffp-contract=off like the rest of the codec.
test_kaalvoice_enhance hashes the enhanced output of five configurations (3.2, 4.8 / 20 ms, 6.0 with DTX and noise reduction, 8 kHz,
HQ16 forced on; with losses and class-C damage): digest 5c6b5c649e3047a1, the same on arm64 and on x86-64 (built with -arch x86_64
-mavx2 -mfma, run under Rosetta). test_kaalvoice_golden now runs with the enhancer off and still gives the codec's recorded digest, so
the plain decoder is bit-exact with the one before this change.

## 7. Interface for a future neural post-filter

```cpp
struct MyPostFilter : kaalvoice::ExternalPostFilter {
    void reset() override;                                                  // decoder reset
    void process(const kaalvoice::EnhanceSide& side, float* pcm) override;  // frameSamples floats, int16 scale, in place
};
dec.setPostFilter(std::make_shared<MyPostFilter>());
```

It runs after the classical stages and before the int16 conversion, whether the classical enhancer is on or off (a neural filter can
replace it with `c.enhance = 0`, or keep it as a pre-stage). `EnhanceSide` carries what the decoder knows at no extra bits: frame kind
(speech / concealed / comfort noise), first frame after a gap, lost run, the quantised LPC, lag, pitch and code gains per subframe,
voicing, the high-band levels per 5 ms block and the background noise estimate. It must not add delay (the decoder's delay figure does
not include it). `Decoder::lastSide()` returns the same record after each frame. The test runs a pass-through filter (output unchanged)
and a x0.5 filter (output halved) through the slot.

## 8. Files and tests

- `kv_enhance.h/.cpp`: the enhancer; `kv_codec.cpp`: the calls; `kv_core.*`, `kv_hb.cpp`: side information (`SubInfo`, excitation,
  `HbDecoder::levels/synth/comfortLevels`) without changing any decoded value.
- `kaal_voice_eval.h`: new metrics `stoi`, `levelBias`, `levelErr`.
- `test_kaalvoice_enhance`: activation rules, unchanged bit stream, the proxies on vs off at seven profiles (STOI~ and BLD must not get
  worse), every stage switch, stability, onset after silence, de-click, the external slot, CPU, digest.
