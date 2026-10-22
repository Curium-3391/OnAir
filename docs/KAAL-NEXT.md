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
message type and sequence, 34 bytes of message (beacon fields, a slot's talker callsign, or a text chunk), CRC-16. 680 bit/s of side
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

## 3. Status

In progress; numbers in section 6 when measured.
