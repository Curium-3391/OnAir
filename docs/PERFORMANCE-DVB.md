# DVB family performance

How much CPU the TV receivers need per second of signal, stage by stage, and what was changed to bring it down (perf-dvb-030).

## How it is measured

`bench_dvb` (tests/bench/bench_dvb.cpp, built with the tests, not run by ctest) generates each standard's built-in test signal with the
synthetic source at the radio rate a user runs it at (resampled when the generator works at the channel's native rate), keeps it in memory
as 8-bit I/Q and feeds it straight into the receiver as fast as it takes it. It prints CPU seconds per second of signal for

- **all threads** of the process,
- the **feed thread** (the engine's analysis thread: the one the radio's ring waits for; above 1.0 the ring overflows),
- every **stage** the receiver names with `DECT2_STAGE("...")` (dect2/common/stage_prof.h: thread CPU time, exclusive of the stages
  opened inside it, off unless enabled: one relaxed load per scope),

and a digest (FNV-1a) of the transport stream delivered, which must not change when an optimisation claims to be exact.

```
bench_dvb --list
bench_dvb --case dvbt,t2 --seconds 6            # one or more cases
bench_dvb --case dvbt --pipelined                # the receivers' live-radio threading (DVB-T decoder thread, T2 resampler thread)
bench_dvb --json out.json                        # all cases, machine-readable
DECT2_STAGE_PROF=1 <any program>                 # switch the stage accounting on elsewhere
```

All figures below are CPU time on an Apple Silicon Mac shared with other jobs, run with `nice -n 19` (so partly on efficiency cores):
compare rows with each other, not with another machine. Each "before/after" pair was run alternately several times and the minimum taken
(the machine's load moves single runs by 10-20%).

## Results (CPU s per s of signal)

| Case | Before: all threads | After: all threads | Before: feed thread | After: feed thread | TS digest |
|---|---|---|---|---|---|
| DVB-T 8K GI 1/8 64-QAM 2/3, radio at 10 Msps | 0.57 | 0.51 | 0.37 | 0.26 (pipelined) | unchanged |
| DVB-T2 32K ext GI 1/128 PP7 256-QAM 2/3, 10 Msps | 1.22 | 0.95 | 0.34 | 0.32 | unchanged |
| DVB-S2 32APSK 4/5 8 MBd, 20 Msps | 1.57 | 1.21 | 0.28 | 0.28 | unchanged |
| DVB-S2 8PSK 3/4 10 MBd, 20 Msps | 1.09 | 1.03 | 0.31 | 0.32 | unchanged |

Baseline only (not changed yet): DVB-C 256-QAM 6.95 MBd at 10 Msps 1.13, ATSC 8 Msps 0.90, ISDB-T 8 Msps 0.57, DVB-S QPSK 10 MBd 0.52,
DTMB 10 Msps 0.49, ATSC 3.0 10 Msps 0.40.

### DVB-T stages (10 Msps, synchronous, minimum of 4 runs)

| Stage | Before | After | Change |
|---|---|---|---|
| dvbt.viterbi | 0.198 | 0.185 | Viterbi chunks on the shared pool, at most 4 at a time, instead of 4 threads started per block |
| dvbt.resample (10 -> 9.14 Msps) | 0.111 | 0.111 | the shared RationalResampler (perf-dsp-030's area) |
| dvbt.chanest | 0.089 | 0.064 | phasor per 64 carriers instead of sin/cos per carrier; buffers and carrier roles kept |
| dvbt.demap | 0.045 | 0.046 | |
| dvbt.rs | 0.032 | 0.018 | decoded bits appended in one go |
| dvbt.fft | 0.031 | 0.031 | |
| dvbt.depuncture | 0.025 | 0.026 | |

With `setPipelined(true)` (the engine does it for live radios) demapping, Viterbi and Reed-Solomon run on their own thread behind a
bounded queue: the feed thread keeps only input, resampler, sync, FFT and channel estimate. The owner's "load 115%, ring overflow" came from
the feed thread waiting for the Viterbi inside feed(). A full queue (the decoder is too slow for the machine) drops the oldest symbols; the
decoder restarts at the next one, and DECT2_DEBUG logs it with the stage names.

### DVB-T2 stages (after)

t2.ldpc 0.27, t2.demap 0.21, t2.resample 0.10, t2.chanest+eq 0.09, t2.measure+constellation 0.05, ts.out 0.05, t2.fft 0.04, t2.bch 0.04.

- `t2LdpcDecode` (core/src/t2/t2ldpc.cpp): decodeFast's layered normalised min-sum, bit-identical (decisions and iteration counts, every
  rate, both frame sizes, QPSK and 256-QAM, 5-30 dB), without the per-block allocation and zeroing of the 1 MB message store, and with the
  parity check on sign bytes. -40% per block on a good signal, -20% at the waterfall.
- Demapper: exact max-log per axis, eight cells at a time, bit de-interleaver folded in.
- An 8-bit LDPC (decodeFast8) for every rate was tried and rejected: about 1 dB worse at 256-QAM 2/3.
- A "the input is already a codeword" pre-check was tried and rejected: at 256-QAM even 30 dB leaves a bit error in most blocks, and the check
  costs 7% of a decode.

### DVB-S2

`s2Demap` eight symbols at a time with the same arithmetic (bit-identical LLRs): 8PSK 23 -> 5 ns per symbol, 16APSK 28 -> 8, 32APSK 82 -> 36.

### Behind time

- DVB-T (pipelined): a decoder thread that cannot keep up drops the oldest symbols and starts again; the count is in
  `RxTelemetry::dvbt.fecDropped` and the engine logs "this computer is too slow for the error correction (stages dvbt.demap, dvbt.viterbi,
  dvbt.rs)" at most every 5 s. The queue is bounded (192 symbols), so it never grows without limit.
- `bench_dvb` also prints the busiest thread: above 1.0 that thread cannot keep up even when the total fits in the cores.

### Fixed on the way

`dvbs::SymbolThread::push` (recordings, blocking mode) could wait for room forever: test_dvbs_rx2 hung for 900 s on pub/main's code
on this machine and passes with the fix (a block goes in once the queue is empty, whatever its size).

## What is left (largest first, from the profiles)

- DVB-S2 front end: S2Rx::fastRun, SymbolTimer, s2RefinePhase, PolyResampler.
- DVB-C: onSymbol, process, smooth.
- ATSC: pfbBlock, equalize.
- ISDB-T: the shared Viterbi (fecp::viterbiRange, about 70 NEON instructions per step: near its limit without 8-bit metrics, which would
  break the bit-exact comparison with the Rust decoder).
- DTMB: its LDPC (dtmb::LdpcCode); ATSC 3.0: resampleExact.
- The 10 -> 9.14 Msps resampler of DVB-T and T2: done in the second pass below (one filter with the clock correction; T2 kernel half as long).

## Second pass (perf2-dvbt-030)

Measured the same way (`bench_dvb`, synchronous, 6 s per case; it now also prints the peak memory), but through the build guard: `nice` plus the macOS *utility* QoS clamp, which
keeps the work mostly on the efficiency cores of a Mac shared with other builds. Those figures are two to four times what a performance core
needs (the same Viterbi microbenchmark: 7.4 ns per step at normal QoS, 34-56 ns under the clamp), so compare the columns with each other only.
Base = the merge of perf-dvb-030 into integ-30 (58f529d21); each figure is the lower of two alternating runs.

### Exact changes (TS digests unchanged)

| Case | Base: all threads | After: all threads | Base: feed thread | After: feed thread | TS digest |
|---|---|---|---|---|---|
| DVB-T 8K GI 1/8 64-QAM 2/3, 10 Msps | 0.834 | 0.562 (-33%) | 0.601 | 0.352 (-41%) | e47eb9823a6e4e86 both |
| DVB-T2 32K PP7 256-QAM 2/3, 10 Msps | 1.502 | 1.144 (-24%) | 0.577 | 0.446 (-23%) | 5d8a78ef18258700 both |
| DVB-C 256-QAM 6.95 MBd, 10 Msps | 1.680 | 1.717 (noise) | busiest 0.993 | busiest 1.012 | 00b2b22145508e1e both |

DVB-T stages (base -> after): Viterbi 0.314 -> 0.276, resample 0.159 -> 0.105 (no change there: core placement noise), chanest 0.105 -> 0.045,
demap 0.088 -> 0.055, FFT 0.060 -> 0.018, depuncture 0.037 -> 0.008, RS 0.023 -> 0.021.
DVB-T2 stages: LDPC 0.384 -> 0.303, demap 0.299 -> 0.167, chanest+eq 0.183 -> 0.147, FFT 0.072 -> 0.049, TS out 0.064 -> 0.022.

What changed (every one compared old against new on the same input, bit for bit, besides the digests):

- **Viterbi** (fec_proto.cpp, NEON): the four butterfly groups' metrics are M0, -M0, M2, -M2, so two are computed per step; four steps per pass;
  decisions stored straight from the register; the 130 kB decision buffer kept with the thread. 69 instructions per step (was ~96). Microbenchmark
  7.2 -> 6.6 ns per step. It is int16 and throughput-bound now: the only big step left is 8-bit metrics, which cannot be exact (the metric spread
  of a K = 7 code with soft values up to 127 needs about 3000).
- **Depuncturing**: G whole puncture periods per NEON table lookup (identical bytes for every rate, phase and length), 3.8x.
- **FFT input**: derotated straight into the split arrays the FFT works on; carriers read in two contiguous runs (no two divisions per carrier).
- **Channel estimate** (GridInterpolator, shared by DVB-T, T2 and ISDB-T): the delay phasors are kept while the timing stays; `convCorrShort`
  computes 32 outputs at a time (eight independent multiply-add chains; each output still one sum in tap order). -50% per call.
- **Equaliser** (DVB-T): data carriers gathered first, then one plain loop.
- **T2 LDPC**: a layer is worked through in blocks of 20 checks with min1/min2/index/sign in registers across the connections; the same operations
  per element in the same order (decisions and iteration counts identical over 2400 blocks, all rates). -7% per iteration; the rest is the float
  arithmetic itself (about 1.7 cycles per edge).
- **T2 demapper**: per axis a minimum tree over the 16 levels (36 minimum operations instead of 64) in NEON registers, NaN and infinity cases
  included: 256-QAM block 170 -> 100 us.
- **T2 cell / time de-interleaver**: buffer kept with the thread, filled column-wise in tiles, no division per cell.
- **T2 data stage and FFT input**: four 256-sample phasor recurrences side by side (the per-sample chain of double complex multiplies was the
  limit); the PLP cells go straight into the stream (no three vectors per symbol).
- **TS unpacker (T2, S2)**: byte-table CRC-8 instead of a bit loop with a branch; no vector per packet. T2 "TS out" -65%.
- **ClockCorrector** (T2 sample-clock correction): NEON, two outputs per pass, identical output, 1.9x.
- Smaller: LLR, FEC and decision buffers kept with their threads instead of allocated (and page-faulted) per block.

Viterbi, second step (exact): renormalisation every 16 steps instead of 4. Without saturation the int16 metrics are exact integers and where the
common offset is taken out changes nothing; 16 steps after a renormalisation every metric stays within +-7200 (spread at most 6 x 2 x 255, at
most 255 per step). 69 -> 64.6 instructions per step.

### Not bit-identical inside, the decoded stream the same (TS digests unchanged)

- **DVB-T: one resampler for the rate change and the clock correction.** At a radio rate that needs resampling (10 Msps for 8 MHz) one
  ExactResampler (16 lobes, 36 taps) goes straight to 64/7 Msps and the clock correction steers its step. Before: the rational resampler, then
  (once the clock is 2 ppm off, i.e. with every real radio and with the test signal too) a second, near-unity ExactResampler: 32 + 32 taps per
  sample. Worst in-band error of the new filter -53 dB at the band-edge carrier. Native-rate input (no resampling) is unchanged.
- **DVB-T2: the resampler kernel sized for the occupied band** (the 27841 carriers of 32K extended, as DVB-T's already was): 32 taps instead of
  64 at 10 Msps, 65 dB in the band that is used.
- Tried and dropped: DVB-C equaliser sum in four partial sums instead of two (the DVB-C digest changed, the thread was not faster); an int16
  layered LDPC (eight checks per vector, saturating; same frame error rate as the float decoder at 1.1 / 1.3 / 3 dB, but no faster in this form).

All together (base = 58f529d21, new = perf2-dvbt-030 head; CPU s per s of signal, lower of two alternating runs, under the guard as above):

| Case | Base: all threads | New: all threads | Base: feed thread | New: feed thread | TS digest |
|---|---|---|---|---|---|
| DVB-T 8K 64-QAM 2/3, 10 Msps | 0.675 | 0.404 (-40%) | 0.463 | 0.261 (-44%) | e47eb9823a6e4e86 both |
| DVB-T same, native 9.14 Msps | 0.549 | 0.459 (-16%) | 0.348 | 0.286 (-18%) | e47eb9823a6e4e86 both |
| DVB-T2 32K PP7 256-QAM 2/3, 10 Msps | 1.148 | 0.902 (-21%) | 0.422 | 0.327 (-23%) | 5d8a78ef18258700 both |
| DVB-T2 8K PP2 16-QAM 3/4, 10 Msps (exact part only) | 0.738 | 0.589 (-20%) | 0.456 | 0.329 (-28%) | 3d8f48b74ae4e2db both |

DVB-T stages at 10 Msps, base -> new: Viterbi 0.265 -> 0.191, resample 0.115 -> 0.049, chanest 0.082 -> 0.042, demap 0.063 -> 0.050,
FFT 0.043 -> 0.017, depuncture 0.032 -> 0.007, RS 0.021 -> 0.019.

### The whole engine (what top shows), 8 MHz DVB-T

`dect2cli` playing a 33 s 8K 64-QAM 2/3 recording at 10 Msps through the Engine (file source thread, 8-bit to float, ring, spectrum thread,
the receiver and its Viterbi workers, TS output; no GUI), as fast as it takes the samples (DECT2_NOPACE), process CPU seconds per second of
signal = the process CPU top would show in real time (100% = one core), nice 19 on the loaded Mac:

| | Base | New |
|---|---|---|
| process CPU | 79.2% | 55.0% (-31%) |
| packets | 485487 | 485487 |

Real-time playback (top-style, 20 s windows) on the same Mac swung between 60% and 96% for the same binary with the load of other jobs, so
those runs only confirm the direction. A 5 s profile of the new build in real time: the onair-analysis thread 68% of the process (Viterbi 20%,
resampler 16%, demap 10% of that thread), eight onair-w Viterbi workers 3.5% each, onair-spectrum 1.8%, onair-src (file read and 8-bit to
float) 1.5%. Without the GUI everything outside the DVB-T receiver is below 4% of the process; in the app the GUI (waterfall, drawing) comes on
top and is not measured here.

### Where it stands

- **Viterbi** is now about half of DVB-T: 64.6 NEON instructions per step, throughput-bound (four pipes), plus a 3-4 cycle traceback chain per
  step. The next big step would be 8-bit metrics (16 states per vector), which cannot be exact with soft values up to 127 (spread up to 3000);
  that would need 4-bit soft values and a BER check.
- **T2 LDPC** (float layered min-sum): about 1.7 cycles per edge, at the limit of four-lane float min-sum. Halving it needs 16-bit messages with
  a frame-error-rate check, and a faster form than the prototype.
- **DVB-C** is limited by serial recurrences that exactness keeps serial: the timing loop (each output position depends on the previous timing
  error), the equaliser (each symbol's taps depend on the previous error), the carrier loop (sine and cosine per symbol on the chain) and the DC /
  mixer recurrences in double. Its stages: timing+equaliser 0.76, decide+RS 0.30, phase-smooth 0.26 (guard figures).
- **Estimated on one Raspberry Pi 4 core** (Cortex-A72, 1.8 GHz, two 128-bit NEON pipes, IPC about 2 on this code): the DVB-T Viterbi alone is
  about 26.8 M steps/s x ~70 instructions = 1.9 G instructions/s, about 0.5-0.6 of a Pi 4 core; DVB-T in all about 1.1-1.3 cores, so real time
  on a Pi 4 needs the Viterbi workers on a second core. DVB-T2 32K 256-QAM: the LDPC alone about 3.7 G instructions/s (2.5 iterations, 925
  blocks/s), about one Pi 4 core; two to three cores in all.
