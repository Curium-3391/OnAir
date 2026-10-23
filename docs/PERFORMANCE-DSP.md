# Signal-processing cost (0.3.0 work, branch perf-dsp-030)

What the receivers and DSP kernels cost in CPU, measured without the GUI, the Engine or a radio, and what was changed to lower it. The GUI and
frame loop (perf-ui-030) and idle / battery behaviour of the app (perf-power-030) have their own work and are not in these figures.

## How it is measured

`tools/bench/bench_dsp.cpp`, built and run by `tools/bench/bench_dsp.sh` (not part of ctest, not part of the CMake build):

```
CORE_BUILD=<a cmake build dir> tools/bench/bench_dsp.sh /tmp/bench-dsp -- --seconds 4 --best 3 --md
tools/bench/bench_dsp.sh /tmp/bench-dsp -- --list            # the rows
tools/bench/bench_dsp.sh /tmp/bench-dsp -- --only dvbt,fm    # some rows
DECT2_PORTABLE=1 tools/bench/bench_dsp.sh ...                # the portable kernels (what Windows and Linux run) instead of Accelerate
```

The script compiles only the core sources the rows need (DSP, the measured receivers, their FEC) from the tree, at `nice -n 19`, caching the
objects and their header dependencies; everything else a receiver pulls in (audio output, feeds, the Rust layers) is linked from an existing build
(`CORE_BUILD`). The sources measured are always the tree's own. Do not run the CTest suite against such a mixed binary: classes whose layout
changed are still compiled the old way inside the existing build.

Each row makes its signal first (a mode's built-in test generator, noise for the `_idle` rows), not timed, then feeds it in blocks of 16384
samples, or runs the kernel over it. The figure is **cost = process CPU seconds (user + system, all threads, getrusage) per second of signal**;
a receiver with its own threads is destroyed inside the timed part so the work it still had queued counts. `--best N` keeps the run with the
lowest cost of N (the least disturbed by other jobs on the machine). "Items" is a sanity count (packets, log lines, outputs): it must not drop
after a change.

Accuracy: on the loaded development Mac (many agents building at once) unchanged rows move by up to about 10 % between runs (`exact_rs`,
`tv_channel` below did not change). The multi-threaded TV receivers (DVB-T, DTMB, ATSC) move by up to 20 %: they drop work when they fall
behind, so their item counts and cost vary from run to run (ATSC decoded 35 672 to 50 804 packets from the same 4 s in repeated runs of the
same binary). Their rows show no change beyond that noise except where noted.

## Before / after

Apple M-series Mac, macOS, Accelerate kernels, `--seconds 4 --best 3`. Before = pub/main 279129cf8, after = this branch.

| Item | What | Rate (Msps) | Before (CPU s / s) | After | Change | Items before / after |
|---|---|--:|--:|--:|--:|--:|
| `fft32k` | fftSplit 32768 points, back to back at 10 Msps | 10 | 0.0094 | 0.0096 | +2% | 1220 / 1220 |
| `spectrum` | SpectrumAnalyzer 32768 bins at 10 Msps, a frame every 40 ms | 10 | 0.0251 | 0.0249 | -1% | 100 / 100 |
| `rational_rs` | RationalResampler 10 -> 9.142857 Msps (DVB-T2 native) | 10 | 0.0780 | 0.0623 | **-20%** | 36571360 / 36571360 |
| `rational_dec` | RationalResampler 2.4 Msps -> 48 kHz, 12.5 kHz occupied (wide-bank carrier) | 2.4 | 0.0014 | 0.0013 | -7% | 191998 / 191998 |
| `exact_rs` | ExactResampler 10 -> 9.142857 Msps, 16 lobes | 10 | 0.0461 | 0.0425 | (noise) | 36571409 / 36571409 |
| `tv_channel` | ChannelFilter (multi-TV channeliser) 20 Msps -> 10 Msps, 4 MHz off | 20 | 0.0561 | 0.0492 | (noise) | 39999992 / 39999992 |
| `wide_bank` | CarrierFinder + 8 Channelizers 2.4 Msps -> 48 kHz (wide-bank carriers, receivers not included) | 2.4 | 0.0801 | 0.0269 | **-66%** | 1535984 / 1535984 |
| `fm_front` | FmFront (LTR / EDACS control channel) 2.4 Msps -> 48 kHz, 100 kHz off | 2.4 | 0.0204 | 0.0113 | **-45%** | 191992 / 191992 |
| `t2_ldpc` | DVB-T2 LDPC 64800 rate 3/5, 624 blocks a second (8-bit layered min-sum) | - | 0.3186 | 0.2994 | (noise) | 2496 / 2496 |
| `fm` | FM broadcast receiver (stereo, RDS) at 2 Msps | 2 | 0.0559 | 0.0386 | **-31%** | - |
| `nfm` | General receiver, NFM | 2.4 | 0.0439 | 0.0322 | **-27%** | - |
| `am` | General receiver, AM | 2.4 | 0.0380 | 0.0277 | **-27%** | - |
| `usb` | General receiver, USB | 2.4 | 0.0386 | 0.0277 | **-28%** | - |
| `adsb` | ADS-B 1090 receiver at 2.4 Msps | 2.4 | 0.0452 | 0.0430 | (noise) | 2 / 2 |
| `ais` | AIS receiver (both channels) at 2 Msps | 2 | 0.0056 | 0.0056 | 0% | 9 / 9 |
| `vdl4` | VDL Mode 4 receiver (six channels) at 2 Msps | 2 | 0.0738 | 0.0670 | -9% | - |
| `dmr` | DMR receiver at 2.4 Msps | 2.4 | 0.0169 | 0.0157 | (noise) | 14 / 14 |
| `p25` | P25 receiver at 2.4 Msps | 2.4 | 0.0299 | 0.0287 | (noise) | - |
| `tetra` | TETRA receiver at 2.4 Msps | 2.4 | 0.0331 | 0.0254 | **-23%** | 4 / 4 |
| `dvbt` | DVB-T 8K 64-QAM 3/4 GI 1/8 at the native 9.142857 Msps | 9.14 | 0.5776 | 0.6135 | (noise, see below) | 63758 / 63758 |
| `dtmb` | DTMB receiver at 10 Msps | 10 | 0.5151 | 0.5263 | (noise) | 55881 / 55863 |
| `dab` | DAB receiver at 2.048 Msps | 2.05 | 0.0248 | 0.0258 | (noise) | - |
| `atsc` | ATSC 1.0 receiver at 8 Msps | 8 | 0.7175 | 0.8686 | (noise, see below) | 46540 / 50804 |
| `dvbt_idle` | DVB-T receiver on noise (searching) | 9.14 | 0.2410 | 0.1620 | **-33%** | - |
| `dtmb_idle` | DTMB receiver on noise | 10 | 0.1434 | 0.1450 | 0% | - |
| `atsc_idle` | ATSC 1.0 receiver on noise | 8 | 0.5846 | 0.4592 | **-21%** | - |
| `dab_idle` | DAB receiver on noise | 2.05 | 0.0245 | 0.0246 | 0% | - |
| `fm_idle` | FM broadcast receiver on noise | 2 | 0.0545 | 0.0425 | **-22%** | - |

DVB-T and ATSC with a signal: four more alternating runs of both binaries gave DVB-T 0.76 / 0.55 (before) against 0.60 / 0.56 (after) and
ATSC 0.80 / 0.74 against 0.65 / 0.62 CPU s per s; neither change touches their locked path except the Viterbi scheduling (below), so read
these rows as unchanged. The searching rows are clear: DVB-T alone, 6 s, best of 2: 0.277 -> 0.140.

Where the CPU of the expensive rows goes (macOS `sample`, self time, after): DVB-T: the K = 7 Viterbi (NEON, about half), FFT, demapper,
resampler; ATSC: the stage A polyphase filter (`pfbBlock`, NEON, 55 %), the segment sync and equaliser; DTMB: its LDPC decoder; DVB-T2: the
8-bit layered min-sum LDPC. These were already vectorised by earlier work.

## What changed

All changes give the same output bits as before unless a tolerance is given. Tests added: `channelizer_mix`, `viterbi_simd`.

| Where | Change | Result |
|---|---|---|
| `Channelizer` (carrierfind.cpp: wide-bank carriers of DMR, P25, NXDN, dPMR, TETRA, ...) | the per-sample `std::complex<double>` phasor (`p *= rot`, a NaN check per product, no vectorising) replaced by the table mixer `ChannelFilter` already used: 1024 float steps and one double phasor per table period, renormalised | `wide_bank` 0.080 -> 0.027; within 1e-6 of a double phasor at rms 0.2 (test `channelizer_mix`) |
| `CarrierFinder::feed` | no vector allocated per call, block copies instead of a per-sample loop, `std::norm` without its infinity branch | same sums; same carriers for any block size (test) |
| `RationalResampler::process` (DVB-T2, DAB, the wide-bank carriers, ...) | phase outputs written straight into the output (no four temporary vectors per call, no split-and-interleave pass), interleaved input read as floats | bit-identical output (hash compared on four ratios with random block sizes); `rational_rs` -20 % |
| `qo100::Nco::mix` (General receiver NFM / AM / SSB / CW, QO-100) | the complex products written out (no NaN check after every product) | bit-identical; general receiver -27 % |
| FM broadcast `cma()` (16-tap constant-modulus equaliser per channel sample) | the same | bit-identical (checked: `std::complex` and the written-out form give the same bits with and without FMA); `fm` -31 % |
| `FmFront` (LTR, EDACS) | a rotating phasor (double, renormalised every 1024) instead of `cos` and `sin` per input sample | `fm_front` -45 %; LTR and EDACS receiver tests pass |
| TETRA `feed` / `channelize` | the level and copy loops branch-free with one resize (was `push_back` per sample); the channel mixer's products written out | bit-identical; `tetra` -23 % |
| VDL Mode 4 | the per-sample products of the six channel mixers written out | bit-identical; -9 % |
| DVB-T acquisition (`acquire()`, runs on every block while searching) | the 1.3 MB of cumulative sums are kept between calls (were allocated per search), real / imaginary rows, the fold by symbol phase one period at a time (vectorises), the correlation at the peak phase summed only for the phase that is used, the per-phase counts computed | the same decisions (same sums in the same order) |
| DVB-T search rate | after 2 s without a cyclic prefix the search window advances by 7/8 of its length instead of 1/2 (each sample looked at once instead of twice) | with the above, `dvbt_idle` 0.24 -> 0.16 (0.28 -> 0.14 alone); the first 2 s of a search are unchanged |
| DVB-T Viterbi `decode` | the chunks run on the shared pool (`ThreadPool::parallelFor`, at most the caller and 3 workers as before) instead of starting and joining 4 threads per block of 16 symbols (about 200 thread starts a second) | same bits (each chunk writes its own slots) |
| `ThreadPool::parallelFor` | a `maxHelpers` limit; the caller is woken once, by the last job, not after every index | |
| ATSC segment sync search (13 000 a second while unlocked) | the median of the correlation is only computed when the peak passes the absolute threshold (it is only used then), scratch kept between searches | identical decisions; `atsc_idle` 0.58 -> 0.46 |

## Looked at, not changed

- **K = 7 Viterbi (DVB-T, ISDB-T)**: the NEON trellis is latency-bound (about 8 ns a step on M-series). Packing the 64 decisions with seven
  shift-and-inserts instead of narrow + mask + pairwise adds saved instructions but no time (measured 8.0 against 8.1 ns a step); not kept. Two
  chunks interleaved in one loop would hide the latency but needs more than the 32 vector registers. `tests/fec/test_viterbi_simd.cpp` (vector
  against a scalar copy) is kept for whoever tries next.
- **`-fcomplex-arithmetic=improved`** for the whole core: every `std::complex` product loses its NaN check (multiplication results are the
  same bits, checked), a further 5 to 15 % on several receivers. Not done: complex **division** then uses Smith's algorithm instead of the
  runtime's, which changes the last bit of every equaliser division (OFDM receivers divide by the channel estimate). Worth doing with a check of
  every receiver's decoded output.
- **LDPC** (DVB-T2 8-bit min-sum, DTMB float min-sum): already vectorised, with early termination and give-up rules; nothing found that keeps
  the decoded output.
- **ATSC stage A filter** and the ExactResampler loop: already NEON / AVX2 with several outputs per pass.
- **DTMB on noise** (0.14): the frame search, its own work; not changed.
- **Noise blanker**: there is none in core (FLARM, DJI and the packet demodulators have local impulse handling). **Spectrum / waterfall
  decimation**: the analyser (`SpectrumAnalyzer`, 0.025 at 10 Msps) caps itself at 16 blocks a frame; the waterfall drawing is GUI code
  (perf-ui-030).
- The `p25` rewrite of its per-sample products gave nothing measurable (a serial recurrence: the NaN check is a well-predicted branch there) and
  was left out. Moving the ATSC filter's second half from its own thread to the shared pool cost more CPU (the pool workers run at default
  priority, the helper ran at real-time priority on a performance core) and was left out.

## The pattern that paid most

`std::complex` products in a per-sample loop: in the libc++ of current Xcode they are `_Complex` builtins with C99 Annex G NaN recovery, so every product
is followed by a NaN test and a possible library call, and the loop does not vectorise. Written out (`ac - bd`, `ad + bc`, one product per
statement so nothing is fused differently) the result is the same bits and the loop is plain float or double arithmetic. Where the loop is a
long serial recurrence (a phasor `p *= step` for every sample) a table of steps with one exact phasor per table period removes the
recurrence too.
