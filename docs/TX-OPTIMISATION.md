# TX optimisation (0.3.0)

## First half of the registry: acars .. iono (62 modes, tx-opt-a-030)

**How it was measured.** Every generator ran in a standalone harness linked against the core (`makeTxGenerator` with the mode's nominal rate, callsign N0ABC, grid KO24, text and name set, then the same 62 modes with empty fields = the demo signal), followed by the session's own output stage (`txOutputStage`) and limiter (`txSoftLimit`), 2 s of signal (FT modes 16 s). Columns:

- **x real time**: seconds of signal per CPU second of the generator thread (thread CPU time; the laptop was loaded by other builds, so read it as a ratio between before and after).
- **peak**: largest |sample| before the limiter (its knee is 0.9; 1e7 means the level was wrong and everything was hard limited). **in limiter %**: samples above the knee.
- **ACLR**: power in the 99 % occupied band against the same width beside it (starting a quarter width beyond the edge), worse side. "-": the neighbour band is outside the sample rate's span.
- **spur**: strongest bin outside twice the occupied band, relative to the strongest bin inside.
- **first 0.5 ms**: power of the first 0.5 ms against the rms (-99: the signal starts with silence; about 0 dB: the key-up was a step).

The test `tests/tx/test_tx_quality_a.cpp` (ctest `tx_quality_a`) measures the same things for these 62 modes, payload and demo, with limits from `test_tx_quality_a_limits.inc` (a few dB from the values below), plus: the first sample is 0, the key-down of a one-pass session fades, samples do not depend on the block size (blocks of 4093 vs 32768, except clock-driven modes), FT8 refuses an unpackable call with a reason, and the generator runs faster than 0.25x real time. `tx_quality_a_rt` (`--rt`) plays DRM, Contestia, DominoEX, FSQ, IFKP, ALE, BLE, CubeSat, Datawell and CW through the engine at two SNRs (in the occupied band) and carrier offsets and needs a lock (runs on CI; the local library could not be rebuilt here).

**Shared changes (all modes, both halves):**

- Key-up fade on by default: `out.rampMs` defaults to 2 ms (`kTxDefaultRampMs`), raised cosine instead of linear; a one-pass transmission also fades out over its last 2 ms; Stop on a real radio writes a 2 ms raised-cosine fade from the last sample before the radio closes (still inside `stop()`). The level is never raised; 0 in the Advanced block still turns it off.
- `genutil::fastPolar`: 4096-entry sin/cos table, error < 3e-7, for per-sample oscillators.
- `cell::Resampler`: polyphase rows (contiguous taps, two dot products and a blend) instead of a per-tap table interpolation.
- Clean-signal convention (`snrDb >= 99`, which `cleanSynth` sets for every TX) now honoured by the generators that ignored it: no receiver-test noise floor, fixed levels instead of "SNR over the noise".
- Packet (APRS) generator: same full-scale bug as CubeSat (other half's mode, same file).

| mode | x real time | peak | in limiter % | ACLR dB | spur dB | first 0.5 ms dB | change |
|---|---|---|---|---|---|---|---|
| acars | 114.7 -> 142.7 | 0.67 | 0 | 42.0 | -62.0 | -99 |  |
| adsb | 1219.1 -> 1223.7 | 0.27 | 0 | - | - | -99 |  |
| aero | 15.1 -> 13.6 | 0.677 | 0 | - | - | -0 -> -23 |  |
| airband | 45.5 -> 58.9 | 0.784 | 0 | 95.6 -> 95.5 | -107.5 | 2 -> -22 |  |
| ais | 200.0 -> 249.9 | 0.3 | 0 | 36.1 | -61.9 | -99 |  |
| ale | 24.9 -> 29.2 | 0.119 -> 0.011 | 0 | - -> 34.2 | - -> -44.8 | -0 -> -99 | payload and demo: no noise floor (it was 3x the signal power over the span); demo level no longer 190 dB over the noise |
| ant | 298.9 -> 546.5 | 0.5 | 0 | 33.1 | -40.3 | -99 |  |
| ardop | 72.5 -> 78.3 | 0.4 | 0 | 29.5 | -35.2 | -99 |  |
| astro | 15.7 -> 13.1 | 0.773 | 0 | - | - | -0 -> -24 |  |
| atsc | 3.5 -> 3.1 | 0.579 | 0 | - | - | 0 -> -24 |  |
| atsc3 | 7.3 -> 7.1 | 0.731 | 0 | - | - | 0 -> -23 |  |
| atv | 5.2 -> 5.3 | 0.493 | 0 | - | - | 2 -> -22 |  |
| ble | 10.0 -> 39.7 | 0.3 | 0 | 32.6 -> 34.0 | -31.7 -> -32.7 | -99 | Gaussian pulse from a table: 3x faster; 2 us ramps inside the burst padding |
| bletracker | 19.0 -> 36.1 | 0.3 | 0 | 32.4 -> 33.8 | -35.7 -> -36.2 | -99 | as BLE |
| cdr | 60.8 -> 51.5 | 0.765 | 0 | 53.8 | -70.9 | -0 -> -24 |  |
| cell | 1.2 -> 1.5 | 0.895 | 0 | - | - | -1 -> -23 | cell::Resampler polyphase rows (also GSM-R, NR, NB-IoT): 1.2x -> 1.8x |
| codar | 3.8 -> 5.8 | 6.56e+07 -> 0.202 | 100 -> 0 | - -> 63.3 | - -> -74.9 | -99 -> -23 | no noise or interference sweep in TX; level 0.25 instead of 6e7 (was hard limited) |
| contestia | 45.7 -> 52.6 | 0.542 -> 0.45 | 0 | 54.0 -> 92.9 | -28.5 -> -85.0 | -22 -> -99 | HF family: no -31 dBFS noise floor across the span; 8 kHz images -28 -> -85 dB (polyphase band-pass x32 then linear) |
| cquam | 22.6 | 0.476 | 0 | 85.8 -> 85.6 | -113.6 -> -113.4 | 3 -> -21 |  |
| cubesat | 51.1 -> 70.9 | 1 -> 0.4 | 27.5 -> 0 | 39.1 -> 39.2 | -57.4 | -99 | peak 0.4 instead of full scale (27 % of samples were in the limiter); packet path fixed too |
| cw | 37.7 -> 38.4 | 0.0447 | 0 | 26.4 | -35.3 | -99 | demo: level 0.1 instead of 2.6e7 (was hard limited); no noise |
| dab | 55.3 -> 51.8 | 0.807 -> 0.87 | 0 | - | - | -15 -> -38 |  |
| datawell | 78.0 -> 273.8 | 3.39e+07 -> 0.3 | 100 -> 0 | 30.4 | -39.3 | -99 -> -24 | level 0.3 instead of 3e7 (was hard limited); no noise; 4x faster |
| dect | 6.1 -> 5.7 | 0.5 | 0 | 26.3 | -28.2 | 10 -> -19 |  |
| dji | 15.5 -> 13.3 | 0.844 | 0 | - | - | -99 |  |
| dmr | 69.1 -> 61.8 | 0.22 | 0 | 38.5 | -66.4 | -99 |  |
| dominoex | 45.3 -> 58.9 | 0.549 -> 0.483 | 0 | 35.9 -> 35.3 | -32.1 -> -41.4 | -23 -> -99 | HF family fix (as contestia) |
| dpmr | 35.2 -> 28.4 | 0.05 | 0 | 38.4 | -57.6 | -99 |  |
| drm | 66.7 -> 39.0 | 0.647 | 0 | 35.8 -> 99.1 | -37.8 -> -86.4 | 0 -> -89 | channel filter on the clean TX signal: OFDM sidelobes -38 -> -86 dB |
| dstar | 28.2 -> 34.7 | 0.05 | 0 | 34.4 | -55.0 | -99 |  |
| dtmb | 3.0 -> 3.5 | 0.824 | 0 | - | - | 0 -> -24 |  |
| dvbc | 4.0 -> 3.5 | 0.527 | 0 | - | - | -2 -> -24 |  |
| dvbs | 4.8 -> 4.1 | 0.319 | 0 | - | - | -0 -> -24 |  |
| dvbt | 5.6 -> 5.2 | 7.52 -> 1.04 | 0.00048 -> 0.00042 | - | - | 1 -> -23 | start transient (peak 7.5 in the first samples) now inside the key-up fade |
| dvbt2 | 3.0 -> 3.4 | 1.03 | 0.00025 | - | - | -0 -> -24 |  |
| eczas | 291.7 -> 187.3 | 0.3 | 0 | 24.4 -> 32.6 | -25.6 -> -38.9 | -0 -> -24 |  |
| elektro | 19.7 -> 16.7 | 0.293 | 0 | 55.5 | -59.4 | -0 -> -24 |  |
| flarm | 45.3 -> 51.1 | 0.5 | 0 | 53.7 -> 53.3 | -73.1 -> -71.8 | -99 |  |
| fm | 40.5 -> 48.7 | 0.09 | 0 | 39.0 | -67.5 | 0 -> -24 |  |
| fpv | 0.7 -> 1.6 | 0.3 | 0 | - | - | 0 -> -24 | oscillators from a sine table: 0.7x -> 1.7x real time (was below real time) |
| freedv | 43.8 -> 54.4 | 0.722 -> 0.682 | 0 | 31.7 -> 70.6 | -30.4 -> -88.2 | -23 -> -99 | HF family fix |
| fsq | 41.2 -> 57.2 | 2.26e+07 -> 0.3 | 48.8 -> 0 | 33.5 -> 32.9 | -39.9 -> -39.6 | -99 | level 0.3 instead of 2e7 (was hard limited); no noise; 5 ms key-up/down ramp |
| fst4 | 51.3 -> 55.6 | 0.55 -> 0.484 | 0 | 52.7 -> 49.2 | -33.8 -> -57.5 | -22 -> -99 | as FT8 |
| ft2 | 55.1 -> 57.5 | 0.548 -> 0.477 | 0 | 48.6 -> 42.6 | -32.1 -> -65.4 | -22 -> -99 | as FT8 |
| ft4 | 56.5 -> 59.5 | 0.545 -> 0.49 | 0 | 48.3 -> 45.2 | -33.0 -> -62.0 | -22 -> -99 | as FT8 |
| ft8 | 54.7 -> 58.2 | 0.555 -> 0.45 | 0 | 51.1 -> 47.1 | -34.2 -> -62.7 | -23 -> -99 | HF family fix; demo sends FT8 (was RTTY); unpackable call (N0CALL) refused with a reason instead of a silent slot |
| general | 24.6 -> 27.7 | 0.3 | 0 | 47.7 | -106.4 | 0 -> -24 |  |
| globalstar | 8.2 -> 7.9 | 1.03 | 0.00153 -> 0.00152 | 71.5 -> 81.1 | -75.4 -> -83.9 | 2 -> -23 | chunks on a fixed grid (blocks of 4093 gave other samples), ramped second-channel bursts: ACLR 71 -> 81 dB |
| gnss | 7.2 -> 6.7 | 0.794 | 0 | - | - | 0 -> -24 |  |
| goes | 18.2 -> 18.5 | 0.293 | 0 | - | - | 0 -> -24 |  |
| goesdcs | 98.5 -> 100.1 | 0.3 | 0 | 28.4 | -35.5 | -99 |  |
| gsmr | 2.2 -> 2.7 | 0.55 | 0 | 69.8 | -134.2 | -0 -> -24 | faster resampler (cell) |
| halow | 2.4 -> 2.5 | 1.33 | 0.0269 -> 0.0268 | - | - | -99 | offset mixer by phasor |
| hamdrm | 43.9 -> 57.5 | 1.22 -> 0.859 | 0.486 -> 0 | 30.0 -> 45.0 | -27.8 -> -79.5 | -22 -> -99 | HF family fix; OFDM level capped at 0.65 (was 0.5 % in the limiter) |
| hdr | 27.5 -> 25.0 | 0.343 | 0 | 23.6 | -27.8 | 0 -> -24 |  |
| hell | 41.4 -> 58.8 | 0.528 -> 0.454 | 0 | - -> 24.1 | - -> -35.0 | -13 -> -99 | HF family fix |
| hfbeacon | 7.7 -> 6.7 | 0.0107 | 0 | 35.2 | -45.9 | 5 -> -20 |  |
| hfdl | 50.3 -> 46.4 | 0.157 | 0 | 55.5 | -58.0 | -99 |  |
| hrpt | 4.2 -> 3.8 | 0.362 | 0 | - | - | -0 -> -24 |  |
| ifkp | 44.2 -> 50.4 | 2.61e+07 -> 0.301 | 48.8 -> 0 | 36.3 -> 39.2 | -47.1 -> -50.2 | -99 | as FSQ |
| inmc | 67.3 -> 64.1 | 0.302 | 0 | 58.3 | -61.0 | -0 -> -25 |  |
| iono | 2.0 | 0.813 -> 0.195 | 0 | - -> 89.5 | - -> -95.9 | 1 -> -19 | no full-span noise (rms 0.2) in TX |

Demo signal (empty callsign / text) where it differs:

| mode | peak | in limiter % | ACLR dB | spur dB |
|---|---|---|---|---|
| airband | 0.784 | 0 | 95.6 -> 95.5 | -107.5 |
| ale | 8.66e+06 -> 0.011 | 70 -> 0 | 35.1 -> 35.0 | -41.5 -> -40.9 |
| ble | 0.3 | 0 | 32.6 -> 34.0 | -31.7 -> -32.7 |
| bletracker | 0.3 | 0 | 32.4 -> 33.8 | -35.7 -> -36.2 |
| codar | 6.56e+07 -> 0.202 | 100 -> 0 | - -> 63.3 | - -> -74.9 |
| contestia | 0.535 -> 0.45 | 0 | 54.1 -> 92.1 | -29.9 -> -85.1 |
| cquam | 0.476 | 0 | 85.8 -> 85.6 | -113.6 -> -113.4 |
| cubesat | 1 -> 0.4 | 42.5 -> 0 | 33.9 | -52.9 -> -53.0 |
| cw | 2.56e+07 -> 0.102 | 74.3 -> 0 | 26.6 -> 79.3 | -38.2 -> -109.8 |
| dab | 0.85 -> 0.9 | 0 | - | - |
| datawell | 3.39e+07 -> 0.3 | 100 -> 0 | 30.4 | -39.3 |
| dominoex | 0.549 -> 0.483 | 0 | 35.6 -> 35.0 | -32.1 -> -41.1 |
| drm | 0.663 | 0 | 35.7 -> 99.2 | -37.8 -> -86.4 |
| dvbt | 7.55 -> 1.01 | 0.00042 -> 0.00036 | - | - |
| eczas | 0.3 | 0 | 27.5 -> 32.6 | -44.0 -> -38.7 |
| flarm | 0.5 | 0 | 53.8 -> 53.3 | -72.8 -> -71.8 |
| freedv | 0.722 -> 0.682 | 0 | 31.7 -> 70.6 | -30.4 -> -88.2 |
| fsq | 2.26e+07 -> 0.3 | 48.8 -> 0 | 33.5 -> 32.9 | -39.9 -> -39.6 |
| fst4 | 0.555 -> 0.767 | 0 | 31.0 -> 28.9 | -39.9 -> -83.3 |
| ft2 | 0.555 -> 0.519 | 0 | 31.0 -> 54.8 | -39.9 -> -60.4 |
| ft4 | 0.555 -> 0.721 | 0 | 31.0 -> 32.6 | -39.9 -> -84.2 |
| ft8 | 0.555 -> 0.82 | 0 | 31.0 -> 32.3 | -39.9 -> -83.4 |
| globalstar | 1.03 | 0.00153 -> 0.00152 | 71.5 -> 81.1 | -75.4 -> -83.9 |
| halow | 1.33 | 0.0269 -> 0.0268 | - | - |
| hamdrm | 0.555 -> 0.655 | 0 | 31.1 -> 45.0 | -40.0 -> -79.5 |
| hell | 0.528 -> 0.454 | 0 | - -> 24.1 | - -> -35.0 |
| ifkp | 2.61e+07 -> 0.301 | 48.8 -> 0 | 36.3 -> 39.2 | -47.1 -> -50.2 |
| iono | 0.813 -> 0.195 | 0 | - -> 89.5 | - -> -95.9 |

**Left open (not changed here):**

- Very low levels that the rule "never raise output levels" keeps: CW typed message 0.045, D-STAR / dPMR 0.05, HF beacons 0.011, ALE 0.011 (about -27 to -40 dBFS). An owner decision.
- HaLow (PAPR 17 dB, 0.03 % in the limiter), DVB-T2 / DVB-T / Globalstar (about 0.002 %): OFDM peaks reach the soft limiter rarely; no crest-factor reduction added.
- BLE, BLE tracker, DECT, ANT: spurs -28 .. -40 dB two bandwidths out are the GFSK (BT 0.5) spectrum itself, not splatter (burst ramps were added for BLE and moved the figure by 1 dB).
- HD Radio: ACLR 24 dB is the hybrid digital sidebands beside the analog FM; the metric does not fit it.
- Underrun gaps on a real radio are still a step to zero (the sender has nothing to fade with).
- TETRAPOL (other half) has the same `scale_ = 1` clean-signal bug as packet / CubeSat had.
- Slowest after the changes (loaded laptop, E-cores): cell 1.5-1.8x, FPV 1.6-1.9x, iono 2x, HaLow 2.5x, GSM-R 2.7x real time.
