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

## 2. First tuning (2026-10-23, from the test's proxies)

The first build raised the bark loudness disturbance (BLD) at every profile, lowered the STOI-like score at 4.8 kbit/s and made output
peaks of 1.7 times the input at 3.2 / 4.8 kbit/s (test_kaalvoice_roundtrip). Stage by stage on the test signals:
- high-band envelope smoothing: the release was too slow (0.5 per 5 ms block, smearing every offset); at 0.8 it lowers BLD everywhere;
- harmonic extension: the rectified excitation's pitch pulses lined up into the peaks; the harmonic share is 0.5 now (was 0.7..0.9);
- loudness restoration: half the former noise fill (it lowered STOI at 4.8 kbit/s / 20 ms at full strength).
Now every profile is at or above the plain decoder on STOI and below it on BLD. The side information also carries the frame layout
(subframes, their length) for comfort-noise frames.
