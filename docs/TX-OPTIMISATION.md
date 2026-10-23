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
# Transmit optimisation (0.3.0)

Every mode of the TX registry (core/src/tx/tx.cpp) measured and tuned: speed, signal quality, timing, round trips, UI. Two halves by mode id,
one section each.

## Second half of the registry: iridium .. zwave (branch tx-opt-b-030)

The 61 mode ids after the first ceil(123 / 2) = 62 in sorted order: iridium isdbt ism js8 jt65 jt9 kaal kaft kait kapp kavb kaws lbj loraraw m17 mac
marine mesh meteor mfsk modeac mscatter msk144 mt63 multitone nbiot ndb nr nxdn olivia orbcomm p25 packet pager psk q65 qo100 rid rtty same satbeacon
sonde specan specx sstv tetra tetrapol thor timesig uat uplink vdes vdl2 vdl4 vorils wifi wisun wmbus wspr zigbee zwave.

### How it is measured

`tests/tx/test_tx_quality_b.cpp` (ctest `tx_quality_b`, nothing is transmitted) runs every mode's `makeTxGenerator` at its nominal rate with an
operator's payload filled in (callsign LY2TX, grid, a text, an address), 0.5 to 2 s of signal (JS8 32 s at 250 ksps and KAWS 125 s, for their UTC
slots), in the session's 32768-sample blocks, and measures the IQ:

- real time x: seconds of signal per second of thread CPU time (the machine was shared with other builds; `nice -n 19`)
- peak and the share of samples at or above the output limiter's knee (0.9 of full scale); above 1.0 would be clipped
- spectrum: Welch, Blackman-Harris, 4096 bins. B = the 99 % occupied band, fc its centre. ACLR = power in fc +- 0.75 .. 1.75 B over the band (worse
  side), far = power beyond fc +- 3 B over the total, spur = the strongest bin beyond 1.5 B over the strongest in the band. "-" = nothing measurable
  (the band fills the span, or no power there)
- DC = |mean| / rms: a carrier at the IF centre the mode does not ask for
- edge step = the largest change of the envelope between two samples over the peak (1.0 is a hard key-up)

Limits now enforced for every mode of this half: no NaN / Inf, a signal, nothing above full scale, under 1 % of samples at the limiter knee, faster
than 1.5 x real time, far < -40 dBc (UAT -30, see below), DC < -20 dB (KAAL and TETRAPOL excepted, see below), and an edge step under 0.5 for every
mode on the burst-ramp list. A round trip RTTY transmitter -> HF receiver over white noise at 30 / 15 / 10 dB in 3 kHz and +-25 Hz carrier offsets
must give the text back each time (the HF sideband path is the one all the WSJT and keyboard modes of this half share).

### What was wrong and what changed

- **Level 10^10 too strong: PSK31, JS8, MT63 demo signals.** The synths added the transmitter's 200 dB "clean" SNR to the stations' levels: peaks of
  6e6 .. 1.3e8 into the limiter, which flattened the PSK31 envelope and splattered. With snrDb >= 150 they now leave the noise out and put the strongest
  station at 0.25 / 0.2 / 0.15 rms (`psk_gen.cpp`, `js8_gen.cpp`, `mt63_gen.cpp`).
- **Full-scale constant envelope into the limiter: packet (APRS), SAME, TETRAPOL.** At 200 dB the noise sigma was tiny but not zero, so the "no noise:
  0.4 / 0.35" level never applied and the signal ran at 1.0 (28 / 43 / 99.7 % of the samples in the limiter). Now 0.4 / 0.4 / 0.35.
- **VDES clipped:** the four-channel 16-QAM bursts peaked at 1.23; the cycle is scaled to a 0.8 peak for a transmitter.
- **HF sideband path (RTTY, SSTV, JT65, JT9, Q65, MSK144, WSPR, MFSK, Olivia, THOR, ...): noise and images on the air.** Every transmission carried
  -31 dBFS of white noise over the whole 2 MHz span (the receiver-test noise), and the 8 kHz sideband was linearly interpolated straight to the
  radio rate, which left copies at 8, 16, 24 kHz at -20 .. -35 dBc. Now no noise for a transmitter and an 8 -> 48 kHz polyphase low-pass (576 taps,
  about 80 dB) before the linear step: far -22 -> -50 .. -65 dBc, spurs -29 .. -44 -> -52 .. -69 dBc (`hfdig_gen.cpp`). The same file serves FT8 /
  FT4 / FreeDV / HamDRM of the first half.
- **Multitone paging:** -60 dB of noise (the synth capped the SNR at 60 dB) over 2 MHz, hard carrier keying: far -37.8 -> -87.5 dBc.
- **Hard key-up / key-down: M17, NXDN, P25, VDL4, LoRa (raw), wM-Bus, Z-Wave, UAT, ISM, Wi-Fi, multitone.** `core/include/dect2/tx/tx_shape.h`
  (`TxBurstShaper`, applied in `makeTxGenerator` to the modes in `txBurstRampSec`) gives every burst and the start of the transmission a raised-cosine
  ramp of about one to two symbols, looking ahead so the end of a burst is ramped too; inside a burst nothing changes, the gain is never above 1.
  M17 far -46.8 -> -79.2, NXDN -54.1 -> -97.4, P25 -56.1 -> -87.4, VDL4 ACLR -45.3 -> -94.8 dBc.
- **JS8 sent nothing** with a text and no recipient (the first frame needs one: now @ALLCALL), and its frames were not on the UTC 15 s slots
  (the slot clock started at Start). Now the slot clock comes from the system clock and the first frame goes in the next slot (`tx_payload.cpp`).
- **UAT** played its four bursts back to back once and then stayed silent for good; now 10 ms after each and the schedule again after 1 s.
- **Spectrum analyser / Explorer:** the analyser scene sent its simulated LO leak (DC at -1.1 dB of the rms) and a noise floor; both are left out for
  a transmitter (DC -101 dB). Explorer's carriers are phasors now.
- **Speed** (CPU seconds per second of signal, measured in isolation with the same code before and after, outputs compared sample by sample):

  | generator | before | after | change | output difference |
  |---|---|---|---|---|
  | 5G NR, 20 Msps, two cells | 1.566 | 0.459 | 3.4 x | -93 dB (an exact polyphase resampler for the rational step 48/125 instead of a per-tap table interpolation; firDot) |
  | Drone Remote ID, 20 Msps (BLE + Wi-Fi at 40 Msps) | 2.40 | 0.58 | 4.1 x | -133 dB (a BT 0.5 Gaussian pulse table instead of five erf pairs per sample, a sine table, vectors instead of a deque, firDot for the channel filter) |
  | Wi-Fi, 20 Msps | 0.141 | 0.073 | 1.9 x | exact (phasors instead of cos / sin per sample and position, a vector queue, Horner Catmull-Rom) |
  | Wi-SUN, 20 Msps | 0.312 | 0.196 | 1.6 x | -133 dB (sine table) |
  | Spectrum analyser, 20 Msps | 0.489 | 0.043 | 11 x | carriers only (no per-sample Gaussian noise or cos / sin) |
  | Explorer, 10 Msps | 0.270 | 0.113 | 2.4 x | -179 dB (phasors, no noise at 200 dB) |

  NR (0.7 x) and Remote ID (0.4 x) could not keep up with the radio before; both now do. New helpers: `core/include/dect2/dsp/fir_dot.h`
  (a complex FIR dot product with sixteen independent accumulators that the compiler vectorises without -ffast-math) and
  `core/include/dect2/dsp/fast_sincos.h` (a 4096-point sine table, error below 3e-7).

### Known and left as they are

- **UAT** writes dump978-style phase steps (two samples a bit at any rate: 1.0 Mbit/s and +-12 kHz at 2 Msps instead of 1.041667 Mbit/s and
  +-312.5 kHz). It decodes in OnAir's own receiver but is not the standard waveform; fixing it needs the receiver changed with it. Far limit -30 dBc.
- **KAAL** (DC -4 dB of the rms) is the owner's own waveform; not changed. **TETRAPOL** sends one test frame over and over: its line spectrum has a
  line at the centre (DC -10 dB). Both are excepted from the DC limit.
- **Wi-Fi DSSS** chips are rectangular (sample and hold) with no pulse shaping; at 20 Msps the main lobe is wider than the span, so it cannot be
  measured here. **NB-IoT** ACLR -26.7 dBc and **ISM** -32 dBc (OOK test scene) are the generators' own shapes; not changed.
- **Iridium, LoRa mesh, NB-IoT, Mode A/C, uplink** still step hard (0.8 .. 0.9) but have no silence of exact zeros between their bursts for the
  shaper to see (Mode A/C and the uplink are 0.25 us pulses at 4 / 10 Msps: a ramp would need more rate).
- **ISDB-T** makes a whole frame at a time (worst block 6.7 x the block time, 4.6 x real time overall): the session's queue carries it.
- **Round trips** at several SNRs and offsets run here for RTTY only; packet, pager, AIS, DMR, CW, FT8 keep their engine round trips in
  `tests/tx/test_tx.cpp`. The others are still "unverified" in the registry.
- **UI:** the advanced table (`app/panels/tx_params.cpp`) already gives every field a unit, a range and a default; nothing changed there.

### Before / after per mode

Before = the same test against the code before this branch (the prebuilt library of preview-031), after = this branch. "a -> b" where it changed.
Speed numbers are from a shared, busy machine and vary by +-20 %; the table above has the isolated timings.

| mode | rate | real time x | peak | >0.9 FS | ACLR dBc | far dBc | spur dBc | DC dB | edge step |
|---|---|---|---|---|---|---|---|---|---|
| iridium | 10 M | 8.5 -> 10.1 | 0.574 | 0.0% | - | - | - | -134.2 | 0.90 |
| isdbt | 8.04 M | 4.5 -> 4.7 | 0.855 | 0.0% | - | - | - | -47.1 | 0.85 |
| ism | 2 M | 12.2 | 0.100 | 0.0% | -30.5 -> -32.1 | -43.7 -> -46.4 | -47.0 -> -50.4 | -111.4 -> -79.8 | 0.28 -> 0.29 |
| js8 | 250 k | 25625 -> 228.5 | 0.000 -> 0.028 | 0.0% | - -> -54.6 | - -> -72.6 | - -> -67.6 | - -> -130.6 | 0.00 |
| jt65 | 2 M | 54.8 -> 60.3 | 0.544 -> 0.471 | 0.0% | -52.0 -> -50.2 | -22.9 -> -63.4 | -32.4 -> -66.4 | -95.0 -> -107.5 | 0.26 -> 0.00 |
| jt9 | 2 M | 64.0 -> 60.0 | 0.553 -> 0.489 | 0.0% | -52.4 -> -56.8 | -23.1 -> -63.8 | -33.8 -> -66.9 | -92.1 -> -107.6 | 0.26 -> 0.00 |
| kaal | 48 k | 35.3 -> 35.8 | 0.473 | 0.0% | -46.4 | - | -49.0 | -4.1 | 0.36 |
| kaft | 2.4 M | 220.7 -> 219.0 | 0.626 -> 0.621 | 0.0% | -63.1 | -82.1 | -68.7 -> -68.1 | -48.4 -> -47.5 | 0.16 -> 0.17 |
| kait | 632 k | 964.9 -> 1075 | 0.900 | 0.0% | -54.5 | - | -58.1 | -94.0 | 0.53 |
| kapp | 250 k | 915.0 -> 870.0 | 0.611 | 0.0% | - | - | - | -33.8 | 0.63 |
| kavb | 2.8 M | 15.0 -> 14.7 | 0.622 | 0.0% | -53.5 | - | -56.5 | -36.2 | 0.51 |
| kaws | 12 k | 1791 -> 1826 | 0.606 | 0.0% | -88.9 -> -90.0 | -113.2 -> -114.8 | -103.3 -> -103.4 | -110.3 -> -115.3 | 0.36 |
| lbj | 960 k | 67.7 -> 70.1 | 0.450 | 0.0% | -49.8 | -101.3 | -82.5 | -113.7 | 0.00 |
| loraraw | 2 M | 359.5 -> 109.0 | 0.300 | 0.0% | -35.4 -> -35.5 | -49.1 -> -50.9 | -44.6 -> -44.9 | -102.7 -> -105.4 | 1.00 -> 0.00 |
| m17 | 2 M | 84.5 -> 56.2 | 0.050 | 0.0% | -40.0 -> -41.3 | -46.8 -> -79.2 | -53.6 -> -79.6 | -122.0 -> -129.3 | 1.00 -> 0.00 |
| mac | 20 M | 7.6 -> 7.3 | 0.386 | 0.0% | - | - | - | -49.6 | 0.43 |
| marine | 2 M | 72.4 -> 72.0 | 0.215 | 0.0% | -55.1 | -71.8 | -67.6 | -107.8 | 0.01 |
| mesh | 2 M | 80.8 -> 78.4 | 0.449 | 0.0% | -46.5 | -60.7 | -57.1 | -115.4 | 0.89 |
| meteor | 1 M | 11.3 -> 14.6 | 0.334 | 0.0% | -99.3 | -109.6 | -111.1 | -128.5 | 0.11 |
| mfsk | 2 M | 56.7 -> 53.1 | 0.540 -> 0.482 | 0.0% | -50.7 -> -49.5 | -22.7 -> -65.3 | -36.0 -> -68.8 | -91.4 -> -106.9 | 0.26 -> 0.00 |
| modeac | 4 M | 46.2 -> 46.3 | 0.450 | 0.0% | - | - | - | -72.7 | 0.88 |
| mscatter | 2 M | 79.8 -> 81.5 | 0.300 | 0.0% | -82.7 | -108.5 | -100.8 | -69.0 | 0.00 |
| msk144 | 2 M | 64.5 -> 59.9 | 0.689 -> 0.636 | 0.0% | -34.6 -> -45.7 | -23.2 -> -59.4 | -29.2 -> -63.0 | -91.3 -> -109.9 | 0.20 -> 0.00 |
| mt63 | 2 M | 52.6 -> 53.3 | 132293904.337 -> 0.500 | 100.0% -> 0.0% | -46.9 | -49.7 | -49.7 | -104.4 | 0.00 |
| multitone | 2 M | 18.2 -> 40.9 | 0.517 -> 0.500 | 0.0% | -52.3 -> -55.4 | -37.8 -> -87.5 | -60.8 -> -76.0 | -102.0 -> -113.7 | 0.96 -> 0.00 |
| nbiot | 2 M | 9.7 -> 11.0 | 0.939 | 0.0% | -26.7 | -43.3 | -30.9 | -45.5 | 0.82 |
| ndb | 256 k | 231.6 -> 232.7 | 0.450 | 0.0% | -49.6 | -53.5 | -62.7 | -110.3 | 0.26 |
| nr | 20 M | 0.7 -> 1.9 | 0.779 | 0.0% | - | - | - | -81.8 | 0.91 |
| nxdn | 2 M | 38.1 -> 30.3 | 0.050 | 0.0% | -40.2 -> -40.6 | -54.1 -> -97.4 | -57.9 -> -84.9 | -109.7 -> -115.5 | 1.00 -> 0.00 |
| olivia | 2 M | 61.2 -> 59.6 | 0.531 -> 0.450 | 0.0% | -49.0 -> -47.1 | -21.5 -> -61.7 | -30.9 -> -64.5 | -90.0 -> -105.6 | 0.27 -> 0.00 |
| orbcomm | 1 M | 12.0 -> 12.5 | 0.354 | 0.0% | -78.7 | -83.6 | -91.7 | -124.2 | 0.15 |
| p25 | 2 M | 29.4 | 0.050 | 0.0% | -42.0 -> -42.3 | -56.1 -> -87.4 | -60.5 -> -81.2 | -120.0 -> -114.8 | 1.00 -> 0.00 |
| packet | 2 M | 65.8 -> 68.8 | 1.000 -> 0.400 | 28.1% -> 0.0% | -42.5 | -55.0 | -55.8 | -113.1 | 0.05 |
| pager | 2 M | 21.5 -> 20.4 | 0.450 | 0.0% | -68.3 | -105.0 | -98.7 | -115.4 | 0.00 |
| psk | 2 M | 30.7 -> 31.0 | 0.011 | 0.0% | -88.1 | -112.0 | -104.4 | -115.0 | 0.00 |
| q65 | 2 M | 62.9 -> 59.6 | 0.547 -> 0.482 | 0.0% | -52.2 -> -50.2 | -23.0 -> -62.6 | -33.3 -> -65.4 | -94.6 -> -107.6 | 0.26 -> 0.00 |
| qo100 | 10 M | 20.3 -> 20.5 | 0.217 | 0.0% | -53.5 | -85.6 | -76.0 | -64.8 | 0.00 |
| rid | 20 M | 0.4 -> 1.6 | 0.600 | 0.0% | - | - | - | -47.1 | 0.93 |
| rtty | 2 M | 61.8 -> 59.6 | 0.555 -> 0.507 | 0.0% | -31.1 -> -49.3 | -22.1 -> -50.2 | -44.2 -> -52.3 | -92.2 -> -107.0 | 0.26 -> 0.00 |
| same | 2 M | 74.6 -> 66.3 | 1.000 -> 0.400 | 42.7% -> 0.0% | -44.1 | -55.3 | -57.9 | -115.8 | 0.05 |
| satbeacon | 2 M | 12.7 -> 10.9 | 0.350 | 0.0% | -71.9 | - | -108.8 | -109.4 | 0.46 |
| sonde | 8 M | 30.1 -> 30.7 | 0.200 | 0.0% | -50.8 | -73.7 | -70.9 | -21.8 | 0.00 |
| specan | 20 M | 2.5 -> 25.8 | 0.086 -> 0.056 | 0.0% | - | - | - | -1.1 -> -101.1 | 0.88 -> 1.00 |
| specx | 10 M | 4.5 -> 10.6 | 0.187 | 0.0% | - | - | - | -121.5 | 0.42 |
| sstv | 2 M | 64.9 -> 59.4 | 0.582 -> 0.507 | 0.0% | -29.5 -> -47.4 | -21.8 -> -53.3 | -39.2 -> -55.5 | -93.5 -> -106.3 | 0.24 -> 0.00 |
| tetra | 2 M | 16.8 -> 16.2 | 0.363 | 0.0% | -58.8 | -93.0 | -66.6 | -121.3 | 0.02 |
| tetrapol | 250 k | 657.4 -> 674.1 | 1.000 -> 0.350 | 99.7% -> 0.0% | -40.5 | -58.0 | -63.0 | -10.4 | 0.02 |
| thor | 2 M | 66.9 -> 59.5 | 0.541 -> 0.482 | 0.0% | -50.6 -> -49.5 | -22.5 -> -64.1 | -34.2 -> -67.2 | -93.0 -> -106.8 | 0.26 -> 0.00 |
| timesig | 256 k | 279.5 -> 285.4 | 0.300 | 0.0% | -39.2 -> -39.3 | -40.7 | -46.7 | -158.6 -> -158.8 | 0.85 |
| uat | 2 M | 6611 -> 132.4 | 0.080 | 0.0% | -27.3 -> -26.4 | -28.9 -> -34.1 | -36.4 -> -36.2 | -44.3 -> -41.4 | 0.50 -> 0.19 |
| uplink | 10 M | 63.5 -> 65.7 | 0.251 | 0.0% | - | - | - | -61.5 | 0.50 |
| vdes | 1 M | 454.7 -> 466.3 | 1.231 -> 0.800 | 0.1% -> 0.0% | -75.2 | - | -86.7 | -120.8 | 0.19 |
| vdl2 | 2 M | 306.4 -> 315.8 | 0.101 | 0.0% | -86.6 | -107.1 | -114.5 | -198.6 | 0.01 |
| vdl4 | 2 M | 94.1 -> 57.3 | 0.080 | 0.0% | -45.3 -> -94.8 | -49.2 -> -119.6 | -61.4 -> -124.8 | -85.4 -> -117.2 | 1.00 -> 0.01 |
| vorils | 2 M | 20.4 -> 18.3 | 0.340 | 0.0% | -78.7 | -79.3 | -94.2 | -127.9 | 0.06 |
| wifi | 20 M | 7.7 -> 6.9 | 0.250 | 0.0% | - | - | - | -63.2 -> -63.1 | 1.00 -> 0.15 |
| wisun | 20 M | 3.4 -> 4.8 | 0.300 | 0.0% | - | - | - | -139.8 | 0.01 |
| wmbus | 2 M | 161.8 -> 73.6 | 0.400 | 0.0% | - | - | - | -108.5 -> -122.5 | 1.00 -> 0.02 |
| wspr | 2 M | 65.7 -> 59.8 | 0.550 -> 0.483 | 0.0% | -52.3 -> -50.7 | -23.1 -> -63.9 | -34.0 -> -67.0 | -94.5 -> -107.6 | 0.25 -> 0.00 |
| zigbee | 4 M | 27.4 -> 25.5 | 0.300 | 0.0% | - | - | - | -87.2 | 0.71 |
| zwave | 2 M | 164.0 -> 77.2 | 0.300 | 0.0% | - | - | - | -136.5 -> -133.8 | 1.00 -> 0.02 |
