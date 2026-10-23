# Spectrum / waterfall research (2026-10-23, branch spectrum-sdrpp-030)

## What SDR++ does (source read: core/src/gui/widgets/waterfall.cpp, signal_path/iq_frontend.cpp, root/res)
- FFT: default 65536 points, 20 rows/s, Nuttall window (config fftSize/fftRate/fftWindow=2). Rows are decoupled from the sample rate: the front end skips samples to hit the rate.
- Pixel reduction (`doZoom`): each output pixel = the MAX of ceil(bins/pixels) bins (never an average, never a pick of one bin), so no peak aliases away and a narrow carrier keeps its height at any zoom. The same function feeds the trace and every waterfall row.
- Waterfall: CPU framebuffer of uint32 rows; a new row = `memmove` the buffer down one row + write row 0, uploaded as one texture each new FFT (a texture, not per-pixel draws). Palette = 1,000,000-entry lookup table (no banding), linear blend between the colour-map stops. Level range fixed -120..0 dB by default (fftMin/fftMax; waterfallMin/Max are user sliders; "autoRange" is a button, not continuous).
- Default colour map "Classic" (15 stops): #000020 #000030 #000050 #000091 #1E90FF #FFFFFF #FFFF00 #FE6D16 #FE6D16 #FF0000 #FF0000 #C60000 #9F0000 #750000 #4A0000. Others: gqrx (256 stops), websdr (000000 000050 FF00FF FFFF50 FFFFFF), viridis, turbo...
- Trace: 1 px line per pixel column, colour PlotLines #66E5FF, plus a vertical 1 px line from each point to the baseline in the same colour at alpha 0.2 (this is the "fill"); grid lines #323232 1 px, rounded to whole pixels (no blur); frequency ticks as 1-2-5 steps with a short tick below the axis; optional peak hold line (yellow, falls by fftHoldSpeed per frame); optional smoothing = exponential (alpha 1/beta per pixel column on the already reduced line, default speed 100 = off effect).
- VFO: translucent rectangle over both plots, red border lines for bandwidth edges, snap interval for tuning (click rounds to snapInterval).
- Interaction: click/drag inside the captured span only moves the VFO offset (`setOffset`), the hardware is never retuned for that; dragging the frequency scale pans the view (clamped to the span); wheel on the scale pans by 1/20 of the view; wheel on the plot zooms; left/right keys pan; Page Up/Down cycles VFOs.

## SDRoxide (open source, Rust + wgpu: github.com/dividebysandwich/sdroxide, crates/sdroxide-dsp/src/spectrum.rs, sdroxide-ui)
- FFT: Blackman-Harris window, 50 % overlap, running exponential average of POWER (time constant), SIMD log10 polynomial, one column = strongest bin of its slice (max, not mean).
- The waterfall row = the PEAK of the averaged power since the previous row was taken ("hold"), so rows are decoupled from FFT rate and a short burst can never fall between two rows. The spectrum line uses its own EMA (`spectrum_alpha`), the waterfall uses raw un-averaged frames.
- Waterfall on the GPU: R8 history ring texture, fragment shader samples it with linear filtering (smooth between bins and rows, optional) through a 256x1 RGBA colour LUT. Palettes (11): the default is black, blue(0,0,160 @0.25), cyan (0,180,200 @0.45), green, yellow, red, white; "Icom" etc. All start dark so the noise floor recedes.
- Spectrum: optional vertical gradient fill (top, bottom colours), 1 px trace, cached polyline (one vertex per pixel), decaying peak hold, 1-2-5 grid, per-pixel trace math cached by view.
- Unknown: exact default dB range and colours of the screenshot theme (the repo is open, the reference screenshot is a Japanese theme with a navy gradient background).

## What the good UIs share
log-averaged power with exponential averaging, rows decoupled from the FFT rate, max (peak) reduction per pixel, dark floor to bright peaks palette with fine steps (LUT of 256+ entries, interpolated), 1 px crisp grid snapped to pixels, trace + translucent fill, big readable axis labels on their own strip.

## Our code vs SDR++ (before this branch)
| | OnAir (classic) | SDR++ / SDRoxide |
|-|-|-|
| bins to pixels | min/max envelope, drawn per frame | max per pixel column |
| waterfall rows | one row per drawn frame | row = peak since last row |
| between bins when zoomed in | stair steps | smooth (linear / cubic) |
| palette | few stops, hard steps | 256-1M entry LUT, dark-floor ramps |
| trace | thin, no fill, labels inside the plot | 1 px + 0.2-alpha fill; strip of axis labels |
| grid/ticks | per-plot steps, labels can touch | 1-2-5 steps, labels never touch, minor ticks |
| tuning | see below | see below |

The earlier branch spectrum-look-030 already implemented most of the left column (spec_look.h, plt::Look, WfView Smooth/LowPass, maps `SDR++ smooth` and `Oxide blue`); it was never built. This branch merged it, built it, rendered it and fixed what the build/render showed.

## Presets (Spectrum > Look, in the Display section)
`OnAir classic` (unchanged old look), `SDR++ style` (DEFAULT now: black, light grid, cyan trace over translucent fill, light passband block with red edge and yellow centre, kHz offsets on a black strip, SDR++ smooth map) and `Crisp` (navy-to-grey gradient, thin light-blue cubic-smoothed trace, warm passband, big white mono labels, deep-blue waterfall; modelled on the SDRoxide reference screenshot). Review renders in scratchpad/spectrum-review/ (look0/1/2.png and side_by_side_ref43/44.png). Rendering uses `--hidden --mute --specdemo --look N --shotdelay 25 --shot out.png`; `--specdemo` also suppresses the public-map consent question.

## Tuning feel (owner: "super unsmooth")
SDR++ moves the VFO offset inside the captured span and never touches the hardware for that, and nothing in its click path writes to disk. Ours already moves the marker inside the band (tuneDecode, "Normal tuning") and paces drag retunes at 70 ms (tunedrag::Pacer), but:
1. FOUND: every tuning event (click on the spectrum, step button, frequency box, toolbar retune) called `savePrefs()` synchronously on the UI thread = a rewrite of the whole settings file plus a rename, per click / per step.
2. FOUND: `Engine::retune` runs on the UI thread (driver set-frequency, `holdDelivery(300)`, receivers re-placed). It is paced for drags, but the toolbar/wheel path is not coalesced.
Changed: tuning events now call `savePrefsSoon()`: the save is written 0.6 s after the last tuning event (`savePrefsTick()` each frame, `savePrefsFlush()` at exit). New counters (app/gfx/tune_stat.h, tuneStats()): retunes, mean/max ms in `Engine::retune` on the UI thread, saves, mean/max ms per save, saves deferred; printed as a `tune:` line after every `--shot` run.
Measured (this Mac, this settings file): one save = mean 0.62 ms, max 3.5 ms in an idle run, i.e. that cost per tuning event used to sit between the mouse and the retune; now 0 ms on the tuning path (the single save follows when the tuning rests). The retune call itself was not measured live (no radio attached in these runs; it is counted when one is).
NOT changed / still open (each needs a radio to verify, so left alone): `Engine::retune` still blocks the UI thread for the driver call (a worker thread with latest-wins coalescing is the next step); wheel-on-frequency-digit and arrow keys use the toolbar retune path unpaced; no snap interval on spectrum clicks; cursor-line jitter was not measured.

## Honest gaps
Waterfall noise floor in the demo is almost black (SDR++'s reference shows textured navy): it depends on the level range, which is percentile-based here; a real recording should be tried. No pixel-exact match to the reference; no live-radio check.

## spectrum-match-030 (2026-10-23): measured against the references
References (owner's screenshots, scratchpad refs/spectrum-ref-43/44.png) analysed numerically (scripts in scratchpad/spectrum-match/tools):
- ref44's waterfall pixels lie within 1 RGB step (median; p90 19) of SDR++'s 15-stop Classic map: palette "Navy heat". ref43's ramp extracted by brightness: "Blue green" ((0,2,92) .. (0,14,180) .. (8,96,188) .. (2,154,102)).
- Noise floor: ref44 (0,0,41), blue std 11.5 (mostly over time); ref43 (1,5,124), std 12; texture correlated over ~4 px across and ~3 px down.
- Spectrum: black, grid (35,35,35), trace (140,226,237), fill = trace colour at alpha 0.2, VFO block white alpha 0.2, passband 0.35; ref43: navy (18,50,92) to grey (46,47,52), warm band alpha ~0.16, red centre ~0.4.
What was wrong (harness numbers on real recordings): 16 FFT blocks per frame flattened the floor to +-1.4 dB (featureless rows); the waterfall range was fixed at -100..-32 dB, so the floor sat in mid blue or in black depending on gain; 8192 history columns over 4 MHz = 488 Hz per column (20 px blobs zoomed in); the classic trace drew a min..max bar per pixel column (the owner's white comb at a big FFT).
Changed: 2 blocks a frame; FFT auto 8k..64k (~125 Hz bins); history as fine as the FFT; a 0.15 s power average down the rows; auto range on by default, the floor of the VIEW (median of its lowest 20 %, followed over 2 s) at the palette's place (Navy heat 3 dB over its bottom, 80 dB span; Blue green 8 / 48); the spectrum's dB range follows the same floor; the trace averaged in power with a time constant growing with the bins per pixel; levels per Hz. One look, no presets named after other programs.
Harness tools/spec_render: the app's analyser, Waterfall, WfView and plt drawing rasterised in software. Pipeline CPU (harness, one core): 20 Msps synthetic band 4.4 % -> 1.6 %; 10 Msps real UHF recording 2.6 % -> 1.7 %.
Matched: real 31 m AM station vs ref44: floor (0,0,45) vs (0,0,41), along-row std 4.4 vs 4.9, vertical correlation 0.91/0.75 vs 0.96/0.88; real 31 m band in Blue green vs ref43: floor (0,5,122) vs (1,5,124), std 10.8 vs 12.0.
Still different: the references' own fading (ref44's column std 10 is the signal), ref43's navy gradient behind the spectrum (OnAir keeps the black look the owner approved), the video blur of ref43.
