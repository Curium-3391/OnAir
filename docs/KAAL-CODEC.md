# KAAL voice codec 4 (KV4)

Status: bit-stream version 1 of KV4, branch kaal-codec-030 (2026-10-23). Own design, written from scratch for the owner's KAAL mode.
Tested in simulation only: **no listening test has been done**; every quality number below is an objective score (SNR, spectral
distances) on synthetic speech and on the one real speech sample in the repo (`voice_listen/00_original.wav`, 8 kHz).

Code: `core/include/dect2/kaalvoice/kaal_voice.h` (interface), `core/src/kaalvoice/` (codec), `kaal_voice_eval.h` (evaluator and
test signals), `tools/kaalvoice_bench.cpp` (before/after comparison), `tests/kaalvoice/` (tests).

## 1. What it is

- 16 kHz (wideband) or 8 kHz speech, 20 ms or 40 ms frames, fixed-size speech frames per profile.
- Profiles (bits per 20 ms): **3.2** (64, 40 ms frames only), **4.8** (96), **6.0** (120), **8.0** (160), **HQ12** (240), **HQ16** (320).
  Any other budget from 64 to 400 bits per 20 ms also works (`Config::payloadBits`, custom slot sizes).
- The KAAL v4 PHY (docs/KAAL-SLOTS.md on kaal-phy-030) carries one payload per slot every 40 ms: V80 = 8.0, V60 = 6.0, V48 = 4.8,
  V32R = 3.2 kbit/s. Use `frameMs = 40`, `inBandType = true`, `sampleRate = 16000`; a payload is then exactly 320 / 240 / 192 / 128
  bits, its first bit says speech (0) or SID (1), and a slot whose encoder returned NoData can carry data instead.
- DTX: silence sends a 40-bit SID on its first frame and then every 480 ms (sooner if the background changes), otherwise nothing.
  The decoder makes comfort noise of the same level and colour.
- Concealment of lost frames, bit ordering by importance (classes A, B, C), optional input noise suppression.
- The old KAAL codecs stay selectable: `makeFrameCodec(CodecId::LegacyV3 / LegacyV1, cfg)` gives the same interface over KaalCodec3
  (12/16/24/32 kbit/s) and KaalCodec v0.1, bit-exact with their own `encode()` and stream decoders, so old KAAL signals still decode.

## 2. Interface

```cpp
kaalvoice::Config c;                 // sampleRate 16000, frameMs 20, profile K6_0, dtx on, postfilter on
c.frameMs = 40; c.profile = kaalvoice::Profile::K6_0; c.inBandType = true;   // KAAL v4 V60 slot
kaalvoice::Encoder enc(c);  kaalvoice::Decoder dec(c);
std::vector<uint8_t> f;
kaalvoice::FrameType t = enc.encode(pcm640, f);   // Speech / Sid: f has payloadBytes; NoData: f empty (send data)
dec.decode(t, f.data(), f.size(), out640);        // received payload
dec.erasure(out640);                              // payload lost (LDPC failed)
dec.decode(t, data, n, out, kaalvoice::kBadC);    // container says class C was damaged (A, B intact)
```

`info(c)` gives frame samples, payload bits/bytes, the class sizes (class A = the first `classBits[0]` bits, then B, then C, then
zero padding), and the delay. `typeOfLength()` maps a received length to a frame type when the type is not in band (0 bytes = no data,
5 bytes = SID, payloadBytes = speech).

Delay (decoder output against encoder input): 173 samples at 16 kHz (10.8 ms: 5.8 ms band split + 5 ms look-ahead), 40 samples at
8 kHz, plus 80 samples (5 ms) with the noise suppressor. Add the frame itself (20 / 40 ms) for the algorithmic delay.

## 3. Algorithm

### 3.1 Signal path

```
16 kHz in -> [noise suppressor] -> DC blocker (20 Hz) -> 95-tap low-pass, decimate -> 8 kHz core (CELP)        -> interpolate -+-> out
                                                   \-> high band = input delayed - interpolated low band -> envelope coder --/
```

The split filter is a Kaiser-windowed low-pass (cut-off 3.75 kHz, beta 5). The encoder takes the high band as the exact remainder
`x(n - 93) - up(down(x))(n)`, so the two paths add up to the delayed input; the high band therefore covers whatever the low band
does not (from about 3.6 kHz). At 8 kHz the core codes the input directly.

### 3.2 Core (8 kHz analysis-by-synthesis)

- LPC order 10 on a 280-sample asymmetric window (Hamming rise over 240 samples, quarter-cosine fall over the 5 ms look-ahead),
  60 Hz Gaussian lag window, -40 dB white-noise correction. 40 ms frames also analyse the middle of the frame.
- LSFs from the Chebyshev series of the sum/difference polynomials (grid search + bisection, our own cosine).
- **LSF quantiser**: prediction from the previous frame (rho 0.6 for 20 ms, 0.45 for 40 ms frames) around a mean; the residual is
  rotated onto the eigenvectors of its covariance (KLT), each coefficient is divided by its standard deviation and quantised with a
  Gaussian Lloyd-Max quantiser; bits are spread by greedy reverse water-filling. The encoder then refines the indices by coordinate
  descent on an LSF error weighted by the closeness of neighbouring LSFs (formant peaks). 40 ms frames add 2 bits choosing the
  mid-frame interpolation weight (0.25 / 0.45 / 0.62 / 0.80). Mean, eigenvectors and deviations were measured on synthetic training
  speech (12 seeds x 4 voices x 6 s); held-out check on the real sample: spectral distortion 1.38 dB at 22 bits, 1.13 dB at 26,
  0.76 dB at 32 (scalar quantisation without KLT: about 2.2 dB at 22 bits).
- Perceptual weighting W(z) = A(z/0.94) / A(z/0.6); target = weighted speech minus the zero-input response of W/Aq.
- **Pitch**: open-loop per 10 ms on weighted speech (normalised correlation, three lag sections favouring the shortest lag that is
  85 % as good: no pitch doubling), closed-loop search around it; lags 20..147 samples, integer (3.2 / 4.8) or half-sample
  resolution (16-tap windowed-sinc interpolation), absolute in the first subframe of each 20 ms, delta-coded in the others.
- **Fixed codebook**: pyramid vector quantiser (Fischer): K unit pulses (may stack) over the whole subframe, enumerated exactly
  (index bits = ceil log2 V(L, K)); greedy analysis-by-synthesis search on the backward-filtered target with two refinement passes;
  pitch sharpening (comb at the lag with the previous pitch gain, max 0.8) when the lag is shorter than the subframe. K follows from
  the bits left after the side information.
- **Gains**: pitch gain (3 or 4 bits, up to 1.2) and fixed-codebook gain as a correction (4 or 5 bits) to an energy predicted by a
  4-tap moving average of past corrections; both searched jointly for the smallest weighted error. Two pitch gains above 1 in a row are
  never sent (decoder stability).
- **Decoder postfilter**: formant postfilter A(z/gn)/A(z/gd) with tilt compensation, pitch postfilter on the residual (strength by
  profile, only for clearly periodic subframes), automatic gain control.

### 3.3 High band (16 kHz only)

5 ms MDCT blocks (sine window, 80 bins of 100 Hz), six bands from 3.6 to 8 kHz. Per 20 ms: a level (4 or 5 bits, 6 or 3 dB steps),
a spectral shape (tilt 3 bits; or tilt + curvature 5 bits; HQ16: 3 bits per band) and a time envelope (2 bits per 5 ms block, relative
to the loudest block). The decoder fills the bins with noise of that envelope. Without the time envelope, onsets of fricatives were
smeared: adding it lowered the loudness disturbance from 0.245 to 0.181 at 6 kbit/s (synthetic male voice).

### 3.4 VAD, DTX, SID, comfort noise

VAD: frame energy against an adaptive noise floor (fast down, about 2.5 dB/s up), helped by a pitch correlation, hangover 60 ms after a
short burst and 160 ms after speech. During silence the encoder averages the background (LSF, LP residual level, high-band level and
tilt, ~130 ms) and sends a SID (40 bits: LSF 28 non-predictive, residual level 6, high-band level 5, tilt 1). The decoder runs white
noise through the SID filter, its parameters gliding towards each new SID; at the start of speech both sides reset the excitation,
the gain predictor and the LSF predictor (to the last SID), and the comfort noise fades out under the first speech frame.

### 3.5 Concealment and errors

Lost frame: LSFs drift 15 % towards the mean, the last lag is kept, the pitch gain decays (0.98, 0.9, 0.75, 0.5, 0.25 by lost time),
random pulses with a falling gain; after 120 ms the output fades to silence. The first good frame after a loss limits the pitch gain to
1. Damaged class B: those bits are set to the middle of their interval (lag delta 0). Damaged class C: random pulses with the decoded
gains, high-band time envelope flat. Decoder guards clamp the excitation and the filter memories, and a soft limiter above 28000
avoids hard clipping. After any amount of garbage the decoder follows a valid stream again (test: 27.5 dB against a clean decode).

### 3.6 Noise suppressor (optional, encoder input)

50 % overlapping sqrt-Hann frames of 10 ms (zero-padded FFT), minimum-tracking noise estimate, decision-directed Wiener gain with a
-16 dB floor. On speech at 12 dB SNR it lowered the pauses from 54.4 to 39.4 dB and brought the decoded speech closer to the clean
speech (loudness disturbance 0.657 -> 0.291).

## 4. Bit layout

Order inside a frame: LSF (KLT coefficients 0..9), [interpolation], then per 20 ms half: per subframe lag, pitch gain, gain
correction, pulses; then the high band of that half. The packed payload is ordered by class: all class A bits (in field order), then B,
then C, then padding; each field's top bits go to A, the next to B, the rest to C:

| field | A | B | C |
|---|---|---|---|
| LSF coefficient k | 2 bits (k < 3), 1 bit (k < 6) | the rest | - |
| interpolation | 2 | - | - |
| first lag of a 20 ms half | 5 | rest | - |
| lag delta | 1 | rest | - |
| pitch gain | 2 | rest | - |
| gain correction | 3 | rest | - |
| high-band level | 3 | rest | - |
| high-band shape | - | all | - |
| pulses, high-band time envelope | - | - | all |

Profiles (16 kHz; at 8 kHz the high-band bits go to the pulses):

| profile | ms | bits | subframes | LSF bits (per KLT coef.) | interp | lag abs/delta | gp/gc | high band | pulses (K per subframe) | pad | A/B/C |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 3.2 | 40 | 128 | 4 x 80 | 20 (4332222110) | 2 | 7/3 | 3/3 | 30 | 32 (K 1,1,1,1) | 0 | 49/31/48 |
| 4.8 | 40 | 192 | 4 x 80 | 22 (5432222110) | 2 | 7/4 | 3/4 | 30 | 85 (K 3,3,3,4) | 3 | 49/39/101 |
| 6.0 | 40 | 240 | 4 x 80 | 24 (5433222210) | 2 | 8/4 | 3/4 | 36 | 125 (K 5,5,5,6) | 1 | 49/49/141 |
| 8.0 | 40 | 320 | 8 x 40 | 26 (5433222221) | 2 | 8/4 | 3/4 | 36 | 160 (K 3,3,4,4,4,4,4,4) | 0 | 73/71/176 |
| HQ12 | 40 | 480 | 8 x 40 | 28 (5443322221) | 2 | 8/5 | 4/5 | 36 | 294 (K 8 .. 9) | 2 | 73/95/310 |
| HQ16 | 40 | 640 | 8 x 40 | 32 (5544333221) | 2 | 8/5 | 4/5 | 62 | 426 (K 14 .. 15) | 0 | 73/125/442 |
| 4.8 | 20 | 96 | 2 x 80 | 22 | - | 7/4 | 3/4 | 15 | 34 (K 2,3) | 0 | 28/26/42 |
| 6.0 | 20 | 120 | 2 x 80 | 24 | - | 8/4 | 3/4 | 18 | 50 (K 4,4) | 2 | 28/32/58 |
| 8.0 | 20 | 160 | 4 x 40 | 26 | - | 8/4 | 3/4 | 18 | 68 (K 3,3,3,3) | 0 | 40/44/76 |
| HQ12 | 20 | 240 | 4 x 40 | 28 | - | 8/5 | 4/5 | 18 | 135 (K 7,7,7,9) | 0 | 40/57/143 |
| HQ16 | 20 | 320 | 4 x 40 | 32 | - | 8/5 | 4/5 | 31 | 198 (K 12,13,13,13) | 0 | 40/74/206 |

With `inBandType` the type bit comes first (it counts as class A) and speech has one bit less. Measured sensitivity (6.0 kbit/s,
one flipped bit per 40 ms frame): loudness disturbance 0.730 in class A, 0.157 in B, 0.131 in C (clean 0.120), so A is the part to
protect; on the KAAL v4 PHY the whole payload is LDPC-protected and arrives bit-exact or not at all.

## 5. Quality (objective, simulation)

`kaalvoice_bench --wav voice_listen/00_original.wav`, dtx off, postfilter on. Means over 6 signals: the real sample (8 kHz,
interpolated to 16 kHz), four synthetic voices (male, female, child, low male; formant synthesiser with fricatives and stops) and the
synthetic male voice with noise at 15 dB SNR. SNRlo / segSNRlo: the band below 3.8 kHz (waveform coding); LSD in dB; BLD: bark
loudness disturbance (PESQ-like idea, not calibrated to MOS; lower is better). The parametric high band has no waveform match, so the
full-band SNR is low for every KV4 profile by design.

| codec | kbit/s | SNRlo | segSNRlo | LSD | BLD |
|---|---|---|---|---|---|
| KV4 3.2 / 40 ms | 3.2 | 6.34 | 6.28 | 8.08 | 0.226 |
| KV4 4.8 / 40 ms | 4.8 | 9.13 | 9.01 | 7.46 | 0.168 |
| KV4 6.0 / 40 ms | 6.0 | 10.58 | 10.35 | 7.07 | 0.147 |
| KV4 8.0 / 40 ms | 8.0 | 11.70 | 11.51 | 6.63 | 0.127 |
| KV4 4.8 / 20 ms | 4.8 | 8.65 | 8.47 | 7.53 | 0.172 |
| KV4 6.0 / 20 ms | 6.0 | 10.25 | 10.04 | 7.12 | 0.149 |
| KV4 8.0 / 20 ms | 8.0 | 11.53 | 11.25 | 6.69 | 0.128 |
| KV4 HQ12 / 20 ms | 12 | 14.31 | 14.08 | 6.08 | 0.106 |
| KV4 HQ16 / 20 ms | 16 | 16.33 | 16.02 | 5.74 | 0.103 |
| old KaalCodec3 | 12 | 11.57 | 11.86 | 6.95 | 0.196 |
| old KaalCodec3 | 16 | 14.32 | 15.22 | 6.28 | 0.177 |
| old KaalCodec v0.1 | 16 | 7.19 | 6.76 | 8.12 | 0.263 |

Before/after: KV4 at 8 kbit/s reaches the waveform scores of the old codec at 12 kbit/s with a lower loudness disturbance
(0.127 vs 0.196); KV4 at 4.8 kbit/s has a lower BLD than the old codec at 16 kbit/s (0.168 vs 0.177). On the real speech sample alone
the old codec at 16 kbit/s keeps the better BLD (0.095 against 0.121 for HQ16, equal SNR 19.0 / 19.3 dB); at 12 kbit/s KV4 is better
there too (0.126 vs 0.157). Losses (4 s synthetic male, 6.0 kbit/s): 1 % of the 40 ms payloads lost costs 0.5 dB segSNRlo, 3 % costs
0.6 dB, 10 % 2.4 dB.

Real-time: CPU time of encode + decode per second of speech, one stream, on the development Mac (Apple silicon, heavily loaded at the
time): 0.007 .. 0.015 (the old codecs' wrapper: 0.033 .. 0.041, as it encodes three frames per frame). Not measured on a Raspberry
Pi 4; a Cortex-A72 at 1.5 GHz is typically 5 to 8 times slower on this kind of scalar float code, which would put four slots at
roughly 0.2 to 0.5 of one core.

Limits of these numbers: one real 2.6 s narrow-band sample; the synthetic speech is a formant synthesiser, and the LSF tables were
trained on it (held out: the real sample and other seeds); no listening; the postfilter lowers LSD/BLD slightly (it is meant for the ear,
not for these scores) and can be switched off (`Config::postfilter`).

## 6. Determinism

All arithmetic on the bit-stream path is + - * / and sqrt of IEEE floats/doubles; sine, cosine, log2 and exp2 are our own series;
the sources are compiled with `-ffp-contract=off` (no fused multiply-add). `test_kaalvoice_golden` hashes the bit streams and decoded
samples of six configurations (both frame lengths and rates, DTX, in-band type, noise suppressor, losses): the same digest on arm64
macOS and on x86-64 (built with `-arch x86_64`, also at -O3 with AVX2/FMA enabled). A changed digest means a changed bit stream.

## 7. Clean room and patents

Written from public textbook methods only: LPC/LSF (Itakura 1975), analysis-by-synthesis with an adaptive codebook (Atal/Schroeder,
1980s), pyramid vector quantisation (Fischer 1986), Lloyd-Max quantisers, KLT transform coding, postfiltering (Chen/Gersho 1987),
decision-directed Wiener noise suppression (Ephraim/Malah 1984), MDCT. No algebraic-codebook track structures of any standard, no
code or tables from codec2, AMR, Opus, mbelib or LPCNet; no neural networks; no downloaded data. All tables were measured on the
repo's own sample and on speech synthesised by `kaal_voice_eval`.

## 8. Tests

- `kaalvoice_roundtrip`: PVQ enumeration, LSF conversion, every layout and the pack/unpack round trip, quality floors per profile,
  delay, vowel and fricative, legacy wrappers bit-exact.
- `kaalvoice_robust`: random and PHY-style (40 ms pair) erasures, 400 ms burst (fade, recovery), damaged classes, bit-error sweep,
  class importance order, garbage then recovery.
- `kaalvoice_dtx`: frame types and sizes, SID rate, comfort-noise level, DTX does not hurt the speech, digital silence, lost SIDs, the
  noise suppressor.
- `kaalvoice_golden`: determinism digest (section 6).
