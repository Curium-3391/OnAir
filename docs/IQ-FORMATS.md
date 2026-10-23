# IQ file formats

## X-Midas BLUE (.tmp, .prm, .blue, .bluefile)

OnAir reads and writes X-Midas BLUE files (also called Bluefiles): a 512-byte header, the samples, and an optional extended header of
keywords. Code: `core/src/source/bluefile.cpp`, `core/include/dect2/source/bluefile.h` (namespace `dect2::blue`).

Written from the public format description (the SigPlot "bluefile" page and the sigfile `bluefile.js` documentation page, read as
documentation only). No code from XMidasBlueReader, SigPlot or sigfile was copied or used; the licences of those projects therefore do
not apply to OnAir. The owner's spec PDF (8173203.pdf) was not available to the author: where the web pages are silent (listed below)
the behaviour is conservative.

### Reading

* Opening: the Open button of the file source, the recordings library, the wideband viewer; a file is BLUE when it has one of the
  extensions above or starts with the four bytes `BLUE` under any other name (except .cs8 / .cu8 / .cf32).
* Header: `version` must be `BLUE`. `head_rep` and `data_rep` are `EEEI` (little-endian) or `IEEE` (big-endian), separately, so a file
  with a big-endian header and little-endian data works. `type` 1000 and 2000 are read (2000 as one stream of samples; `subsize` is
  kept). Detached headers (`detached` != 0) are refused with a message.
* Sample rate: `1 / xdelta` when `xunits` is 1 (seconds) or 0 (none). A frequency axis (`xunits` 3, a spectrum file) is not a sample
  rate; then the keyword `SAMPLE_RATE` is used if present, else the file is refused for playback ("does not give a sample rate").
* Format: two letters. First `S` (scalar, "real") or `C` (complex). Second `B` int8, `I` int16, `L` int32, `X` int64, `F` float32,
  `D` float64, `O` unsigned 8-bit (offset binary). Other first letters (vectors `V`, `Q`, `M`, `X`, `T`, ...) and `P` / `A` are refused.
* Scaling: integers are divided by 2^(bits-1) (full scale = 1.0), `O` is (v-128)/128, floats are used as they are.
* Real (`S`) data plays as I with Q = 0 (the spectrum shows it mirrored, as for any real signal). A Hilbert / "real as IF" option is not
  offered for BLUE files yet.
* Playback path: complex `CB`, `CI`, `CF` play straight from the file (offset and length from the header, big-endian too). Every other
  format (real, `L`, `X`, `D`, `O`) is first copied to a cf32 file `onair-blue-<hash>.cf32` in the system temp folder (streaming, so large
  files are fine; reused while the source is unchanged) and played from that. The recordings library never makes this copy.
* Centre frequency: the first of the keywords `RF_FREQ`, `RF_FREQUENCY`, `CENTER_FREQ`, `CENTER_FREQUENCY`, `CENTRE_FREQ`,
  `COL_FREQ`, `COLLECT_FREQ`, `CHAN_FREQ`, `FREQ` found (Hz). There is no standard keyword for it; the list is a best guess.
  `DESCRIPTION` / `COMMENT` become the description.
* Damaged files: `data_size` larger than the file is clamped (a note "file is shorter than its header says" is shown); `data_size` 0
  (a recording that was never closed) uses the rest of the file; a damaged keyword record ends keyword reading but the samples still
  play; a non-BLUE `.tmp` is reported, not listed.

### Writing

* Recorder: the format box in the diagnostics row has "X-Midas BLUE (.tmp)" (float complex samples, file `...Msps_<stamp>.tmp`).
  Rate is stored as `xdelta` (seconds), the centre frequency as keyword `RF_FREQ`, plus `SAMPLE_RATE` and `DESCRIPTION` (the mode id).
  No SigMF files are made for a BLUE recording. The API (`IqRecorder::start` with a .tmp/.blue/.prm path) also takes the 8-bit format.
* Transmit to an IQ file: formats "X-Midas BLUE cf32 / ci16 / ci8".
* `blue::Writer`: complex or real, `B I L F D`, little- or big-endian (EEEI / IEEE) for header and data; data padded to a 512-byte block,
  keywords after it, `data_size`, `ext_start`, `ext_size` filled at close. Little-endian IEEE float with `EEEI` is what the app writes.
* Keyword records: `lkey` (total, multiple of 8), `lextra` (everything but the value), `ltag`, type letter, value, tag.

### Not verified against third-party files

Everything was tested with files written by OnAir's own writer plus hand-built headers (tests/source/test_bluefile.cpp). A real X-Midas
file may differ in: the keyword record layout (`lextra` meaning), which keyword holds the centre frequency, `xunits` codes, byte order of
keyword values (header order is assumed), or the main-header 92-byte keyword string (ignored). Useful samples to send: a `.tmp` from
X-Midas/`sigplot` examples in `SF`/`CF` and `CI`, a big-endian (`IEEE`) file, a type 2000 file, and a file with `RF_FREQ`-style keywords,
together with the numbers the tool that wrote it shows (rate, centre, length).
