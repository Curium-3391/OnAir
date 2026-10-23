# KAAL next (frame version 4): four voice slots, fast chirp sync, better receiver

Branch kaal-phy-030, started 2026-10-22. Section 1 is the state before this work (read from the code, not from the old specs). Sections
2 onwards are the new design and its measured numbers.

## 0. Bandwidth: what "the same bandwidth as now" is

The old scratchpad KAAL-SPEC-v0.1.md / KAAL-BRIEF.md describe a 500 kHz UHF video system. That idea was dropped before any code; the KAAL
in OnAir is a voice mode: **about 10 kHz wide at 8 ksym/s** (12 / 16 kbit/s voice) and **about 20 kHz wide at 16 ksym/s** (24 / 32 kbit/s,
in 0.2.8). "The same bandwidth as now" is therefore taken as the 20 kHz channel of the 16 ksym/s profile (RRC roll-off 0.25: 16 x 1.25 =
20 kHz). Version 4 uses exactly that channel. Nothing here needs 500 kHz; the same frame scaled to 500 kHz would be 400 ksym/s (25x the
numbers below) if that is ever wanted.

## 1. Current KAAL (v3, 0.2.8/0.2.9), as built

Sample rate 48 kHz (the 16 ksym/s profile is the same stream played at 96 kHz and read as 48 kHz by the live receiver).

- **Voice frame (v3):** 160 symbols = 20 ms at 8 ksym/s: 32-symbol known preamble + 120 16-QAM data symbols + 8 pilots (every 16th
  symbol, pilot (3+3j)/sqrt(10)). 480 coded bits per frame. One frame per 20 ms codec frame at 12 / 16 kbit/s; two frames per codec frame
  at 24 / 32 kbit/s, sent at 16 ksym/s. Pulse shaping RRC 0.25, 6 samples per symbol.
- **FEC:** one QC-LDPC (640,320), Z = 32, 10 x 20 base matrix, dual-diagonal parity, rate-matched to 480 sent bits (shortened to k = 240 or
  320 information bits, the parity tail dropped): rate 1/2 (12 kbit/s) or 2/3 (16 kbit/s). Normalised min-sum, alpha 0.8, 40 iterations,
  flooding schedule. Pseudo-random interleaver over 4 words (1920 bits = 80 ms).
- **Sweep (the "chirp"):** a slow linear chirp -5 .. +5 kHz over 24240 samples (0.505 s) added **on top of** the data at -10 dB, all the
  time. Four variants per period (up, down, fast1, fast2) carry the station ID (2 bits per period, 44-bit ID = 24 periods = 12.12 s).
  The receiver estimates and subtracts it every period. It costs no air time, but it takes 0.41 dB of the transmit power (1/11 of it) and
  leaves a residual after cancellation; its main use is the weak-signal ID (readable down to about -22 dB per 48 kHz sample after 12-36 s).
- **Beacon / text:** a separate narrow slice, BPSK 1000 sym/s, RRC 0.35, at +4.5 kHz (inside the voice band, also at -10 dB on top of the
  data), K = 7 rate 1/4 convolutional code, 64-symbol sync word. Header-only frame (170 bits) every 2 s, full frame with up to 100
  characters of text every 10 s. **Text throughput: 100 characters per 10 s = 80 bit/s.**
- **Sync and acquisition:** 3 s (or 1.2 s fast) buffer: sweep search (joint offset/CFO fold search), symbol clock from the energy of the 6
  sample phases, frame sync by correlating the differential preamble, CFO from the sweep position; the live receiver first moves the
  centre of the spectrum to 0 Hz (coarse carrier search over several kHz) and decides 8 vs 16 ksym/s from the spectrum width. Then frame
  by frame: per-frame complex gain fit on preamble + pilots, noise estimate, max-log 16-QAM demap, deinterleave, LDPC; block alignment
  (which of the 4 interleaver offsets) from how many words decode. Lock: 1.25 s (fast) to about 3.5 s.
- **Demod chain:** one-shot: no iteration between the decoder and the channel estimate, no decision-directed tracking inside a frame
  (pilots only, linear phase fit per frame), no equaliser.

Why text/data is slow now: the voice frame has no room for anything but one codec stream, and all other information goes through the
1 kbaud rate-1/4 beacon slice (250 bit/s raw, 80 bit/s of text on the 2 s / 10 s schedule). The slow sweep is not what limits the data
rate (it is superimposed, not time-multiplexed); what limits it is that sync, ID and text are carried by low-rate side signals designed for
weak-signal ID instead of by the main 16 ksym/s carrier.

## 2. Version 4 design

### 2.1 Choice of the multiplex (4 voice channels in 20 kHz)

Raw capacity of the channel: 16 ksym/s x 4 bits (16-QAM) = 64 kbit/s coded. Three ways to split it into 4 channels:

| | TDMA (chosen) | FDMA, 4 x 5 kHz | CDMA, 4 codes |
|---|---|---|---|
| per-channel symbol rate | 16 ksym/s in a 9 ms slot | 4 ksym/s continuous | 16 kchip/s, SF 4 |
| guard / overhead | one shared sync per 40 ms (5 %) | 4 syncs, guard bands between sub-channels (about 10-20 % of the band) | one sync, but codes lose orthogonality with any timing error or multipath |
| PAPR with 4 active (16-QAM, RRC 0.25) | single carrier, same as 1 active (measured 2.5 below) | sum of 4 carriers: +2..3 dB | sum of 4 codes: Gaussian-like, worst |
| unused channel capacity | the slot simply carries data instead | a whole sub-channel of data, separate modem | another code of data |
| CFO / Doppler tolerance | relative to 16 ksym/s: 4x better than FDMA | relative to 4 ksym/s | as TDMA, but needs chip timing |
| one receiver for all | yes, one sync, one CFO, one clock | 4 receivers (or a channeliser) | 4 despreaders |
| different transmitters per channel | needs a timing master (not in v4.0) | free (no time sync needed) | needs power control (near-far) |

TDMA wins for one transmitter carrying up to four talkers (net/repeater/multi-operator station), which is what "4 voice slots in the same
signal" means; it reuses the whole existing single-carrier receiver, keeps the PAPR of one carrier, and an idle slot turns into data
with no change to the waveform. The cost is that four separate stations cannot share the channel without a common time reference: v4.0
is a single-transmitter multiplex. (A later v4.x could add a 4-symbol guard per slot and slot-only bursts timed from the master's sync.)

### 2.2 Frame

16 ksym/s, RRC 0.25, 3 samples per symbol at 48 kHz, occupied bandwidth 20 kHz. Superframe 40 ms = 640 symbols:

| symbols | content |
|---|---|
| 0..15 | up-chirp (16-symbol discrete chirp, sweeps -8 .. +8 kHz in 1 ms) |
| 16..31 | down-chirp (the conjugate); its sign is flipped (x -1) in superframe 0 of every 10 (control-word marker) |
| 32..63 | control channel: 32 QPSK symbols = 64 coded bits; 10 superframes = one (640,320) LDPC word (rate 1/2) |
| 64..207 | slot 0: 144 symbols = 8 pilots (positions 0, 18, .., 126) + 136 data symbols |
| 208..351 | slot 1 |
| 352..495 | slot 2 |
| 496..639 | slot 3 |

Slot word: the same (640,320) mother code, shortened to k information bits and rate-matched by a circular buffer to 544 coded bits
(16-QAM) or 272 (QPSK): k information bits, then the parity bits from the start, repeated from the start when more are needed. Profiles and
budgets: docs/KAAL-SLOTS.md.

Control word (every 400 ms, 320 bits, rate 1/2 QPSK, the most robust part of the frame): version byte (4), slot map (4 x 4-bit profile),
message type and part counters, 33 bytes of message (beacon fields, a slot's talker callsign, or a text chunk), CRC-16. 660 bit/s of side
information that is always there, even with all four voice slots busy.

### 2.3 The sweep, made fast

The 0.5 s sweep is replaced by a 1 ms up-chirp and a 1 ms down-chirp at the start of every 40 ms superframe (a 250x faster sweep that
is part of the symbol stream, at full power, not superimposed). It still draws the KAAL diagonal on the waterfall (now a short "<" pair every
40 ms). What it gives: frame timing and the carrier offset in one step (a carrier offset f moves the up-chirp correlation peak by +f/1000
symbols and the down-chirp by -f/1000: the difference is the offset, the mean is the timing, no frequency search), the control-word
marker, and a sync every 40 ms instead of every 0.5 s, so the receiver locks in one or two superframes.

The station ID and the beacon move into the control channel (and into data slots when they are free). The superimposed slow sweep and the
beacon slice are gone from v4: that is the 0.41 + 0.41 dB of power they took, and their residual interference, returned to the data.
Cost: the very-weak-signal ID of the slow sweep (-22 dB per sample after 12-36 s) is replaced by the control channel's ID, which needs more
signal (measured in section 6); v3 signals keep the old sweep ID unchanged.

### 2.4 Version and compatibility

v4 is a new frame, recognised by its sync (chirp pair) and the version byte in the control word. v3 signals are decoded by the v3 receiver
exactly as before (no v3 code path changed except the LDPC decoder, see 2.5). The live receiver runs the v4 detector next to the v3 one.
The transmitter makes v3 by default; v4 with opt "kaal.frame" = 4 (see 4).

### 2.5 Receiver (v4)

1. Acquisition: matched filter, 3 sample phases; up- and down-chirp correlation over one superframe; the peak pair gives timing and CFO
   (+-4 kHz without a search); fine CFO from the phase of the sync correlation; checked on the next superframe's sync (40 ms later).
2. Tracking per superframe: timing from the chirp peaks (fractional, parabolic), sample-clock drift from successive superframes; carrier
   phase/frequency from the sync, pilots and a decision-directed PLL over the data symbols.
3. Per slot: least-squares gain/phase/frequency fit on pilots, soft demap with a per-slot noise estimate, LDPC decode (layered min-sum),
   then **iteration**: the decoded (or partly decoded) word is re-encoded, all 136 symbols serve as pilots for a second, finer phase and
   gain estimate, demap again, decode again (turbo-style decision-directed estimation; only when the first decode failed or was close).
4. Profiles: k hypotheses, the slot map first.
5. Multipath: a 5-tap symbol-spaced channel estimate by least squares on the two chirps (32 + 16 contiguous known symbols), averaged over
   superframes; above 2 % off-centre energy a 13-tap MMSE linear equaliser is applied to the whole superframe.

## 3. What was built (branch kaal-phy-030)

- core/include/dect2/kaal/kaal_slots.h: the slot interface for the codec (profiles, budgets, SlotPayload, SlotCodec registration).
- core/include/dect2/kaal/kaal_v4.h, core/src/kaal/kaal_v4.cpp: FrameTx (frame, control channel, text segmentation, beacon), FrameRx
  (chirp acquisition, tracking loops, equaliser, DD smoothing, iterative slot decoding, control word, text reassembly).
- kaal_fec: ldpcDecodeLayered (row-layered normalised min-sum, alpha 0.9). **ldpcDecodeRm (the v3 voice words) now uses it too**: an
  intended decoder change for v3; the transmitted v3 signal and every golden vector are unchanged.
- TX: makeKaalTxGenerator with opt kaal.frame = 4 (also the TX-to-IQ-file path, which uses the same generator); kaal.v4voice, kaal.v4profile,
  kaal.v4robust. The TX panel has a Frame choice (v3 / v4), the slot voice profile and the number of voice slots. Transmitting still goes
  only through the existing Arm / licence / band-plan gate (not touched).
- RX: KaalLiveReceiver runs the v4 receiver beside v3 (on the 96 kHz stream brought to 48 kHz, full band); while v4 is locked the v3
  receivers are not fed. Telemetry: frameVersion, per-slot profile and talker, texts; the RX tab shows the four slots.
- Voice: the PHY carries any codec registered through registerSlotCodec(). Until the codec module (kaal-codec-030) registers one, a v4
  transmission carries the beacon and text only, and the receiver plays nothing (the live test uses a stub codec).
- Tests (tests/kaal, registered by the CMake glob test_kaal_*): test_kaal_v4 (unit + loopback + no false lock on noise or v3),
  test_kaal_v4_impair (the tables below; --full for longer runs), test_kaal_v4_live (TX generator at 240 kHz with a 1.5 kHz offset into
  KaalLiveReceiver: v4 lock, beacon, 300-character text, slot voice through a stub codec).

## 4. Compatibility

v3 signals decode as before: the receiver is the same, only the LDPC schedule changed, and the new one decodes more words, never fewer (all
v3 tests pass, see 7). The v4 receiver does not lock on v3 signals or on noise (tested), and the v3 receiver is not fed while v4 is locked.
Old receivers (0.2.9 and earlier) cannot decode v4 at all: v4 is a new frame, not an extension of v3. The chirp sync and the version byte
(4) in the control word identify it.

## 5. Not done / limits

- No over-the-air test, no GUI run (the panel code is compiled only in CI), no listening test (there is no v4 codec yet).
- Four separate transmitters cannot share one channel yet (no time master, no guard symbols): v4.0 is one transmitter carrying four slots.
- Fast Rayleigh fading (60 Hz Doppler, no line of sight) loses about 30 % of V60 payloads at 20 dB, because a 9 ms slot has no time
  diversity. Interleaving a slot over two superframes would recover much of this for +40 ms latency; not done.
- The slow sweep's weak-signal station ID (-22 dB per sample) has no v4 equivalent. The v4 ID is the control-channel beacon (QPSK, rate
  1/2), which needs about 2-3 dB Es/N0, i.e. about -2 dB per 48 kHz sample. Chase combining of repeated beacon words would gain about
  3 dB per doubling; not done.
- Carrier capture of the v4 receiver: +-4 kHz around the tuned frequency (tested at +-4 kHz). The v3 coarse spectrum search is not used for v4.

## 6. Measured (simulation on this machine, 2026-10-23)

Es/N0 is per symbol, with the noise counted over the whole 48 kHz band (the matched filter removes the out-of-band part). The v3 numbers use
the v3 data power (8 ksym/s, 6 samples per symbol), so its sweep and beacon slice count as interference, as they do on air.

### 6.1 Voice payload loss by Es/N0 (AWGN, carrier +300 Hz, four slots of the profile, 100 superframes each)

| Es/N0 dB | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 1 % PER at |
|---|---|---|---|---|---|---|---|---|---|
| V32R 3.2k (QPSK) | 0.595 | 0.092 | 0 | 0 | 0 | 0 | 0 | 0 | 5.0 dB |
| V48 4.8k | 1 | 0.971 | 0.697 | 0.276 | 0.003 | 0 | 0 | 0 | 6.7 dB |
| V60 6.0k | 1 | 1 | 0.992 | 0.882 | 0.387 | 0 | 0 | 0 | 8.0 dB |
| V80 8.0k | 1 | 1 | 1 | 1 | 1 | 0.700 | 0.058 | 0 | 10.0 dB |

For comparison, v3 (one voice stream) with the new decoder reaches 1 % FER at 8.6 dB for 12 kbit/s and 11.8 dB for 16 kbit/s (table 6.4).
One v4 slot at 6 kbit/s therefore needs about the same signal as v3 at 12 kbit/s, and v4 fits four such slots in the 20 kHz that the v3
24/32 kbit/s stream used.

### 6.2 Offsets (V60, 14 dB)

- Carrier -4000 .. +4000 Hz: PER 0; estimate within 0.1 Hz.
- Sample clock -100 .. +100 ppm: PER 0; clock estimate within 10 %.
- Fractional timing 0.25 / 0.5 / 1.5 samples: PER 0.
- Lock time (first decoded slot after the signal starts; random carrier +-3 kHz, random start, 10 dB): mean 0.12 s, worst 0.41 s.
  v3: 1.25 s with fast acquisition, about 3.5 s otherwise.

### 6.3 Channels and receiver parts (PER, carrier +450 Hz, 100 superframes)

| channel | V60 full receiver | no iterative re-estimation | no DD smoothing | no equaliser | V48 full |
|---|---|---|---|---|---|
| AWGN 10 dB | 0 | 0 | 0 | 0 | 0 |
| Rician K 6 dB, Doppler 10 Hz, 14 dB | 0 | 0.008 | 0 | 0 | 0 |
| Rician K 6 dB, Doppler 60 Hz, 14 dB | 0.026 | 0.103 | 0.158 | 0.026 | 0 |
| Rayleigh, Doppler 10 Hz, 20 dB | 0.058 | 0.129 | 0.118 | 0.058 | 0.021 |
| Rayleigh, Doppler 60 Hz, 20 dB | 0.33 | 0.38 | 0.92 | 0.30 | 0.28 |
| echo -6 dB at 1 symbol, 16 dB | 0 | 0 | 0 | 0.863 | 0 |
| echo -3 dB at 0.5 symbol, 16 dB | 0 | 0 | 0 | 0 | 0 |
| echo -10 dB at 2 symbols, 14 dB | 0 | 0 | 0 | 0 | 0 |
| phase noise 0.01 rad/sample, 14 dB | 0 | 0 | 0 | 0 | 0 |
| phase noise 0.02 rad/sample, 16 dB | 0 | 0.013 | 0 | 0 | 0 |

Under fading, the iterative re-estimation cuts the loss to between a half and a quarter. DD smoothing matters most with Doppler. The
equaliser is what makes a 1-symbol echo decode at all.

### 6.4 Decoder: flooding vs layered LDPC

(640,320) code, BPSK, 2000 words, 40 iterations:

| Eb/N0 | flooding (alpha 0.8) | layered (alpha 0.9) |
|---|---|---|
| 2.0 dB | FER 0.069 | FER 0.035 |
| 2.5 dB | FER 0.009 | FER 0.003 |
| mean iterations | 12.7 | 7.2 |

Whole v3 receiver (3 x 12 s of speech), FER by Es/N0, before -> after:

| Es/N0 dB | 8 | 9 | 10 | 11 | 12 | 13 | 1 % FER |
|---|---|---|---|---|---|---|---|
| v3 12 kbit/s | 0.194 -> 0.090 | 0.014 -> 0.003 | 0 -> 0 | 0 | 0 | 0 | 9.1 -> 8.6 dB |
| v3 16 kbit/s | 1 -> 1 | 0.977 -> 0.918 | 0.784 -> 0.497 | 0.311 -> 0.085 | 0.049 -> 0.006 | 0.005 -> 0 | 12.7 -> 11.8 dB |

### 6.5 Text / data throughput

**Before (v3):** text goes only through the beacon slice, 100 characters per 10 s = **10 bytes/s** at any signal level. This is fixed by
the schedule in kaal_beacon.h; the slice is 1 kbaud BPSK with a rate-1/4 code.

**After (v4):** text delivered in bytes/s (32-byte messages, one D68 slot each), by the number of V60 voice slots:

| Es/N0 | 0 voice | 1 voice | 2 voice | 3 voice | 4 voice |
|---|---|---|---|---|---|
| 8 dB | 1472 | 1336 | 888 | 408 | 64 |
| 11 dB | 3168 | 2376 | 1584 | 792 | 64 |
| 14 dB | 3168 | 2376 | 1584 | 792 | 64 |
| 20 dB | 3168 | 2376 | 1584 | 792 | 64 |

- About 300x more text per second with no voice in the channel, and about 240x with one voice slot.
- With all four voice slots busy, text still goes through the control channel: 33 bytes per 400 ms, i.e. 64 bytes/s of 32-byte messages,
  about 6x v3.
- A 100-character message arrives in one superframe (40 ms) when a slot is free, or in 1.2 s through the control channel. v3 takes up to 10 s.
- Robust data (D26R, QPSK) carries 13 bytes per slot (325 bytes/s per slot) down to about 5 dB.

### 6.6 PAPR (99.9 % / 99.99 % / max)

| signal | 99.9 % | 99.99 % | max |
|---|---|---|---|
| v4, 1 voice slot + data | 5.4 dB | 5.9 dB | 6.6 dB |
| v4, 4 voice slots (same waveform) | 5.4 dB | 6.1 dB | 6.8 dB |
| v3 16 kbit/s with sweep and beacon slice | 6.2 dB | 7.0 dB | 8.7 dB |
| FDMA alternative (4 x 4 ksym/s), 1 sub-channel active | 5.4 dB | 6.1 dB | 6.5 dB |
| FDMA alternative, 4 sub-channels active | 7.7 dB | 8.5 dB | 9.2 dB |

With four channels in use, FDMA costs 2.3 dB more PAPR than v4 TDMA.

## 7. Tests

All KAAL tests, built on their own against this branch's KAAL sources, pass: phy, golden, tx_golden, tx_v3, rx_loopback, roundtrip,
tx_beacon, tx_mix, codec3, sweepid, file, lock_time, engine, live_fuzz, v4, v4_live and v4_impair (see the PATHWAY entry). No golden digest
was changed. test_kaal_mode (source text checks) still finds its strings.
