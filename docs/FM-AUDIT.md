# FM / RDS and AM audit (3.0, dec-audio-030)

Everything here was measured on synthetic signals (core/src/fm/fm_gen.cpp, tests/general/test_general_am.cpp); nothing on a real off-air
signal yet.

## How it is measured

FM: the generator's station (stereo 1 kHz left, 3 kHz right, pilot 9 %, RDS at 2 kHz injection: PS "TESTFM  ", RT "OnAir FM test signal")
at 1 Msps, 20 s, three noise seeds per point; carrier to noise in 200 kHz. Counted: RDS blocks usable (good or corrected), groups per second
shown (11.4 are sent), time to the whole PS and RT, every PS / RT ever shown that differs from what was sent, the stereo time, and the left
channel: SINAD of the 1 kHz tone (both tones fitted out) and the 3 kHz crosstalk from the right channel. The harness is a scratch program around
FmGenerator and FmReceiver; the regression checks are in tests/fm/test_fm_rds.cpp.

## What was found

1. **RDS had no error correction** and needed two clean blocks in a row to synchronise. The demodulator itself is at theory (the matched
   filter output at 18 dB carrier to noise is 8.0 dB, theory 8.1 dB), so the loss was all in the block layer.
2. **The multipath equaliser (CMA) hurt clean signals.** In noise its constant-modulus optimum is a narrower filter: it cuts the outer FM
   sidebands. On a quiet passage (no audio, only pilot and RDS) RDS lost 5 dB (constellation SNR 2.9 instead of 8.2 dB at 18 dB); on a mono
   station 2.3 dB; the stereo separation fell to the SNR (22 dB at 12 dB, 29 dB at 18 dB, 38 dB at 25 dB instead of 50 dB).
3. **Mono RDS** (no pilot): the Costas loop was updated per sample from the raw samples.
4. Dropped RDS groups: AF, ECC, language, PIN, 2B, ODA list, RT+, eRT, clock time, PTYN, EON. The generator sent C instead of C' in version B
   groups.

## What was changed

* **RDS data layer** (core/src/fm/fm_rds.cpp, new): bursts of one or two bits corrected (one wrong biphase symbol is two neighbouring bits
  after the differential decoding), and two weak symbols from the bit reliabilities (pairs among the six weakest). A group with a corrected
  block is used only when the same group came before from a different received word, so a miscorrection never reaches the screen (zero wrong
  PS / RT in 90 runs from 11 to 16 dB). Sync from two block ends a whole number of blocks apart. All the groups above; a group tap with
  redsea-style hex (rdsHexGroup), FmReceiver::setRdsCorrection (0 off, 1 bursts, 2 soft).
* **Equaliser only when an echo is there**: the mean power of the raw channel in 10 kHz bins of the instantaneous frequency; an echo makes
  it depend on the frequency (|H(f)|^2 swept by the modulation), noise does not. Above 6 % (plus 0.6 % per dB of SNR below 30 dB) the equaliser
  is used, below 3.5 % not. Measured: echo 10 us -6 dB 52 %, 4 us -3 dB 25 %, 5 us -10 dB 22 %, 3 us -20 dB 5.5 % (left off: it does
  no harm there), clean 0 - 1.6 %. Below about 22 dB SNR the sound still takes the equaliser's output (its narrower filter is a threshold
  extension there); RDS keeps the raw channel.
* **Mono RDS**: the Costas loop runs once per symbol on the matched filter output.
* **Stereo blend** (Auto only): full stereo above 36 dB of the SNR measure (about 22 dB carrier to noise), mono below 24 dB, over a second;
  "Stereo" keeps full stereo. Telemetry stereoPct, shown as "stereo, blended 40 %".
* **UI** (app/fm/fm_ui.cpp): Now playing (RT+ artist - title), eRT, the name while it is being received, RDS blocks repaired, Multipath, an
  "RDS details" section (PTYN, station clock, AF list, ECC, PIN, EON, other RT+ tags, ODA list, block counts). rx_json: the new fields.
* **AM** (core/src/general/general_rx.cpp): a synchronous detector (both sidebands, upper only, lower only) and a noise blanker, both off
  by default (panel: Detector, Blanker).

## Before and after: RDS

Usable blocks %, groups per second shown, time to the whole PS (s). Before = integ-30 at the start.

| C/N dB | stereo before | stereo after | mono before | mono after | quiet audio before | quiet audio after |
|---|---|---|---|---|---|---|
| 12 | 0.3, 0.00, - | 39, 0.47, 16 | 0, 0, - | 39, 0.28, 15 | 0, 0, - | 39, 0.35, 17 |
| 14 | 15, 0.18, - | 76, 5.1, 3.4 | 0, 0, - | 77, 5.1, 2.1 | 0, 0, - | 76, 5.0, 4.3 |
| 16 | 46, 2.6, 3.2 | 97, 10.0, 1.1 | 6, 0.02, - | 97, 10.0, 1.3 | 0, 0, - | 97, 9.8, 1.3 |
| 18 | 82, 7.9, 0.9 | 99.9, 11.2, 0.7 | 28, 0.85, 4.0 | 99.9, 11.1, 0.7 | 3, 0.15, - | 99.5, 11.1, 1.1 |
| 20 | 98, 10.7, 0.7 | 99.8, 11.4, 0.6 | 69, 6.2, 2.8 | 99.7, 11.3, 0.6 | 36, 1.7, 0.7 | 99.5, 11.3, 0.8 |
| 25 | 100, 11.4, 0.6 | 100, 11.4, 0.6 | 100, 11.4, 0.6 | 100, 11.4, 0.6 | 96, 10.8, 0.6 | 99.7, 11.4, 0.6 |

The 90 % block threshold moved from 19.5 to 15 dB in stereo, from about 22 to 15 dB on a mono station and from about 25 to 15.5 dB on a quiet
passage. Time to RT at 18 dB: 9.3 s before, 5.0 s after. Wrong PS / RT shown: 0 before, 0 after (90 runs at 11 - 16 dB after).
FM impairment test (test_fm_impair): RDS groups equal or more in every case; a neighbour 150 kHz away 10 dB stronger: 12 -> 44 groups
(standard filter) and 6 -> 34 (150 kHz filter), the PS now decoded there; a weak station beside a strong one 84 -> 90.

## Head-to-head: redsea 1.3.1 (built from github.com/windytan/redsea in a scratch folder)

The same IQ: OnAir's receiver on it; redsea on MPX from a plain channel filter (+-150 kHz) and discriminator at 250 kHz (S16LE, -r 250000;
500 kHz input was worse for redsea). Groups per second with blocks B and D (redsea -o hex), three seeds:

| C/N dB | OnAir | redsea | redsea wrong PS lines |
|---|---|---|---|
| 14 | 6.5 | 0 | 0 |
| 16 | 10.7 | 0 | 0 |
| 18 | 11.35 | 1.4 | 0 |
| 20 | 11.35 | 7.6 | 1 (mono 5) |
| 25 | 11.35 | 11.2 | 0 |

About 6 dB more sensitive than redsea on this signal. Part of redsea's loss may be the plain front end given to it; OnAir before this work
(19.5 dB) was already ahead.

## Before and after: FM sound

Left channel SINAD dB / crosstalk dB (stereo), and SINAD on a mono station:

| C/N dB | stereo before | stereo after | mono before | mono after |
|---|---|---|---|---|
| 8 | 18.3 / 18.8 | 21.5 / blended | 32.6 | 32.6 |
| 10 | 20.2 / 20.3 | 30.7 / blended | 32.1 | 31.4 |
| 12 | 22.2 / 22.1 | 30.4 / blended | 34.9 | 36.7 |
| 14 | 24.4 / 24.2 | 28.8 / blended | 23.2 | 38.9 |
| 16 | 26.3 / 26.5 | 28.7 / 10 (60 %) | 26.1 | 40.9 |
| 20 | 30.0 / 31.4 | 30.3 / 22.5 (83 %) | 32.6 | 45.0 |
| 25 | 34.8 / 37.6 | 34.7 / 50.7 | 34.1 | 50.0 |
| 40 | 49.5 / 49.9 | 49.6 / 50.8 | | |

The mono station before was distorted by the equaliser (23 dB at 14 dB). Stereo below 22 dB is a trade: less hiss for less separation;
"Stereo" in the panel keeps the old behaviour without the equaliser's crosstalk.

## AM (tests/general/test_general_am.cpp)

Measured in the prototype (48 kHz, 9 kHz channel); the test checks the same on the receiver:

| case | envelope | synchronous | sync upper | sync lower |
|---|---|---|---|---|
| AWGN 0 / 6 / 20 dB, SINAD | -7.8 / -0.4 / 14.0 | -6.0 / 0.0 / 13.9 | -9.0 / -2.9 / 11.1 | -8.7 / -2.8 / 11.1 |
| selective fading 0.1 / 0.2 / 0.3 ms, 1 kHz over harmonics | 21.9 / 13.3 / 7.3 | 64 / 55 / 48 | 69 / 56 / 49 | 63 / 55 / 45 |
| station 5 kHz above, 10 dB stronger, SINAD | -4.9 | -3.9 | -9.9 | 20.0 |
| impulses +20 / +30 dB, blanker off -> on | 1.8 -> 19.8 / -8.1 -> 19.8 | | | |

## Known limits

* Nothing was checked on a real station: real multipath, real RDS encoders (2B, eRT, RT+ in the field), real impulse noise.
* RDS2 (the extra subcarriers) is not decoded. The ECC is shown as a number (no country table). The AF list collects the frequencies of
  method A and B lists alike.
* The equaliser still fails on a 20 us echo at -6 dB (before and after).
* The FM sound below 9 dB C/N still depends on the equaliser's threshold extension; no PLL / feedback demodulator.
* AM synchronous detection on two stations sharing a channel follows the stronger carrier; the envelope detector stays the default.
