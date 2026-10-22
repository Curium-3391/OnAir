# Signal identifier

Click or drag over a signal on the spectrum or the waterfall (the Identify tool, key **I**, or Shift+drag) and OnAir says what it most likely
is: a ranked list of modes, each with a calibrated confidence, the evidence for it, whether that mode's own detectors confirmed it, and an
**Open** button that switches to the mode, tuned. **Identify all signals** does the same for every signal in the view.

Code: `core/include/dect2/engine/sig_ident.h`, `core/src/engine/sig_ident.cpp` (measurement, scoring, verify), `core/src/engine/sig_known.cpp`
(the hand-written signature table and the merged table), `core/src/engine/sig_known_gen.cpp` (generated rows), `app/specx/specx_ident.cpp`
(the card). Tests: `tests/specx/test_specx_idfeatures.cpp`, `test_specx_ident.cpp`, `test_specx_ideval.cpp`, `test_specx_idmatrix.cpp`.

## How it works

`identify(samples, rate, options)` is a pure, deterministic function: the same samples give the same answer. It takes 30 to 150 ms on a
laptop core for 0.6 to 2 s of samples. `Capture` collects the samples from the engine and runs it on a thread of its own.

### 1. The region and the spectrum

The click picks the signal under it (`regionAt`: runs of the averaged spectrum above the fitted floor, widened to where they meet the floor,
touching runs merged). `findRegions` lists every signal in the span for "Identify all signals". The region is measured
(`specx::measure`): centre, the widths at -3, -20 and -26 dB, the 99 % occupied bandwidth (power percentiles), SNR, flat top.

### 2. Features

The signal is mixed to 0 Hz and filtered to its width (at least 4 kHz of sample rate). Then:

| Feature | How |
|---|---|
| Duty cycle, bursts, burst length, frame period | Block power against an on / off threshold; edges refined to a quarter block; the median start-to-start spacing when it is regular |
| Envelope kurtosis | E\|s\|^4 / (E\|s\|^2)^2, noise taken out: 1 constant envelope, about 1.3 filtered PSK, 2 OFDM / Gaussian |
| Carrier line | A line within 4 % of the width of the centre (AM, unmodulated carriers) |
| Power lines | The strongest line of (x/\|x\|)^1, ^2, ^4, ^8: where they sit and how far apart tells BPSK, QPSK, pi/4-DQPSK, 8PSK, MSK and the carrier's own offset |
| Symbol rate | Cyclostationary lines: \|x\|^2 (linear modulation), the squared FM discriminator (FSK), the x^n line pairs, the chirp length, the OFDM symbol. Table rates with a line of their own are tried; a sharply keyed FSK whose line is its second harmonic is caught by sampling at the candidate rate and seeing the symbols come in equal pairs |
| FSK levels and deviation | k-means (2 and 4 levels) on the smoothed discriminator at the symbol instants |
| MFSK tones | The histogram of the discriminator averaged over half a symbol is a comb at the tone spacing; its autocorrelation gives the spacing (tried for 200, 60, 20, 6 Bd) |
| Spectral shape | A Welch spectrum at about 1/4000 of the width: discrete tones (6 dB over the band median, 4 dB prominence) and their common spacing, the strongest periodicity of the detrended spectrum (OFDM pilot comb, MFSK grid), and the shape in a word (flat top, rounded, tones, a line) |
| Roll-off | Linear modulation: 99 % width / symbol rate - 1 |
| Constellation | Linear modulation: a root raised cosine matched filter, the best of 8 timing phases, the carrier offset taken out; amplitude levels (k-means), \|mean u^M\| for M = 2, 4, 8 (any frequency error is taken by an FFT), symbol kurtosis; an order guess 2 / 4 / 8 / 16 / 64 |
| OFDM | The autocorrelation peak of the cyclic prefix gives the useful symbol Tu (carrier spacing 1/Tu), a line of y(t) y*(t + Tu) the symbol period and so the guard fraction |
| LoRa | y(t + T/4) y*(t) of a chirp is a tone at B/4 (and B/8 at half the lag), averaged over stretches shorter than a symbol so the data does not cancel it; gives SF and bandwidth |
| FM stereo | A 19 kHz pilot in the discriminator |
| Frame syncs | DMR, P25, NXDN, dPMR (4FSK soft symbols), POCSAG, AIS (bits) where the rate fits |

`featureRows()` lists all of them; the card's **Why?** shows that table.

### 3. The signature table

One row per mode the app can decode, in three layers (`signatures()`):

1. **Hand-written** (`sig_known.cpp`, `source` 0): modulation classes, symbol rates (OFDM: useful symbol lengths), 99 % widths, FSK
   deviation, continuous or bursty, burst length and period, frame sync, bands, and for MFSK modes the tone count and spacing.
2. **Measured** (`sig_known_gen.cpp`, `source` 1): for every mode with a test signal and no hand-written row, what this identifier measured on
   the mode's own generator at 30 dB SNR. Regenerate after adding a mode: `test_specx_idmatrix --signatures core/src/engine/sig_known_gen.cpp`.
   These rows are only as good as that one measurement; a hand-written row replaces one.
3. **From the mode list** (`source` 2): every ModeTuning and SubMode with neither. Only its name and usual frequency are known: it becomes a
   candidate (score 30, confidence at most 20 %) when the frequency is its usual one, so that "Open in" reaches every mode.

Rows without bands use the mode's usual frequency (ModeTuning::defMhz / SubMode::defMhz) as their band prior.

### 4. Scoring

Each row's rules add what they earn and what they could have earned; the score (0 to 100) is the share that holds, with a floor of 70
possible points (few measurable traits cannot reach the top). Rules: modulation class (35), symbol rate or OFDM symbol length (25 / 30), width
(15), deviation (12), duty (6), burst length and period (10 each), band (12) or usual frequency (12), MFSK tone spacing (10), constellation
order (6), OFDM pilot comb at a whole number of carriers (5), chirps or stereo pilot (20), frame sync (40). The frequency database and the band
plans add up to 10 when the frequency is known (`usePriors`); they never make a signal that does not fit into a match.

### 5. Verify

The best three candidates are checked with their own detectors at that mode's exact parameters (`Options::verify`, a few ms):

- frame sync found at least twice (DMR, P25, NXDN, dPMR, POCSAG, AIS)
- the symbol clock at the mode's rate within 0.4 % (finer than the scoring's 2 %)
- the cyclic prefix at the mode's own symbol length (OFDM)
- chirps of the mode's bandwidth (LoRa and the networks on it)
- burst length within 25 %, frame period within 6 %, deviation within 10 %, the stereo pilot

Each candidate gets *verified*, *not confirmed* or *inconclusive* with its notes. The clock, prefix and chirp checks move the score (+4 each
confirmed, -8 each failed, at most two); the others have counted already and only set the verdict.

### 6. Confidence

`z = -3.42 + 3.23 s/100 - 5.09 (s1 - s)/100 + 7.64 (s1 - s2)/100` (the last term for the best candidate only), `p = 1 / (1 + e^-z)`,
normalised when the candidates add up to more than one (a logistic fit on the test signals, `CSV=file test_specx_idmatrix`). Verdict:
**Unknown** below 30 %, except when several candidates tie within 3 points with a score of 50 or more and add up to 55 % (modes that look the
same on air: LoRa and its networks, NXDN48 and dPMR without a sync) - then **Ambiguous**, "one of these". **Likely** from 60 % with the
runner-up under half of it.

## How well it does

### Synthetic evaluation (`test_specx_ideval`, a few seconds, the quick CI tier)

Thirty cases of 27 modes made in the test from their published parameters (not with the app's generators, so not the data the table was
measured on): DMR, P25, NXDN48, NXDN96, dPMR with their frame syncs; POCSAG, AIS, RTTY, NAVTEX, D-STAR, radiosonde, GSM, APRS (AFSK on FM);
PSK31, Inmarsat-C, HFDL, TETRA, Meteor LRPT, VDL2; DAB, DRM, DVB-T 2k; FM broadcast with stereo, AM broadcast, CW; LoRa SF7/125 kHz and
SF9/250 kHz; ADS-B; Olivia 32/1000; FT8. Impairments: a random frequency offset up to 30 % of the sample rate, 100 ppm of symbol-rate error,
5 % of deviation error, noise at 10, 15 and 25 dB SNR over the signal's width (average power), a click up to 15 % of the width off the
middle; and once at 15 dB with the frequency known. Measured (macOS arm64):

| | top-1 | top-3 | group-level top-1 | unknown | wrong with >= 50 % |
|---|---|---|---|---|---|
| 10 dB | 53 % | 67 % | 57 % | 23 % | 13 % |
| 15 dB | 80 % | 87 % | 80 % | 10 % | 0 % |
| 25 dB | 83 % | 93 % | 87 % | 7 % | 0 % |
| 15 dB, frequency known | 83 % | 87 % | 83 % | | |

The verify step confirmed the truth in 82 of 104 runs where it was among the first three and failed it in 5. Thresholds (the test fails
under them): top-3 at 15 dB >= 76 %, at 25 dB >= 80 %, at 10 dB >= 56 %; group-level top-1 at 15 dB >= 70 %; wrong-and-sure at 15 dB <= 12 %.

Weak spots it shows: FT8 and Olivia in a 2 s capture (12 FT8 symbols; the tone comb needs a cleaner signal), RTTY at 45 Bd (90 symbols),
CW ties with the other carriers (Multitone, HF beacons), AM at 10 dB (only the carrier stands out), GSM at 10 dB taken for 4FSK.

### The app's own test signals (`test_specx_idmatrix`, minutes, slow tier)

Every mode with a generator, rendered, moved off the centre, noise added. `--quick` (20 dB, blind, 131 cases) measured 52 % top-1 and 63 %
top-3 (39 % and 47 % before the measured rows; the hand-written modes alone are unchanged at 48 % top-1), 7 % wrong with confidence >= 50 %.
The measured rows are trained on these same signals: the gain for those modes is not an independent result. The full run adds 10 and 30 dB
and the frequency known or wrong.

## What the numbers do not say

The confidence is calibrated on synthetic test signals, not on real ones: multipath, adjacent channels, a radio's own spurs, AGC and
real-world filtering are not in them. A mode that looks the same as another on the air (the WSJT family, Olivia and Contestia, LoRa and the
networks on it, NAVTEX and DSC) can only be told apart by its decoder; "Open" and listen is the real verify.

## Changing it

- A new mode: a hand-written row in `sig_known.cpp` (or run `--signatures`), then `test_specx_ideval` and `test_specx_idmatrix --quick`.
- A new feature: add it to `Features`, `featureRows()` and a rule; check the reliability table of the matrix (the confidence must stay honest:
  answers stated at 80 % or more right in at least 85 % of the cases).
