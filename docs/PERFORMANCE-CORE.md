# Shared sample path: cost, changes, ceilings (0.3.0, branch perf2-core-030)

What every mode pays around its own decoder: sample conversion, the IQ ring, the engine's clean-up, the spectrum analyser, the resamplers, the
FFT kernel, the thread pool. Builds on perf-dsp-030 (docs/PERFORMANCE-DSP.md), which is not repeated here.

## How it is measured

- `tools/bench/bench_core.cpp`, built by `tools/bench/bench_core.sh <out dir> [-- args]` from the DSP sources only (no other build needed):
  `--seconds S --best N --repeat R --only a,b --md`; `DECT2_PORTABLE=1` builds the portable kernels (what Linux and Windows run). Every row
  prints CPU seconds per second of signal (getrusage) and an FNV digest of its output: the digest must not change.
- The whole process: an 8 MHz DVB-T signal (8K 64-QAM 3/4 GI 1/8, the test TS) at 10 Msps cs8 like a HackRF capture, played through
  `dect2cli --file ... --rate 10 --format cs8 --standard dvbt --bw 8` (paced at real time, spectrum on), `/usr/bin/time` for the CPU and
  macOS `sample` for the shares. dect2cli now prints the CPU accounts at the end (`cpu: rx1 front ... | process ...`).
- Machine: M2 Pro (6 P + 4 E cores), shared with up to 20 other jobs (load average 10 to 21 during these runs): kernel rows move by about
  10 %, the whole-process figure by up to 25 % between identical runs. Read small differences as noise.

## Where the CPU of an 8 MHz DVB-T stream goes (macOS, before)

Whole process about 0.9 to 1.0 core at 10 Msps (11 to 12 CPU s for 12 s of signal; Arclamp's report: 89 %). `sample`, busy samples by
inclusive frame:

| Part | Share of the process | Owner |
|---|--:|---|
| DVB-T receiver (`DvbtReceiver::feed`) | 64 % | perf2-dvbt-030 |
| of it: K = 7 Viterbi | 37 % | perf2-dvbt-030 |
| of it: `sincos` per carrier in `estimateAndDecode` | 5 % | perf2-dvbt-030 (a table or a rotating phasor would remove it) |
| of it: RationalResampler 10 -> 9.142857 Msps | 7 % | this branch (kernel) |
| of it: ExactResampler (the clock-drift correction after it, at 9.14 Msps) | 5 % | kernel here; merging it with the rational stage into one ExactResampler 10 -> 9.14 (1 + drift) would remove one of the two (7 %), but changes the bits: the DVB agent's call |
| file reader (`FileSource::produce`, paced) | 3 % | this branch |
| spectrum (thread, analyser, FFT) | 1.6 % | this branch |
| ring, ingest, clean-up | under 1 % | this branch |

So the spectrum is not a large share on macOS (vDSP's FFT is near its ceiling: 0.9 ns per point for 32768 points); the surrounding work
together is about 17 % of the process, the resamplers most of it.

## Before / after: kernels

`bench_core --seconds 2 --best 2 --repeat 2`, same machine, same load. Digest: identical before and after on every row (the bits did not change).

macOS (Accelerate):

| Row | What | Before (CPU s / s) | After | Change |
|---|---|--:|--:|--:|
| `rs_dvbt` | RationalResampler 10 -> 9.142857 Msps, 7.61 MHz occupied (DVB-T 8 MHz) | 0.0524 | 0.0442 | **-16 %** |
| `rs_wide` | the same, full band (DVB-T2) | 0.0739 | 0.0676 | -9 % |
| `spectrum` | engine spectrum path at 10 Msps (queue copy, analyser 32768, 30 frames/s) | 0.0291 | 0.0280 | (noise) |
| `fft32768` | vDSP | 0.0094 | 0.0099 | (unchanged code) |
| `conv_cs8` | cs8 -> cf32 + ring write at 10 Msps | 0.0094 | 0.0090 | (noise) |

Portable kernels (Linux arm64, a Raspberry Pi's code path; measured on the Mac with DECT2_PORTABLE):

| Row | Before | After | Change |
|---|--:|--:|--:|
| `fft1024` | 0.0353 | 0.0170 | **-52 %** |
| `fft8192` | 0.0629 | 0.0222 | **-65 %** |
| `fft32768` | 0.0764 | 0.0413 | **-46 %** |
| `fft65536` | 0.0942 | 0.0442 | **-53 %** |
| `spectrum` (10 Msps) | 0.0862 | 0.0566 | **-34 %** |
| `spectrum20` (20 Msps) | 0.1360 | 0.0904 | **-34 %** |
| `rs_dvbt` | 0.0927 | 0.0578 | **-38 %** |
| `rs_wide` | 0.1621 | 0.0794 | **-51 %** |

x86 (Windows, Linux PCs): the FFT's bit reversal and the AVX2 stage passes are the same algorithm; they could only be checked for bits here
(Rosetta runs the AVX2 code), not timed. The baseline (SSE2) copy uses the plain-C++ passes.

GCC (gcc-16, arm64) and clang give the same bits as their own old loops (each compiler contracts `a*b - c*d` differently; the NEON and AVX2
passes follow the compiler that builds them), checked by `tests/dsp/test_fft_bits.cpp` with clang arm64, GCC arm64, clang x86-64 baseline
and AVX2 (forced on under Rosetta), and DECT2_NO_SIMD.

## What changed

| Where | Change | Bits |
|---|---|---|
| `fftSplit`, portable (dsp_compat.cpp) | the bit reversal: a gather from a copy in 16 x 16 tiles, the first stage in the same pass (the swap loop's `j > i` branch was mispredicted half the time and cost more than all butterflies); stages from half-length 4 (8 on AVX2) on: three per pass in registers (NEON, AVX2 + FMA, plain C++ for the SSE2 copy), 5 passes over the data instead of 13 for 32768 points | identical (test_fft_bits) |
| `RationalResampler` split path (macOS, x86) | every phase's outputs first, then one interleaving pass in output order (each phase used to write into the output at a stride of L samples: 32 passes over 480 KB per block for 10 -> 9.14 Msps) | identical (same desamp calls) |
| `RationalResampler` NEON path (Linux arm64) | the samples stay interleaved, vld2 splits I and Q, four outputs at a time, written in order; the 8 partial sums and their addition tree of the portable desamp | identical (test_fft_bits compares against the split path). Not on macOS: vDSP_desamp's summing order depends on the output count (groups of 8 outputs, another order for the rest), so a copy cannot match it, and it is about as fast |
| `FileSource::produce` (cs8 / cu8 / cf32 / ci16 recordings) and the BLUE reader | one loop per format instead of a format switch per sample (shared `dsp/sample_convert.h`) | identical: x/128 and x/32768 are exact as products, the cu8/cu16 divisions are kept |
| engine `dropNonFinite` | moved to `dsp/sample_convert.h` (shared with the converters) | identical |
| engine spectrum queue | the blocks' storage is reused (no 512 KB allocation per block) | display only |
| thread pool | workers at `ThreadPriority::Decode`; every job charged to the queuing thread's CPU account | identical (scheduling only) |

## Threads and cores

`platform.h` `ThreadPriority`: Realtime (sample path), High (playback, the spectrum thread), **Decode** (new: the shared pool's workers),
Background (bulk: the LDPC pool, the second mode, housekeeping).

- macOS / Apple silicon: QoS classes. user-interactive (Realtime) and user-initiated (High, Decode) are kept on the performance cores;
  utility (Background) and background (the name-classed housekeeping threads of thread_name.h) go to the efficiency cores.
- Linux big.LITTLE (RK3588, phones, Android boxes): the big cores are found from `cpu_capacity` (else `cpuinfo_max_freq`; more than 12 %
  apart). Realtime, High and Decode threads are pinned to them and the shared pool has one worker per big core (less one for the caller), so
  the decoders never wait on a little core and are not more than the big cores; Background stays free (the DVB-T2 LDPC needs every core).
  Equal cores (Raspberry Pi 4/5, PCs): nothing is pinned.
- Windows: thread priorities (time-critical, above normal, normal, below normal); Background also asks for EcoQoS (Windows 11 puts it on the
  efficiency cores of a hybrid Intel processor).
- No oversubscription: one shared pool for the whole process (hardware threads - 1, at most 8), whatever the number of receivers; parallel
  work is `parallelFor` on it with the caller helping.

## CPU meter (core API for the status bar)

`common/cpu_meter.h`: accounts by name (`cpu::account`), a thread binds to one (`cpu::bindThread`) or charges a block to one
(`cpu::Scope`); the pool runs each job under the account of the thread that queued it. Read with `cpu::percent(id)` (percent of one core,
over about the last second), `cpu::processPercent()`, `cpu::usage()`. The thread CPU clock is read at the scope edges only (no sampler).

Engine: `cpuPercent()` (this receiver: its sample path, spectrum, decoder with its pool jobs, second mode) and `cpuUsage()` (the parts:
"rx<N> front", "rx<N> spectrum", "rx<N> decoder", "rx<N> second"). On the DVB-T file: front 0.10 s, spectrum 0.15 s, decoder 7.7 s of
11.6 s for the process; the rest is the file reader / radio thread, the TS demux and player, and start-up.

## Looked at, not changed

- Spectrum statistics (level, DC, peak, ADC histogram): a NEON histogram gave the same bits and no time (the loop is not the histogram).
  The FFT duty (every sample goes through a 32768-point FFT at 10 Msps, up to 16 blocks per frame) was left as it is: fewer blocks would
  make the trace and the waterfall visibly noisier, and on macOS the whole spectrum is 1.6 % of the process. A cap would be a settings choice.
- `ThreadPool::parallelFor`: about 20 to 40 us of CPU per call with 3 helpers (waking workers). DVB-T calls it about 50 times a second:
  0.1 to 0.2 % of a core; not worth a spinning pool.
- The IQ ring (16 M samples, 128 MB) is bigger than any cache, so the radio's writes go to DRAM; conversion + ring write is 0.9 ns a sample
  (0.9 % of a core at 10 Msps). A smaller ring would trade robustness for a fraction of that.
- `ExactResampler` (resampleLoop): already NEON / AVX2, two outputs per pass, load-bound.

## Honest ceilings

- macOS DVB-T 8 MHz: the shared parts are now about 15 % of the process; the vDSP FFT and vDSP_desamp are at or near their ceilings with
  these bits. The large remaining items are in the decoder (Viterbi 37 %, per-carrier sincos 5 %, two resampling stages 12 %).
- Linux / Windows: the portable FFT is still 4x slower than vDSP (radix-2 arithmetic: more operations than vDSP's radix-4/8). A radix-4
  kernel would be faster but changes the bits of every FFT user (all OFDM receivers): a decision for a release with re-checked digests.
