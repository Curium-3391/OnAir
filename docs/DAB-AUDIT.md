# DAB / DAB+ receiver audit: audio continuity (0.3.0)

Goal: smooth audio with barely any cutouts. The whole chain was measured on synthetic signals, the causes of dropouts were fixed, and the
measurement is now a regression test with ceilings per condition.

## Method

`tests/dab/test_dab_audit.cpp` (ctest `dab_audit`, label slow, about a minute on four cores):

- The generator (`dab_gen.cpp`, written from the standard, no code shared with the receiver) makes the clean ensemble: four services, DAB+
  48 kbit/s EEP-3A on sub-channel 1 (the one played), MP2 128 kbit/s on sub-channel 4.
- A channel model in the test (independent of the receiver) adds: white noise, static single frequency network echoes (inside and beyond the
  guard interval, a weaker transmitter heard first), three-path Rayleigh fading with Doppler (sum of sinusoids), flat slow Rayleigh fading,
  deep fades (signal gone), impulsive noise bursts, lost USB blocks (samples removed), an 8-bit RTL-SDR (quantisation, clipping) with tuning
  error and frequency drift, and sample clock error (the generator's resampler).
- The real `DabReceiver` decodes it; its audio is played by a model of the sound card: AudioOut's rules (playback starts at the start level,
  after an underrun again at 0.8 s), consuming in 10 ms blocks on its own clock.
- Measured where the listener is: a 10 ms block below -50 dBFS (or not there) is silent. **silence/min** = seconds of silent blocks per minute
  after the first sound, **cuts/min** = runs of silent blocks per minute. Next to them the per-stage counts: FIB CRC pass rate, access units
  lost (a superframe that did not decode counts all of its units), sync losses, superframes bad, Reed-Solomon bytes fixed and code words beyond
  repair, frames concealed, underruns.
- 20 s of signal per condition (`DAB_AUDIT_SECONDS` changes it), fixed seeds.
- `tests/dab/test_dab_playout.cpp` (fast) checks the playout alone: no click at any splice (largest second difference equal to the clean
  tone's), a short loss bridged at full level, a long one faded to silence, and the output buffer level against a sound card at -1000 to
  +1000 ppm over ten minutes.
- Service switching (in the audit): two switches while locked, the new service's first audio within 1 s, no sync loss.

## Results (20 s each; before = this branch's start, 1ca0cc98b + fix-dab-030)

| condition | before: AU lost / silence / cuts per min | after: AU lost / silence / cuts per min | ceiling (cuts, s) |
|---|---|---|---|
| clean, 20 dB | 0 % / 0 s / 0 | 0 % / 0 s / 0 | 0, 0 |
| AWGN 6, 5, 4.5, 4 dB | 0 % / 0 s / 0 | 0 % / 0 s / 0 | 0-3.2, 0-0.5 |
| **AWGN 4.5 dB, sample clock +100 ppm** | **no audio at all** (650 RS code words failed) | 0 % / 0 s / 0 | 3.2, 0.5 |
| **AWGN 4.5 dB, sample clock -100 ppm** | **no audio at all** | 0 % / 0 s / 0 | 3.2, 0.5 |
| AWGN 20 dB, +100 ppm | 0 / 0 / 0 | 0 / 0 / 0 | 0, 0 |
| **SFN echo 0 dB at 195 us (inside the guard)** | 12 superframes bad, 1319 RS fixes / 5.2 s / 6.2 | 0 / 0 s / 0 (0 RS fixes) | 0, 0 |
| SFN pre-echo -3 dB, main path 146 us later, 12 dB | 0 / 0 / 0 (30 RS fixes) | 0 / 0 / 0 (0 RS fixes) | 0, 0 |
| SFN echo -3 dB at 342 us (beyond the guard) | 0 / 0 / 0 | 0 / 0 / 0 | 0, 0 |
| mobile 3 paths, Doppler 20 Hz, 10 dB | 0 / 0 / 0 (85 RS fixes) | 0 / 0 / 0 (15 RS fixes) | 3.2, 0.5 |
| mobile 3 paths, Doppler 60 Hz, 18 dB | 0 / 0 / 0 (89) | 0 / 0 / 0 (6) | 3.2, 0.5 |
| mobile 3 paths, Doppler 100 Hz, 25 dB | 30 SF bad / **24.5 s** / 15.6 | 33.8 % / **15.1 s** / 22.5 | 30, 20 |
| flat Rayleigh 2 Hz, 12 dB | 12 SF bad / 5.2 s / 3.1 | 4.0 % / 1.6 s / 6.3 | 10, 3 |
| deep fade 250 ms every 4 s | 13 SF bad / 6.3 s / 6.2 | 6.3 % / 2.2 s / 12.6 | 16, 3.5 |
| deep fade 100 ms every 2 s | 3 SF bad / 3.0 s / 3.1 | 0 / 0 / 0 | 3.2, 0.5 |
| signal gone 1.5 s once (tunnel) | 7.6 s / 3.1 | 4.7 s / 3.1 (1.57 s heard silent for 1.5 s gone) | 3.2, 6 |
| USB block of 8192 lost every 2 s | 0 / 0 / 0 | 0 / 0 / 0 | 0, 0 |
| **USB block of 131072 lost every 3 s** | 3 sync losses / 8.8 s / 9.4 | 0 sync losses / 0.5 s / 6.3 | 10, 1.5 |
| **impulsive noise 100 us +15 dB, 100 Hz** | FIB 73.6 %, 22 SF bad / 8.8 s / 9.4 | FIB 100 % / 0 / 0 | 0, 0 |
| RTL-SDR 8 bit, +25 kHz, 80 Hz/s drift, 60 ppm, 12 dB | 0 / 0 / 0 | 0 / 0 / 0 | 0, 0 |
| MP2, Doppler 20 Hz, 10 dB | 0 / 0 / 0 | 0 / 0 / 0 | 3.2, 0.5 |
| MP2, deep fade 250 ms every 4 s | 5.5 s / 6.2 | 0.7 s / 6.2 | 10, 1.5 |

The "before" build fails 11 of the 24 ceilings. Cuts per minute rose in three conditions where the loss itself cannot be avoided (the 250 ms
fade wipes out about 13 logical frames through the 16 CIF time interleaving, i.e. four superframes): one long silence became shorter ones with
concealed audio in between; the silence per minute fell by two thirds or more in every one of them.

Playout (`test_dab_playout`): one or two lost access units bridged at full level, eight (320 ms) faded to silence and back, largest step 0.0086
(the clean 1 kHz tone's own); sound card at -1000 / -100 / 0 / +100 / +1000 ppm for ten minutes: the buffer stays at 17 000 - 26 000 frames
(target 19 200), no underrun (without the rate control +-1000 ppm drifts 600 ms in that time and runs dry or overflows).

## What was wrong and what changed

Receiver (`core/src/dab/dab_rx.cpp`):

1. **Sample clock error was not followed within a frame.** The windows were placed at U + l * 2552 and the fraction of a sample drifting
   between two symbols (0.26 samples at 100 ppm) turned the differential phase by up to 34 degrees at the band edge: 1.5-2 dB lost, at
   4.5 dB the whole MSC failed while the FIC (first symbols) still passed. Now the clock error is measured from how far each frame moves
   against the prediction (`sro`, shown in the app as "Clock ... ppm"), the windows follow the symbols and the left-over fraction is turned
   out per carrier; the next frame is predicted kFrame * (1 + sro) later.
2. **The FFT window followed the strongest path.** With two paths of nearly equal strength (a single frequency network) it jumped between
   them, and with a weaker transmitter heard first the earlier path's next symbol ran into the window. Now it goes on the earliest path
   within 10 dB of the strongest when all fit in the guard interval, and where the most path energy fits when they do not (`placeWindow`).
3. **Soft bits were normalised per frame**, so a faded frame (or one of pure noise) gave soft bits as confident as a good one and poisoned
   the 16 CIFs of time interleaving. Now they are scaled by a slow average over good frames: a fade gives weak soft bits, nearly erasures.
4. **A fade was taken for a lost sync.** Four frames without the phase reference ended the lock; the carrier offset was re-estimated from the
   cyclic prefixes of noise meanwhile. Now a frame whose phase reference symbol is far weaker than the good frames is a fade: timing and
   carrier offset are held and the frame is decoded with its weak soft bits, for up to 12 frames (1.15 s).
5. **Lost samples (a USB drop) cost the sync for 4 frames, then a full search, and the de-interleaver was out of step.** Now a frame of
   normal strength without the phase reference triggers a quick re-acquisition at once (only +-2 carriers around the known offset, half a
   frame of samples kept for it, decoding state kept); the CIFs missed go into the de-interleaver as erasures, counted from the CIF counter
   of FIG 0/0 (or from the samples when the FIC failed), so the logical frames around the gap still decode and the DAB+ superframe phase is
   kept. Over 20 CIFs lost, the decoding starts over.
6. **Impulsive noise** (ignition, switching) went straight into the FFT. A blanker zeroes samples more than 12 dB over the running mean power
   (OFDM itself goes there about once in 10^7 samples).

Audio (`core/src/dab/dab_audio.cpp`, new `dab_playout.{h,cpp}`):

7. **A Reed-Solomon code word beyond repair dropped the whole superframe** (120 ms, 2-6 access units). Its bytes are spread over the
   superframe (one in every N), so the access units whose CRC still passes are now used.
8. **Lost audio was just missing**: nothing was put out for a bad access unit or superframe, the buffer drained, and AudioOut re-buffered for
   0.8 s after every underrun. Now every lost unit is concealed in time: the last 40 ms repeated (crossfaded at every splice, the samples
   held back by 2 ms for it) at full level for 40 ms, then faded to silence over 120 ms; audio coming back is crossfaded in (faded in over
   10 ms after silence). After a loss that went silent one good unit alone is not played (no blips between silences).
9. **No gap filling**: during a sync loss the output ran dry. When less than 100 ms is queued and nothing arrives, the gap is filled, and the
   level is restored before the audio comes back, so the output never re-buffers.
10. **No rate control**: the sound card's clock and the transmitter's differ by up to 100 ppm (360 ms an hour). The playout resamples by up to
    0.2 % (cubic) to keep 400 ms queued.
11. **Start level 250 ms** was barely above the spacing of the audio bursts (120 ms superframes arriving in 96 ms frames, plus USB block
    jitter). Now 400 ms; after a service switch AudioOut's threshold is set back to it (it waited 0.8 s after every flush).
12. **The superframe phase was searched again after 3 bad superframes**: a fade of a few hundred ms then cost another second. Now after 8,
    and the receiver tells the audio when the phase really changes (reconfiguration, a long gap).
13. MP2 frames lost or erased are concealed the same way (24 ms each).

Status in the app (DAB panel): "Audio x % concealed" with a tooltip (Reed-Solomon fixed / beyond repair, access units ok / concealed, seconds
silent, sync losses and quick re-acquisitions, CIFs erased, impulse samples blanked, output queued and rate correction) and "Clock" (the
measured sample clock error in ppm). The lost-sample counter was already shown ("dropped").

## Checked and left as they were

- Transmission modes II-IV: not implemented. They were removed from EN 300 401 in V2.1.1 (2017) and no ensemble uses them; DAB has only
  DQPSK (no QPSK mode to audit).
- FIC / MSC coding: depuncturing of every EEP and all 64 UEP profiles, energy dispersal and the Viterbi decoder round-trip against the
  generator's own encoder (`test_dab_realworld`, `test_dab_gen`).
- Reed-Solomon erasure decoding: there are no erasure marks before RS in DAB+ (the CRCs are per access unit, after RS), and a lost logical
  frame puts 24 bytes into every code word, more than the 10 erasures RS(120,110) can fill. Not worth it.
- Coarse/fine frequency: +-75 kHz acquisition and an 80 Hz/s drift pass; the offset is now held through fades.
- Doppler 100 Hz (about 480 km/h in Band III): beyond mode I (carrier spacing 1 kHz, symbol 1.246 ms); the loss there is physical.
- Decoder threading and RTL-SDR USB buffers: the DAB receiver runs on the engine's decode thread; the source's lost-sample counter is shown
  in the panel. Lost blocks are now survived (above), the buffer sizes were not changed (they are shared by every mode).
- PPM: the measured clock error is shown; it is not applied to the device automatically (the correction is per device and shared by every
  mode).
- Band III scan with several ensembles: `test_dab_search` passes unchanged; the scan resets the receiver per channel, so the quick
  re-acquisition (which keeps state) never carries over to another channel.

Not verified on a real radio.
