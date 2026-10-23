# Image enhancement

One enhancement pipeline for every mode that produces pictures. The core is pure functions in
`core/include/dect2/image/enhance.h` (`core/src/image/enhance*.cpp`); the app control is `app/panels/enhance_ui.{h,cpp}`.
Classical, clean-room algorithms only; everything runs locally on the CPU.

## Principles

- **Non-destructive.** The raw decoded picture is always kept and saved exactly as before. The enhanced picture is a view, and
  (when asked for) an extra file: `Save enhanced PNG...`, and the gallery's `<id>_e.png` copy for modes with *Apply to all new images*.
- **Fast.** Float planes, row bands on `ThreadPool::shared()`; a cheap preview path (no non-local means, no upscaling, no line
  registration) while a picture is still arriving line by line. The enhancement runs on the pool, never on the UI thread.
- **Deterministic.** Every band writes only its own rows; the output is bit-identical for any `DECT2_THREADS` (checked by the test).

## Where it is

| View | Source key (prefs, kind) |
|---|---|
| Gallery viewer (View > Images): every picture the satellite, fax, SSTV, HamDRM, DRM, GOES/Elektro, Meteor receivers publish | the picture's source |
| SSTV (HF digital): live picture while it builds, and the finished thumbnails | `SSTV` (Sstv) |
| Weather fax (Marine): live while it builds | `Weather fax` (Fax) |
| DRM slide show | `DRM slide show` (Slide) |
| DAB slide show | `DAB slide show` (Slide) |
| KAIT picture | `KAIT picture` (Slide) |
| Analog TV frame | `Analog TV frame` (Tv) |

`kindForSource()` also knows NOAA APT, LRPT, HRIT/LRIT, METEOSAT, HRPT, Wenet/SSDV, RF camera, FPV names, so a new picture mode only
has to publish to the gallery (or add an `enhui::LiveTex`) to get enhancement with the right presets. Not wired in this change: the RF
camera (its picture is a camera frame with an RF heat overlay composed every frame), HRPT and the live Meteor/GOES strips (they reach
enhancement through the gallery), and Wenet/SSDV (lands with the Horus branch).

## The control

`[x] Enhance  [preset v]  [...]  [before | after slider]  [Hold to compare]  [Save enhanced PNG...]  [x] Apply to all new images`

- **Presets:** Auto, Chart, Photo, Satellite, Colour SSTV, Custom. Auto picks by kind and picture: a mostly bimodal grey fax is a Chart,
  a colour SSTV picture Colour SSTV, satellites Satellite, the rest Photo. The combo shows what Auto chose.
- **`...`** opens the settings: every stage with its slider. Changing anything switches to Custom, starting from the preset on show.
  The popup ends with the report of the last run (slant found, line start shift, lines repaired, specks removed, time).
- **Before/after:** the slider puts a split line on the picture; left of it is the picture as received. *Hold to compare* shows the raw
  picture while the button is held.
- **Apply to all new images** is kept per mode in the preferences (`enh_<source>_auto`, `_preset`, `_custom`): views of that mode start
  enhanced and the gallery writes an enhanced copy next to every new raw picture of that mode (in the background; only the newest
  copy is written when a growing picture is published again).
- In the gallery, the enhancement works on the picture after the false-colour channel choice, and the existing tone studio (stretch,
  gamma, curves, palettes, map projection) works on its result. The map needs the picture at its own size, so Upscale and Fax width
  turn the projection off.

## Stages (in order)

1. **Geometry.**
   - *Slant* (clock error): the line-to-line drift of the line starts, measured by circular cross-correlation of line pairs at growing
     lags (4, 32, 128, 512 lines, each stage searching around the slope of the one before), sub-pixel by a parabola, robust median.
     Lines are resampled (linear, circular for fax). A manual slant slider adds to it.
   - *Line start (phasing):* the most constant vertical band that stands out from the picture level (sync bar or margin) is found
     and the picture is turned round so the band starts at column 0, so the chart is not split.
   - *Repair:* a line whose best correlation with the lines up to 3 away collapses below a fraction of the picture's median (or that is
     flat or saturated in a busy area) is interpolated from the nearest good lines above and below.
   - *Line jitter:* every line's shift against the line before is chained into a position; the running median of the chain is the slow
     part (slant, curved content) and the rest is undone.
   - *Fax width:* IOC 288 / 576 (905 / 1810 px).
2. **Noise.** Switching median for impulses only: a pixel is replaced only when it is isolated (at most one of its 8 neighbours alike)
   and far from the 3x3 median, so 1 px chart lines and text survive. Deinterlace (second-field lines that disagree with the average of
   the first field around them, where that field agrees). Destripe (per-line gain/offset pulled to the running median of the
   neighbouring lines) and deband (the same per column). Denoise 0..100: bilateral (default) or non-local means.
   `temporalAverage()` aligns repeated transmissions (SAD search, circular in x) and takes the median of 3 or more copies.
3. **Colour** (colour pictures): chroma phase fix (the shift that lines up chroma gradients with luma gradients) and chroma smoothing
   much stronger than luma; grey-world cast correction with a gain limit.
4. **Tone.** Chart levels (Otsu threshold; ink and paper levels; a smooth step around the threshold, never binary, so thin lines keep
   their antialiasing) or Auto levels (percentile stretch with protected black and white: the gain is at most 4, so a flat or dark
   picture is not stretched into noise). Gamma. CLAHE with a clip-limit slider. `histMatch()` for satellite channels. Palettes: sea /
   land / cloud tint, temperature (infrared, bright = cold), rainbow.
5. **Detail.** Lanczos x2 (6 taps, every output clamped to the range of the 2x2 source pixels around it: no ringing), unsharp mask
   (radius and amount; a small threshold so flat noise is not sharpened).

The **Chart** preset uses only geometry, impulse removal, threshold-aware contrast and text sharpening; it never blurs.

Coastline overlays come from the gallery's map projection (existing vector data), which works on the enhanced picture.

## Measured (tests/image/test_enhance.cpp, synthetic pictures with known degradations)

Apple M2 Pro, 10 cores, `-O2`, clean picture as the reference.

| Case | Before | After |
|---|---|---|
| Fax chart 1200x800: slant 0.15 px/line, line start 300 px, 5 noise lines, 3 missing lines, 0.5 % specks (Chart preset) | PSNR 8.50 dB, SSIM 0.525 | **26.19 dB, SSIM 0.962** |
| ... slant estimate | true 0.1500 px/line | 0.1495 (0.43 px over 800 lines) |
| ... line start found / sync bar edge on lines 10 and 790 | 300 px off | 300 found, edge at 60 / 60 (clean 60) |
| ... lines repaired | 8 damaged | 8 repaired, none on the clean picture |
| Clean chart through Chart (no geometry): what it changes | | 27.1 dB (only the grey grid darkened and edges steepened) |
| Fax line jitter +-3 px | 19.54 dB | 25.11 dB |
| Fax repeated 3 times, noise 0.15, copies shifted (7, 0) and (0, 3) | 19.40 dB (one copy) | 22.85 dB (shifts found exactly) |
| SSTV 320x256 slant 0.06 px/line | | est 0.0634 (0.88 px over 256 lines) |
| SSTV chroma lag 3 px | 25.71 dB | 36.57 dB (3.00 px found) |
| SSTV gaussian noise 0.07 | 23.20 dB, SSIM 0.388 | bilateral 30.97 / 0.794, NLM 37.41 / 0.970 |
| SSTV noise + slant + 4 missing lines (Colour SSTV, levels and cast off) | 13.65 dB, SSIM 0.233 | 22.31 dB, SSIM 0.495 |
| Interlaced frame (second field 6 px off) | 21.36 dB | 35.53 dB |
| APT-like stripes (per-line gain 0.8..1.2, offset +-0.06) | 27.48 dB, SSIM 0.454 | 34.95 dB, SSIM 0.949 |
| 2 % salt and pepper (switching median) | 22.42 dB | 39.51 dB; 0 false hits on the clean picture |
| 8 missing / corrupt lines in a photo | 24.14 dB | 74.28 dB (exactly the 8 found) |
| x2 upscale of a halved photo | bilinear 34.11 dB | Lanczos 35.49 dB, no overshoot on a step |

Speed for 1000 x 1000 (best of 3; full path / preview path):

| Preset | Full | Preview |
|---|---|---|
| Chart (fax) | 31 ms | 26 ms |
| Photo | 17 ms | 18 ms |
| Satellite | 25 ms | 28 ms |
| Colour SSTV | 85 ms | 81 ms |
| Non-local means 40 | 125 ms | (bilateral instead) |
| Lanczos x2 | 18 ms | (off) |

With `DECT2_THREADS=0` (one thread) the presets take 50 to 350 ms and NLM about 1 s; the output checksum is the same.

## Limits

- The slant measure needs a stable feature down the picture (a sync bar, margin, or vertical edges); on a picture of noise it
  finds nothing and does nothing.
- A live fax or SSTV picture is enhanced with the preview path while it arrives; the fax view keeps that path (the Chart preset does
  not use the stages the preview leaves out). The SSTV view switches to the full path when the last line is in.
- The live tabs draw the enhanced picture in the box of the raw one, so Fax width changes the shape there; the gallery shows the true
  size.
