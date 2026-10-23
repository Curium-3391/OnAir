# Issues 2026-10-23: bugs, crashes and platform problems

Branch `issue-bugs-030` (from `integ-30`, which already holds `fix-gl31-030`). Reply drafts: `scratchpad/replies-3.0/<issue>-<slug>.md` (session scratchpad). Nothing posted on GitHub.

| # | Title | Request | Status | Commit / branch | Not verified | Reply draft |
|---|---|---|---|---|---|---|
| 284 | No waterfall on Pi CM5 (Trixie) | waterfall empty, GL_INVALID_VALUE 8192x512 | fixed now | 1f1c82b41 (tex_shrink, maxImageSize, CI llvmpipe GL 3.1 run), b55fc1945 docs | real V3D / Pi | 284-waterfall-4096.md |
| 246 | Waterfall not refreshing (Android) | spectrum/waterfall only move on touch | done before | Android 3.0 rework (704c4a236, 171f36905) | real phone | 246-android-waterfall.md |
| 270 | Waterfall misalignment | waterfall vs spectrum offset | done before | cf3cbec20 | - | 270-waterfall-align.md |
| 228 | SoapyRemote hangs OnAir (macOS) | no hang; later: gain tops out, no lock | done before (hang, 252362614) + fixed now (bug report: the list entry kept gain/rate "unknown", slider 0..1 dB = the RSP's least gain; the ranges are taken over once the radio runs) | 84e96e823 | RSP1B over SoapyRemote | 228-soapyremote.md |
| 226 | OnAir cannot run | i5 2nd gen, no AVX2 | done before (ggml baseline) + fixed now (plain-words SIGILL message) | 9822317da, 9ae1616aa | a no-AVX2 PC; Android CDR part | 226-no-avx2.md |
| 201 | Android crash on stream start | freeze after ~5 s, broken pipe | done before | 5161882f7, aec57e521, Android 3.0 rework | real phone + rtl_tcp driver | 201-android-freeze.md |
| 219 | RSP1 weird tuning on click | clicked frequency off by ~250 kHz | done before | bc04d6202 | real RSP | 219-sdrplay-click.md |
| 279 | Codecs not found (non-default install) | search the install folder too | done before | fix-codec-path-030 (82a73a613) | Windows install | 279-codecs-path.md |
| 180 | Astrometa DVB-T/2 not OK | TV with an RTL2832U+MN88473 stick | handed to the TV-tuner agent (owner decision) | - | - | - |
| 96 | DVB-T/T2 8 MHz with Pluto over IP | lock 8 MHz on Pluto | partly (Link=Ethernet before); stock Pluto USB link cannot carry 9.1 Msps | 62f5c30b (Link setting) | Pluto+ on Ethernet | 96-pluto-8mhz.md |
| 277 | Analogue TV TX slow | 3-4 s per second of signal | partly | 9a8c4d70f (cherry-pick of fix-issues2-030) | reporter's CPU | 277-atv-tx-slow.md |
| 275 | CDR decode failed | sample on WeTransfer | needs owner: the WeTransfer page needs JavaScript to download (not circumvented); the owner downloads it by hand into scratchpad/issue-275/ | - | the recording | 275-cdr-decode.md |
| 254 | DTMB radio choppy | stutter on AirSpy R2 | done before (screenshot: load 119%, ring overflow = CPU; faster/GPU LDPC in 3.0) | a2c563273, a158ae625, fix-dtmb-030 | reporter's PC | 254-dtmb-choppy.md |
| 184 | DTMB 690 MHz no decode | AVS+ video | done before (AVS+ add-on) | d7e578c62 | the sample | 184-dtmb-690.md |
| 262 | DAB tuning fault | click/drag in wide capture | done before | 48ccfb1f4 | LibreSDR | 262-dab-tuning.md |
| 249 | DAB transmitter list | TII list + map | partly (list, import/export; no free German site list) | 153a088dd, bd3e45bf4 | - | 249-dab-tii.md |
| 215 | Auto search bricks unattended devices | never touch unselected network devices | done before (guard test test_pluto_probe) | 78eb978f2, 8a1fcc665 | - | 215-pluto-network.md |
| 238 | Zoom reset on frequency change | keep zoom | done before | 94728cdf6 | - | 238-zoom-reset.md |
| 230 | Spectrum frequency labels off | labels line up and clip | done before | cf3cbec20 | - | 230-spectrum-labels.md |
| 257 | KiwiSDR "stopped sending samples" | Kiwi stream | done before | 3fd0c8578 | real Kiwi on Windows | 257-kiwi.md |
| 267 | TETRA echoing audio | one carrier/slot to the codec | done before | #259 (69cef5260), tetra-echo-030 | real TETRA | 267-tetra-echo.md |
| 252 | Enable DMR audio | mbelib on Linux | fixed now (help showed codec2 state) | 5a302eac1 | Linux with libmbe | 252-dmr-audio.md |
| 178 | DVB-T2 mux label vs DTMB | DTMB, not DVB-T, everywhere | fixed now (spectrum labels, DS numbering, band strip) | 8faf5a804, 1307e03b8 | GUI screenshot | 178-dtmb-labels.md |
| 148 | Airband channels not saved / cannot delete | add, edit, delete reliably | fixed now (stuck rename field) | 6ae5f0b88 | GUI interaction | 148-airband-save.md |
| 104 | assert() in debug mode | ImGui asserts in Debug builds | partly (recoverable errors logged) | bfe542fbb | MSVC Debug build | 104-debug-asserts.md |
| 166 | Crash clicking video services | crash on service select | done before | 0cb10c780 | - | 166-tv-service-crash.md |
| 210 | DAB+ crash on RTL-SDR (mac) | crash | done before | ed4c053e8 | - | 210-dab-rtl-crash.md |
| Discord | VE3SCP: libmbe-neo DMR plug-in freezes and closes OnAir | a voice codec must never take the app down | fixed now (mbelib-neo's argument layout detected and used; codec guard thread: a crash or hang disables the codec with a message; onair_speech_abi handshake; wrapper for both libraries; docs) | 831b6f044 | real mbelib-neo and a DMR signal; the Windows guard path is only built | discord-libmbe-neo.md |
| Discord | ar8001: arm64 plug-ins "incompatible architecture" on an M4 | the Intel OnAir under Rosetta; say so, show architectures | fixed now (plain-words mismatch text, Mach-O arch per plug-in, Rosetta notice + download button, universal build tip). Release ships OnAir-<v>-macos-arm64.dmg and -macos-x86_64.dmg (two thin builds, no universal): the pipeline should name them "Apple Silicon" / "Intel" | 0750fc5da | the user's Mac; UI only syntax-checked | discord-codec-arch.md |
| 112 | DVB-C subtitles broken | subtitles | done before | 49798de36, e613af745 | - | 112-dvbc-subs.md |

Counts (29 rows): done before 16, fixed now 7 (284, 226 with the ggml fix before, 252, 178, 148, 228 with the hang fix before, Discord libmbe-neo), partly 4 (96, 277, 249, 104), needs owner 1 (275 download), handed over 1 (180).
