# DRM receiver audit: smooth audio (0.3.0)

Goal: sound with barely any cutouts. The whole chain was measured on synthetic signals first, the causes of the dropouts were found per stage,
then fixed and measured again. Everything here comes from the generator in core/src/drm/drm_gen.cpp; nothing has been checked on a real
off-air signal yet.

## How it is measured

`tests/drm/test_drm_smooth.cpp` (helpers in `tests/data/drm/testkit.h`). Each case runs the synthetic DRM transmitter (AAC test sound, no two
frames alike) through an impairment into `DrmReceiver` at 48 kHz (DRM+ at 192 kHz) in 50 ms chunks, and records:

* per stage: time to the first audio, locks lost ("signal lost" in the log), FAC / SDC / multiplex frames good and bad, audio frames good
  and bad, the reported SNR and Doppler spread, fades held, re-synchronisations;
* what a listener hears: the 48 kHz sound is played by a simulated sound device that behaves like `AudioOut` (starts once its start
  threshold is queued, restarts with 0.8 s after an underrun) and runs at 48000 frames per second of signal. Counted in 10 ms blocks from
  the first sound on: underrun blocks, silent blocks (below 3 % of the median level of the sound), and the sound the receiver made up
  (concealment, from the telemetry).
* **dropout** = a run of underrun or silent blocks (runs less than 100 ms apart are one). Reported as dropouts per minute and seconds of
  dropout per minute. **made up** = seconds per minute of concealed sound (repeated and faded) plus sound faded to silence.

Impairments: AWGN at a given SNR in the nominal bandwidth; the channels of Annex B.1 (2 Rice with delay, 3 US consortium, 4 CCIR poor,
5 and 6); flat Rayleigh fading (sum of 16 sinusoids, maximum Doppler 0.05 to 2 Hz); holes (the signal drops by 40 dB for 0.1 to 5 s every
15 s); 2 s of zeros; 40 ms of samples lost; impulse noise (1 ms bursts every 0.7 s, 14 dB above the signal); carrier and clock offsets;
an abrupt reconfiguration (64-QAM to 16-QAM at 30 s, two transmitters in step); DRM+ with AWGN and holes.

`ONAIR_DRM_AUDIT=1 test_drm_smooth` prints the whole survey (45 cases, about 4 minutes on one core); `ONAIR_DRM_CASE=<text>` runs the
cases whose name contains the text; without either it runs the regression set below. `ONAIR_DRM_OLD_DEVICE=1` sets the simulated device up
as the receiver did before (start at 250 ms), for the "before" numbers. `tests/drm/test_drm_playout.cpp` tests the playout stage on its own
(a tone, lost frames, missing super frames, 5 s outages, sound card clocks -300 to +300 ppm off, 10 minutes each).

## What was found

1. **Synchronisation gave up in every fade.** The tracker restarted the search after 6 symbols (160 ms in mode B) without guard interval
   correlation. Every fade longer than about 0.2 s threw away the lock, the frame sync, the FAC and the time interleaver: 4 to 5 s of
   silence per fade, often several restarts per fade (17 to 35 "signal lost" in 90 s with a hole every 15 s).
2. **The loops ran on noise during fades.** Frequency loop, timing (window position and delay centroid) and the clock offset estimate kept
   updating from noise; the clock corrector could be steered by up to 400 ppm by one fade. In multipath fading the clock offset estimate
   also followed the wandering centroid of the delay: -584, +178 and +670 ppm reported (and corrected) on channels 4, 5 and 6 at 32 dB
   (test_drm_impair, no clock offset sent).
3. **The Doppler estimate was wrong.** It divided the pilot correlation by (power - noise), but the noise estimate (second difference of
   the pilots in time) also holds the change of a fading channel: with 2 Hz of Doppler spread it read 0 Hz. (The SNR display under-reads in
   fast fading for the same reason; that is left as it is, see below.)
4. **The channel estimate used only linear interpolation in time** between two pilots (one pilot per carrier every gy symbols), so the
   noise of single pilots went straight into the equalised cells.
5. **Audio came in bursts of 1.2 s.** The back end decoded the SDC and the three multiplex frames only at the end of a transmission super
   frame, and the sound device started at 250 ms: right before every burst the device queue was close to empty (one 0.4 s underrun at the
   start of every run, a near underrun every 1.2 s after it; any scheduling jitter turns that into a cutout).
6. **Lost audio frames became silence.** AAC: the last good frame faded out over 40 ms, then silence; the next good frame faded in.
   xHE-AAC: a lost frame produced no samples at all, so the sound ran short and the device underran (0.8 s of silence each time). The
   device queue was flushed at 1.5 s (a hard cut) and there was nothing against the clock difference between the radio and the sound card
   (at -50 ppm an underrun about every 1.4 hours).
7. **Decoding noise.** In a fade the FAC (8 bit CRC) and the audio frames (8 bit CRC) were decoded from noise; one FAC block of noise in
   256 passes its CRC, and a changed configuration from such a block reset the multiplex. Any reconfiguration dropped the interleaver even
   when only the SDC mode or the number of services changed.
8. Not a problem: `feed()` runs on the receiver thread and never blocks, telemetry is copied under a mutex four times a second, AudioOut is
   a lock-free ring; the AFC range (+-12 kHz tested) and clock range (+-300 ppm) were already wide; the channel filter and the occupancy
   from the FAC were right. Impulse noise at the tested level does not cost a frame (before and after).

## What was changed

* **Hold through fades** (`drm_rx.cpp`). The SNR of the pilots of the newest symbol against the noise (smoothed over about three symbols)
  marks a fade below -1 dB (out above +1.5 dB). In a fade the frequency loop, the timing loops and the clock offset measurement hold; the
  noise and Doppler averages are not fed with pilots in or next to the fade (they resume after 2 s, so that a level that changed for good is
  learnt). The lock is no longer given up on the guard correlation once the FAC has been decoded. The FAC decides: 12 frames (4.8 s) of
  FAC failure with the signal there, or 60 frames (24 s) in a fade, restart the search. Frames keep flowing through the fade, so the time
  interleaver bridges short fades and the signal decodes again the moment it comes back.
* **Clock offset from consistent windows only.** A measuring window (130 symbols) is taken when its offset agrees with the window before
  within 15 ppm (or its drift is below 15 ppm); the offset shown is that of the windows taken. Channels 4, 5 and 6 at 32 dB now read
  0.0 ppm; real offsets of +-30 to +-300 ppm are found within 2 ppm as before.
* **Fast re-synchronisation.** While the FAC fails, the acquisition runs in the background every 0.4 s; a signal at another symbol timing
  (more than G/2 or a fifth of a symbol off) or frequency, found twice in a row, is taken at once (samples lost by the radio). While the
  FAC decodes, the same search now and then measures where it puts the symbol start relative to the tracker, the reference for that test.
* **Doppler spread from the pilot correlations at gy and 2 gy symbols** (their ratio is x^3 with a Gaussian Doppler spectrum; the noise is
  in neither): 2.0 to 2.4 Hz on the 2 Hz channels, 1.5 to 1.7 Hz on channel 6.
* **Wiener filter in time** over the five pilots of a carrier nearest in time (t - d - 2 gy .. t - d + 2 gy), fitted to the measured
  Doppler spread (taken 1.5 times wider plus 0.2 Hz, at least 0.3 Hz) and noise, weights scaled to a sum of 1 (no shrinking of the
  constellation). Used below 1 Hz of Doppler spread; above that the linear interpolation is kept (there the noise estimate holds the
  channel's change and the filter would smooth too much). The equaliser delay grows from gy to 2 gy symbols. The cell weights include the
  noise the filter leaves in the estimate.
* **Back end** (`drm_dec.cpp`): the SDC is decoded at the first frame of the super frame and every multiplex frame as soon as its cells are
  in (audio every 400 ms, at worst two frames together, instead of three every 1.2 s). Blocks whose mean cell SNR is below 3 % of that of
  the blocks that decode (and below 2) are not decoded (a fade). A changed configuration or an out-of-sequence frame identity is taken when
  the next FAC block confirms it, or at once when the FAC announced a reconfiguration; only a change of occupancy, interleaver or
  constellation drops the multiplex.
* **Playout** (`drm_playout.h`, new): a queue of one super frame plus 250 ms, emptied at exactly 48000 frames per second of signal time;
  lost frames (the audio decoder now reports them as placeholders, `AudioDecoder::superFrame(..., gaps)`) and an empty queue are concealed:
  the pitch period of the last sound repeated with overlap-add at the joins (a 5 ms hold-back makes the join into the concealment smooth),
  full level for 20 ms, faded out over 200 ms; real sound comes back through a 20 ms crossfade. After a loss the next super frame is placed so
  that the queue is at its nominal level again (the latency does not drift). A 16-tap windowed sinc resampler keeps the sound device at 200 ms
  (P control, at most 0.4 %): measured -300 to +300 ppm with no underrun in 10 minutes each, the device queue between 0.18 and 0.21 s. The
  device starts at 150 ms; the 1.5 s flush is kept only as a safety net.
* **Telemetry and UI** (`drm_tel.h`, `app/drm/drm_ui.cpp`): holding (fade), fades held, re-synchronisations, concealed share of the last
  10 s, made-up seconds (concealed / faded to silence), concealments, playout queue, sound card drift, device underruns. Status bar: Concealed %
  and "fade, holding"; Now playing: a line while it holds or when more than 0.5 % was concealed; meter bank: CONCEALED %; Receiver tab:
  the full numbers.

## Before and after

Mode B 10 kHz unless named, long interleaver, 60 s (fades and holes 90 s). Before: the receiver of pub/main at the start of the audit, its
sound device as it was set up (start at 250 ms). "lost": searches restarted. Dropouts are silence or underrun; made up is concealed sound
(after only; before, a lost frame was silence and counts as dropout).

| case | before: lost, bad frames, dropouts/min, dropout s/min | after: lost, bad frames, dropouts/min, dropout s/min, made up s/min |
|---|---|---|
| 64-QAM AWGN 15 dB | 0, 100 %, -, - (no audio) | 0, 51 %, 44.0, 18.8, 31.2 |
| 64-QAM AWGN 16 dB | 0, 0.1 %, 1.07, 0.44 | 0, 0.1 %, 0, 0, 0.09 |
| 64-QAM AWGN 18 / 22 dB | 0, 0 %, 1.07, 0.44 | 0, 0 %, 0, 0, 0 |
| 16-QAM AWGN 8 dB | 0, 74.8 %, (mostly silent) | 0, 11.4 %, 6.6, 1.85, 6.8 |
| 16-QAM AWGN 9 dB | 0, 4.3 %, 5.33, 1.70 | 0, 1.2 %, 1.08, 0.22, 0.73 |
| 16-QAM AWGN 10 dB | 0, 0.9 %, 2.13, 0.86 | 0, 0.1 %, 0, 0, 0.04 |
| A, C, D 16-QAM, A 4.5 kHz, B 20 kHz, AWGN | 0, 0 %, 1.07, 0.44 | 0, 0 %, 0, 0, 0 |
| channels 2, 3, 4, 6 at 20 and 26 dB | 0, 0 %, 1.07, 0.44 | 0, 0 %, 0, 0, 0 |
| channel 5 20 dB | 3, 18.6 %, 3.2, 19.2 | 0, 0.5 %, 0, 0, 0.30 |
| channel 5 26 dB | 1, 18.2 %, 6.4, 12.6 | 0, 0 %, 0, 0, 0 |
| Rayleigh 0.05 / 0.2 / 0.5 / 2 Hz, 22 dB | 0, 0-0.3 %, 0.7-1.4, 0.3 | 0, 0 %, 0, 0, 0-0.03 |
| hole 0.1 s / 15 s | 0, 0 %, 0.70, 0.29 | 0, 0 %, 0, 0, 0 |
| hole 0.3 s / 15 s | 17, 0.1 %, 6.95, 8.84 | 0, 0.1 %, 0, 0, 0.06 |
| hole 0.8 s / 15 s | 33, -, 8.34, 22.5 | 0, 8.3 %, 15.4, 1.21, 4.84 |
| hole 2 s / 15 s | 32, -, 8.34, 20.0 | 0, 16.5 %, 12.6, 6.63, 9.68 |
| hole 5 s / 15 s | 35, -, 8.34, 34.1 | 0, 18.3 %, 11.2, 17.3, 20.2 |
| 2 s of zeros at 20 s | 0, 4.4 %, 4.26, 2.84 | 0, 4.4 %, 4.30, 1.81, 2.67 |
| 40 ms of samples lost at 20 s | 1, 4.2 %, 4.26, 5.72 | 0 (1 re-sync), 0.5 %, 1.08, 2.57, 2.79 |
| reconfiguration 64 -> 16-QAM at 30 s | 0, 0 %, 3.20, 2.14 | 0, 0 %, 1.08, 2.74, 2.96 |
| CFO +1 kHz with +60 ppm, -150 ppm, impulse noise | 0, 0 %, 1.07, 0.44 | 0, 0 %, 0, 0, 0 |
| DRM+ 16-QAM AWGN 12 dB | 0, 0 %, 0, 0 | 0, 0 %, 0, 0, 0 |
| DRM+ hole 0.5 s / 10 s | 3, 9.5 %, 4.65, 12.2 | 0, 4.8 %, 4.65, 1.21, 2.91 |

The 1.07 dropouts / 0.44 s per minute before in the clean cases is the start-up underrun of the 1.2 s bursts (once per run; in real use
scheduling jitter makes it recur). The thresholds moved by about 1 dB (Wiener filter): 16-QAM at 9 dB loses 1.2 % of the audio frames
instead of 4.3 %, 64-QAM at 15 dB 51 % instead of 100 %. Holes up to the interleaver's reach (0.3 s) now cost nothing; longer ones cost
the hole plus the interleaver's tail, concealed and without a single restart. With dropouts per minute up but seconds down (0.8 s holes),
the longer losses are now split by stretches of good audio instead of one long silence after a restart.

Playout on its own (`test_drm_playout`): sound card -300, -100, 0, +100, +300 ppm off the radio for 10 minutes each: no underrun, the
resampler at the offset (within 0.1 ppm), the device queue 0.18 to 0.21 s, no step in the sound larger than the tone's own. 17 missing
super frames and 54 lost frames in 120 s: exactly the 8.96 s lost made up, no underrun, the largest step at a join 0.029 (the 440 Hz tone at
half scale has 0.029 between samples). A 5 s outage: 0.22 s concealed, 4.58 s faded to silence, the device kept playing.

## Regression tests

`drm_smooth` (labelled slow, the full CI) runs 24 cases of the survey and checks, per case, the time to the first audio (under 6.5 s), the
made-up and dropout seconds per minute, no device underrun, no lost lock, the re-synchronisation after the 40 ms loss, and the clock offset
reported on channels 5 and 6 (0 ppm) and with -150 ppm sent (within 10 ppm). Limits (made up,
dropout s/min): AWGN 64-QAM 18 dB 0.1 / 0.05, 16 dB 0.5 / 0.3; 16-QAM 10 dB 0.3 / 0.2, 9 dB 2.0 / 1.0; C and D 0.1 / 0.05; channels 3, 4,
5 (26 dB), 6 and D on 6: 0.3 / 0.2; channel 5 20 dB 2.0 / 1.0; Rayleigh 0.2 Hz 0.3 / 0.2, 2 Hz 1.0 / 0.5; holes 0.3 s 0.5 / 0.3, 2 s
14 / 10, 5 s 26 / 24; 2 s zeros 4 / 3; 40 ms lost 4.5 / 4; reconfiguration 4.5 / 4; -150 ppm 0.1 / 0.05; impulse noise 0.3 / 0.2; DRM+ AWGN
0.1 / 0.05, DRM+ holes 4.5 / 2. `drm_playout` (quick) checks the playout alone. The other DRM tests (rx, impair, modee, chain, known, data,
mot, realworld, xhe_lib, proto_rs) pass unchanged.

## Known limits

* Holes longer than about 0.5 s are beyond what the 2 s interleaver can bridge (two of its five frames gone): the audio of the hole and
  about 0.4 s after it is lost and concealed (faded to silence after 220 ms).
* A reconfiguration without the announcement in the FAC costs about 2.7 s (new SDC, then the interleaver fills); the announced one takes
  the new FAC at once but still waits for the SDC and the interleaver. Using the next configuration that the SDC sends ahead is not done.
* The SNR shown under-reads in fast fading (the noise estimate holds the channel's change); the weights of the cells use the same estimate,
  which is on the safe side. The Wiener filter is off above 1 Hz of Doppler spread for that reason.
* xHE-AAC concealment: lost frames are placeholders of the last frame's length; a super frame that cannot be read is replaced by 400 ms,
  and the queue evens out the difference. There is no xHE-AAC generator, so this is only covered by the playout test, not end to end.
* No impulse blanker: the tested impulse noise costs nothing; stronger impulses were not studied.
* The engine test (`drm_engine`) and the app were not run locally (the local library was built from older headers); CI runs them.
