# ROS (HF) detector: partial, needs a public spec

Mode list: **AMATEUR - DIGITAL > ROS (HF)**. Receive only.

ROS is a weak-signal HF chat mode by José Alberto Nieto Ros (EA5HVK). The program is proprietary and the framing, coding and FEC of the signal
are **not published**, so OnAir cannot read the text. What it does is the part that public sources describe (listed in [ROS.md](ROS.md)):

- It brings the sideband to audio and keeps a **waterfall** (0 - 3500 Hz above the dial).
- Every 2 seconds it measures the last 16 s (and, when that does not fit, the last 8 s, for a drifting radio) with the ROS identifier: one tone
  at a time, phase continuous, on a fine grid of about 144 tones 15.6 Hz apart, about 2.25 kHz wide.
- It names the **sub-mode** from the symbol rate: ROS 4, 8 or 16 baud (the current ROS-2K modes), with the measured tone range, grid spacing,
  span and a score. Few symbols (a short capture of 4 baud) are reported as "perhaps".
- The band of the tones is marked on the waterfall and in the log (UTC, sub-mode, baud, tones, score).

**Text decoding: partial, needs a public spec.** Nothing is read from the signal; the text field stays empty. If the author or anyone else publishes
the framing and coding, the decoder can be added on top of this mode. Look for an open decoder under a compatible licence was done (GitHub, sigidwiki):
none exists for ROS, so there is nothing that could be ported.

## Using it

1. Pick the band with the buttons (digital part of the band, upper sideband dial) or type a frequency; ROS has no calling frequency in this list.
2. The detection needs about 10 s of signal before the first result; 4 baud needs more.
3. The "Detection" view shows what was measured; "Log" lists every detection with its UTC time.

Advanced: the demodulator (upper or lower sideband) under the tuner.

## How it was checked

Only with its own test signal (the publicly reported physical layer: 144 tones, 15.625 Hz grid, 4 / 8 / 16 baud, random tone every symbol; it is **not** ROS
encoding): clean, tuning errors of +-150 Hz, a drift of 1 Hz/s (2 Hz/s smears the grid too much and is not detected), a symbol clock off by 300 ppm, noise down to 3 dB SNR
in 2.5 kHz, lower sideband; noise alone, a 2-tone FSK and a single carrier are not reported. No real ROS recording was available: if you hear
ROS, a short recording (IQ or audio) that you may share helps to tune the thresholds.

Tests: `ctest -R roshf` (`tests/roshf`), wiring in `tests/app/test_roshf_mode.cpp`.
