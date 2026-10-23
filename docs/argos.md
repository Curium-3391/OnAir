# Argos

Receive only. Mode "Argos" (Satellite and navigation), family 148, engine standard 150. Code: `core/src/argos/`, panel: `app/argos/`, tests: `tests/argos/`. Issue #256.

Argos collects data from small platform transmitters (PTTs) on buoys, profiling floats, animal tags and weather stations through low-orbit satellites (MetOp-B / -C, SARAL, the Kineis constellation and older NOAA satellites), and locates them from the Doppler shift of their bursts. The mode has two links, picked from the dial (above 430 MHz the downlink) or by hand in the panel.

## Frequencies

| Link | Frequency | Notes |
|---|---|---|
| PTT uplink (Argos 2 / 3) | 401.650 MHz nominal, 401.618 - 401.680 MHz | the bursts of PTTs near the receiver; a satellite is not needed to hear them |
| Satellite downlink | 465.9875 MHz | MetOp-B, MetOp-C (A-DCS), SARAL; the NOAA-19 downlink was switched off |

The pass planner (Tools > Satellite passes) lists MetOp-B and MetOp-C with the Argos downlink (NORAD 38771 and 43689, TLEs from the CelesTrak weather group) and can start this mode at AOS.

## What is decoded, and what is public

| Part | Status |
|---|---|
| PTT-A2 burst | 160 ms of unmodulated carrier, then 400 bit/s Manchester phase modulation of +-1.1 rad (decoded) |
| A2 message | 24 synchronisation bits, 4 bit length field, 28 bit platform ID, 24 + 32 k bits of user data (k = 0..7): 80 - 304 bits, 360 - 920 ms with the carrier. The ID and the data are shown (hex; the ID also as the older 20 bit form) |
| Sync bits, length field | shown as received. Their exact pattern and coding are in the CNES physical layer specification AS3-SP-516-2098-CNES, which is not public, so they are not checked; the message length is taken from the burst's duration |
| PTT-A3, ZE, Argos-4 (VLD / MD / HD) | QPSK / GMSK / spread waveforms with convolutional coding whose specifications (AS3-SP-516-274-CNES, A4-SS-TER-SP-0078/0079-CNES) are not public: a burst with a carrier but no A2 Manchester is listed as "other (not A2)" with its frequency, level and carrier length only |
| Doppler (uplink) | every A2 burst's carrier frequency is kept per platform; the slope is shown in Hz per minute (a fixed PTT shows its oscillator drift, a moving one its Doppler) |
| Downlink | 800 symbol/s Manchester BPSK = 400 bit/s, an HDLC bit stream with filler (public descriptions: SatNOGS transmitter entries, ESA). The carrier is followed through the pass; bits are deframed as NRZ, inverted NRZ and NRZI at once, and the coding that shows HDLC flags is reported. Frames are shown in hex with the standard HDLC FCS (CRC-16/X.25) checked. What the frames hold is not public and is not interpreted |
| Doppler (downlink) | the measured carrier against the curve the pass planner predicts for each MetOp above the horizon (needs TLEs and the receiver position); the two differ by the radio's frequency error |

Nothing is encrypted in these layers; nothing is transmitted.

## How the receiver works

The input is mixed to the dial offset and averaged to 128 kHz.

Uplink: the burst finder of the COSPAS-SARSAT mode (32 ms windowed FFTs, a narrow line 15 dB over the median bin that was not there before). The burst is mixed to 0 Hz and averaged to 32 kHz. Each bit is the angle between the vector sums of its two halves: +2.2 rad for a 1, -2.2 rad for a 0, whatever the carrier phase. The first bit is where the bare carrier ends and the +-2.2 rad steps begin; bit timing and rate (+-0.5 %) are searched first over 24 bits, then refined over the whole message. A message is accepted when the steps are close to +-2.2 rad on average (quality >= 0.5), the carrier lasted 120 - 210 ms and the bit count is one of the A2 lengths.

Downlink: the 128 kHz stream is averaged to 64 kHz. The carrier is found as the line of the squared signal within +-14 kHz (BPSK and PM both square to a line at twice the carrier), then followed by a second-order squaring loop of 60 Hz, which keeps up with a LEO pass (about 150 Hz/s at its fastest at 466 MHz). Every 0.1 s the residual carrier is removed and the data axis found; a bit is the difference of its two half-bit sums along it, timed by the alignment where those differences are largest.

## Test signal

`makeArgosSynth` (see `argos_gen.h`). Uplink: three made-up platforms (24, 56 and 248 data bits) at -12, +3 and +21 kHz from 401.650 MHz, a burst every 2 s; options for a drift during each burst, a frequency step from burst to burst, and an extra burst of another modulation (carrier then QPSK). The 24 synchronisation bits are a stand-in (`0x5A5A5A`) and the length field holds k: both are not the real (non-public) coding. Downlink: made-up HDLC frames between flags at 400 bit/s Manchester, +-1.1 rad (or BPSK), NRZ or NRZI, following a pass's Doppler curve.

Tests: `tests/argos/test_argos_proto.cpp` (A2 fields for every length, HDLC deframing with zero-bit insertion and the FCS, the CRC-16/X.25 check value), `tests/argos/test_argos_rx.cpp` (uplink clean, 16 dB in 2.4 kHz, +-4 / -6 kHz offsets, 40 Hz/s drift, 500 ksps, a QPSK burst kept apart, the drift estimate, noise only; downlink clean, 20 dB, BPSK, NRZI, a pass with +-9 kHz of Doppler followed within 3 Hz, noise only).

## Not verified

No real Argos recording was available: only the generated signals. In particular not verified against a real PTT: the A2 bit polarity convention (a 1 is taken as +1.1 rad then -1.1 rad, as in COSPAS-SARSAT), the 24 bit header layout and that all A2 messages are 80 + 32 k bits long. Not verified against a real satellite: the downlink's modulation index (the receiver handles +-1.1 rad and BPSK), NRZ or NRZI, that the HDLC FCS is the standard one, and the frame contents.
