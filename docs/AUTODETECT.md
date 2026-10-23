# Automatic standard detection ("TV (auto)", "Digital voice (auto)")

Two mode-list rows find the standard from the signal and switch the receiver on: **TV (auto)** (TV group) and **Digital voice (auto)**
(two-way radio group). Code: `core/include/dect2/autodet/autodet.h`, `core/src/autodet/`, the app side `app/autodet/autodet_ui.cpp`
(status line at the top of the status bar), the tool `autodettool tv|voice <file>` for recordings. Nothing is decoded or decrypted here;
the receivers that exist do that.

## How TV is told apart

| Step | What it measures | Tells |
|---|---|---|
| spectrum | occupied width (half-way between floor and plateau), edge steepness, narrow lines | analogue (vision + sound carrier 4.5 / 5.5 / 6.0 / 6.5 MHz apart: M/N, B/G, I, D/K-L), ATSC 1.0 (pilot at the lower edge) |
| autocorrelation | `|R(L)|/R(0)` of the whole capture (one FFT) at the useful-symbol length of every mode: DVB-T 2K/4K/8K and T2 1K..32K at 1.7/5/6/7/8 MHz, ISDB-T modes 1-3 at 6/7/8 MHz, ATSC 3.0 8K/16K/32K at fs 6.144/6.912/8.064/9.216 MHz, the DTMB PN420/PN945 headers (255 / 511 symbols) | OFDM standard, FFT size, channel width |
| guard fold | x[n] x*[n+L] folded at the symbol period of each guard: the true one puts the product in one window (max/median, real signals 60-300) | guard interval; rejects lags that come from repeated data or continual pilots (fold 1-3) |
| P1 | part C of P1 repeats 542 T later, frequency shifted by 1/(1024 T) | DVB-T2 against DVB-T (2K/4K/8K; 1K/16K/32K and the T2-only guards are T2 anyway) |
| single carrier | line of \|z\|^2 at Rs (aliases followed), roll-off, 4th moment at the symbol instants (1 PSK, 1.3-1.4 QAM), 4th/8th power lines, DVB-S2 SOF with equal frame spacing | DVB-S/S2 (Rs, QPSK/8PSK) or DVB-C (Rs, QAM); a satellite IF (950-2150 MHz) means DVB-S/S2 |

The receivers then find the rest themselves (DVB-S vs S2 and MODCOD, the DVB-C QAM order, TPS / L1). Verdict: **sure** (strong fold,
z >= 12, width fits), **likely**, **unsure** (a close runner-up of another standard, or nothing fits: the app looks again up to three
times and then asks to pick by hand).

## How voice is told apart

Every narrowband signal in the span (sigid::findRegions) is brought to baseband (~48 kHz and ~144 kHz for TETRA) and low-passed for each
symbol rate tried; one soft level per symbol is the phase advance over a symbol (4FSK: 2 pi f T; pi/4-DQPSK and H-DQPSK: the phase step),
timing per block of 256 symbols. Frame syncs are searched in parallel with a symbol-error count (at most L/8 wrong; short syncs exact and
only when repeated at the frame period): DMR (all 9), P25 phase 1 frame sync, NXDN FSW (48 and 96), dPMR FS1-FS4, Yaesu Fusion FSW, M17
(LSF / stream / packet / BERT), D-STAR data sync (period 21 frames), P25 phase 2 sync (6000 Bd), TETRA training sequences (18000 Bd).
Evidence = hits x sync length. Sure: enough hits and the runner-up below a quarter; no sync but a symbol clock: "a digital signal ... no
frame sync of a known system" (unsure); no sync, no clock, constant envelope: analogue FM (likely). Several systems in the span are all
listed ("DMR on 446.0063 MHz, P25 Phase 1 on 446.2063 MHz"), the strongest sure one is played, a button switches to another, and the span is
looked at again every 20 s.

## Measured (macOS arm64, 2026-10-23)

`test_autodet_voice` (quick tier): each system's own generator, 1 Msps, 1.2 s, random offset up to +-300 kHz, 40 ppm clock, noise over the
signal's width. 15/10/6/3 dB: 34 of 44 right, 30 'sure' answers all right, 0 wrong. Weakest SNR still right: P25 1 and P25 2 at 3 dB; DMR,
NXDN48/96, D-STAR, dPMR, Fusion, TETRA, M17 at 6 dB; analogue FM at 10 dB. Neighbours DMR -100 kHz / P25 +100 kHz / NXDN96 -75 kHz all
found; inverted spectrum found and reported inverted; noise alone gives no 'sure' answer.

`test_autodet_tv` (slow tier; `--quick` one case per standard runs with the quick tier): 24 cases (DVB-T 2K/8K all four guards and 6/7/8
MHz, DVB-T2 1K..32K incl. 32K extended 1/8 PP8, 1.7 and 7 MHz, ISDB-T 6/8 MHz, ATSC, ATSC 3.0, DTMB PN420/PN945, DVB-S, DVB-S2 QPSK/8PSK,
DVB-C 16/64/256-QAM, analogue B/G, I, M), off centre, 20 ppm clock, 12/6/3 dB: 71 of 72 right with all parameters (FFT, guard, width, Rs
within 1 %); the one miss is DVB-T2 8K GI 1/128 at 3 dB (unsure, not wrong). 'Sure' and wrong: 0.

Real recordings (`autodettool`): the 474 MHz capture (10 Msps): DVB-T2 32K GI 1/8 8 MHz, sure, 4 of 4 captures; the 554 MHz DTMB capture:
DTMB PN945 8 MHz, sure, 4 of 4 (strong multipath, +-15 dB ripple); the 5 MHz DVB-T WAV: DVB-T 2K GI 1/4 5 MHz (likely: only 4 dB SNR);
the DAB recording: "unclear" (not a TV standard, nothing claimed). Not tried on real signals: DVB-S/S2, DVB-C, ISDB-T, ATSC, analogue TV
and every voice system (no recordings here).

## Limits

- TV needs the whole channel inside the capture; a DVB-S transponder wider than the radio's rate is measured on what is seen.
- The synthetic ATSC 3.0 signal of this program runs at 6.144 MHz; the detector also knows 6.912 / 8.064 / 9.216 MHz but only the 6.144 one
  is tested. The `atsc3_sample.cs8` recording has no known rate and was not used.
- DTMB PN595 (no cyclic structure) is found only through the 7.56 / 5.67 Msym/s line of its single-carrier form; the multi-carrier PN595
  form is "unsure".
- Confidence figures are rules, not a calibration on real signals; voice syncs at 6 dB and below miss sometimes (then "unsure", not wrong).
