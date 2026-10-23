# Audit: desktop vs Android Results tab, 108 modes

Read-only audit, 2026-10-16. Desktop column is inferred from `app/<mode>/*_ui.cpp` plus `core/include/dect2/*_tel.h` (keyword survey: tables, plots, images, maps, audio); it is not a line-by-line read of every UI. "Table missing" means the desktop has a list the phone cannot show.

## 1. How Android turns telemetry into the Results tab today

- Path: core `RxTelemetry` -> `native/src/results_json.cpp::resultsJson()` -> one JSON string via `OnAirNative.nativeTelemetry(force)` (poll, empty when nothing new) -> `Telemetry(raw: JSONObject)` in Receiver.kt -> `ResultsTab`.
- JSON shape: common header from `rxTelemetryJson` (seq, standard, state, locked, snrDb, "details" for only 8 standards: DVB-T/T2 family, ATSC, ISDB-T, DVB-S, DTMB, spectrum), plus `summary` (core `modeSummary()`, covers about 70 standards), plus `cards` [[k,v]...] and `tables` [{name, cols, rows}] (all values strings).
- Kotlin side is fully generic: `ResultsTab` draws summary text, 2-column cards, and horizontally scrolling tables (`ResultTableView`, column widths by header name). Two special panels: FM (`FmPanel`: PS, RadioText, PTY, PI, stereo, RDS) and General (`GeneralPanel`, with controls). No plots, images, maps, or text-log composables exist. `details` is parsed nowhere on Kotlin side.
- Per-mode content is only in C++: `results_json.cpp` has exporters for 7 standards: ADS-B (cards + 12-col aircraft table, 120 rows), AIS (vessels), ACARS (messages + aircraft), pagers (POCSAG/FLEX messages), packet/APRS (stations + frames), FM, General. Every other mode (about 100 of 108) shows only the one-line summary plus header lock/SNR.
- Audio: not via Kotlin. The core's own AudioOut (miniaudio -> AAudio) plays whatever the engine demodulates; `nativeSetVolume` only sets level/mute. FFmpeg entry points are stubbed in `stubs_ext.c` (return NULL/-1), so every FFmpeg-dependent decoder (DVB/ATSC/ISDB video+audio, DAB+/DRM/HD Radio AAC) cannot play. Patented vocoders (mbelib/AMBE/IMBE) are not built in (README), so DMR/NXDN/P25/dPMR/D-STAR/TETRA voice is silent.
- Telemetry carrying pixels exists only for ATV (`rgba`), Meteor LRPT, HRPT, GOES/Elektro, Marine (wefax/image), HDR art; none is exported to JSON.
- Key leverage: adding a table for a mode is a C++-only change (about 20-40 lines in `results_json.cpp`, no Kotlin edit), because the Kotlin view is generic. Plots, images, maps need new Kotlin composables and new JSON/bitmap channels.

## 2. Mode table

Effort: S = add cards/tables in results_json.cpp only (under 1 h each for pairs of modes, about 20 min each); M = new Kotlin composable and JSON/bitmap channel; L = needs a native feature (FFmpeg for Android, vocoder policy, radio feature, workbench UI). "-" = nothing to do.

| # | Mode | Group | Desktop shows | Android now | Gap | Effort |
|---|---|---|---|---|---|---|
| 0 | DVB | TV | video+audio (FFmpeg), constellation/spectrum plots, service list, lock/SNR/BER cards (some modes: service table, EPG) | summary line only (+ header lock/SNR) | video+audio missing (FFmpeg stubbed); plots missing; service table missing | L |
| 1 | ATSC | TV | video+audio (FFmpeg), constellation/spectrum plots, service list, lock/SNR/BER cards (some modes: service table, EPG) | summary line only (+ header lock/SNR) | video+audio missing (FFmpeg stubbed); plots missing; service table missing | L |
| 2 | ATSC 3.0 | TV | video+audio (FFmpeg), constellation/spectrum plots, service list, lock/SNR/BER cards (some modes: service table, EPG) | summary line only (+ header lock/SNR) | video+audio missing (FFmpeg stubbed); plots missing; service table missing | L |
| 3 | ISDB-T | TV | video+audio (FFmpeg), constellation/spectrum plots, service list, lock/SNR/BER cards (some modes: service table, EPG) | summary line only (+ header lock/SNR) | video+audio missing (FFmpeg stubbed); plots missing; service table missing | L |
| 4 | DVB-S/S2 | TV | plots (constellation, spectrum), 9 table items, video/audio | summary line only | plot missing; table missing; video+audio missing (FFmpeg) | L |
| 5 | DTMB | TV | constellation/SNR plots, lock cards, stream info (no video decode) | summary line only | plot missing, cards missing | M |
| 6 | DVB-C | TV | video+audio (FFmpeg), constellation/spectrum plots, service list, lock/SNR/BER cards (some modes: service table, EPG) | summary line only (+ header lock/SNR) | video+audio missing (FFmpeg stubbed); plots missing; service table missing | L |
| 7 | Analog TV | TV | live picture (rgba frame), 21 plot items, audio, line/lock cards | summary line only | image missing; audio path exists in core but no picture; plots missing | L |
| 8 | MAC | TV | MAC video frames, plots, audio | summary line only | image missing; plot missing | L |
| 9 | DAB / DAB+ | BROADCAST RADIO | service/ensemble table, DLS text, slideshow image, audio (AAC via FFmpeg) | summary line only | table missing; audio missing (FFmpeg stub); image missing | L |
| 10 | FM | BROADCAST RADIO | PS, RadioText, PTY, RDS, stereo, spectrum + audio | FmPanel (PS, RT, PTY, PI, stereo, RDS) + cards + core audio | none (no RDS group stats, no deviation plot) | S |
| 11 | DRM | BROADCAST RADIO | audio (xHE-AAC), MSC plots, service table, text messages | summary line only | audio missing (FFmpeg stub); plot missing; table missing | L |
| 12 | HD Radio | BROADCAST RADIO | program/service table, art image, audio, SIS text | summary line only | table missing; audio missing; image missing | L |
| 13 | CDR | BROADCAST RADIO | service/lock plots, audio | summary line only | plot missing; audio missing | M |
| 14 | ADS-B | AVIATION | aircraft table, map, range plot, stats | cards + aircraft table (12 cols)  (map editing in progress by another agent) | map missing (in progress); range plot missing | M |
| 15 | Mode S uplink | AVIATION | uplink interrogation table, map | summary line only | table missing | S |
| 16 | ACARS | AVIATION | message + aircraft tables, map | message table + aircraft table + cards | map missing | S |
| 17 | VDL2 | AVIATION | message/link table, aircraft map | summary line only | table missing; map missing | S |
| 18 | HFDL | AVIATION | ground station/aircraft table, message list, map | summary line only | table missing; map missing | S |
| 19 | Airband | AVIATION | channel table, plots, AM audio | summary line only + core audio if source covers band | table missing; plot missing | M |
| 20 | VOR | AVIATION | bearing/radial readout, plot, map | summary line only | cards missing; plot missing | S |
| 21 | ILS | AVIATION | localizer/glideslope DDM readout, plot | summary line only | cards missing; plot missing | S |
| 22 | NDB | AVIATION | NDB ident table, audio | summary line only | table missing | S |
| 23 | Drone Remote ID | AVIATION | drone table, map | summary line only | table missing; map missing | S |
| 24 | AIS | MARITIME | vessel table, map | cards + vessel table (7 cols) | map missing | M |
| 25 | VDES | MARITIME | VDES frame/message tables | summary line only | table missing | S |
| 26 | NAVTEX | MARITIME | decoded NAVTEX/DSC/WEFAX: message table, fax image (wefax) | summary line only | table missing; image missing (wefax) | M |
| 27 | DSC | MARITIME | DSC call table, map | summary line only | table missing | S |
| 28 | Weather fax | MARITIME | fax image (progressive), controls | summary line only | image missing | M |
| 29 | GNSS | SATELLITE &amp; NAVIGATION | sky plot, satellite table, fix, map | summary line only | table missing; plot missing; map missing | M |
| 30 | Inmarsat-C | SATELLITE &amp; NAVIGATION | Inmarsat-C message and station tables | summary line only | table missing | S |
| 31 | Inmarsat Aero | SATELLITE &amp; NAVIGATION | Aero channel/message tables, map | summary line only | table missing | S |
| 32 | Iridium | SATELLITE &amp; NAVIGATION | burst/frame tables, map | summary line only | table missing | S |
| 33 | Meteor-M LRPT | SATELLITE &amp; NAVIGATION | LRPT weather image (growing), packet counters | summary line only | image missing | M |
| 34 | GOES HRIT | SATELLITE &amp; NAVIGATION | GOES HRIT file list + images | summary line only | image missing; table missing | M |
| 35 | Elektro-L | SATELLITE &amp; NAVIGATION | Elektro-L images, file table | summary line only | image missing; table missing | M |
| 36 | HRPT / AHRPT | WEATHER &amp; SCIENCE | HRPT/AHRPT image, plots | summary line only | image missing; plot missing | M |
| 37 | Orbcomm | SATELLITE &amp; NAVIGATION | Orbcomm packet table, map | summary line only | table missing | S |
| 38 | QO-100 | SATELLITE &amp; NAVIGATION | QO-100 beacon plots, image, audio | summary line only | plot missing; image missing; audio missing | M |
| 39 | FLARM | AVIATION | FLARM table, map | summary line only | table missing; map missing | S |
| 40 | DJI drones | AVIATION | DJI table, plots, image | summary line only | table missing | S |
| 41 | CubeSat telemetry | SATELLITE &amp; NAVIGATION | CubeSat frame/telemetry tables | summary line only | table missing | S |
| 42 | Globalstar | SATELLITE &amp; NAVIGATION | Globalstar frame table | summary line only | table missing | S |
| 43 | DMR | TWO-WAY RADIO | call/talkgroup table, text, plots, audio (AMBE: mbelib) | summary line only | table missing; audio missing (patented vocoder not shipped) | M |
| 44 | NXDN | TWO-WAY RADIO | call table, audio (AMBE) | summary line only | table missing; audio missing | M |
| 45 | P25 | TWO-WAY RADIO | call/talkgroup table, control events, audio (IMBE) | summary line only | table missing; audio missing | M |
| 46 | dPMR / PMR446 | TWO-WAY RADIO | call table, audio | summary line only | table missing; audio missing | M |
| 47 | TETRA | TWO-WAY RADIO | call/burst tables, audio (ACELP) | summary line only | table missing; audio missing | M |
| 48 | TETRAPOL | TWO-WAY RADIO | burst/call tables | summary line only | table missing | S |
| 49 | Cell towers | CELLULAR &amp; RAIL | cell table, map | summary line only | table missing | S |
| 50 | 5G NR cells | CELLULAR &amp; RAIL | NR cell table, map | summary line only | table missing | S |
| 51 | NB-IoT cells | CELLULAR &amp; RAIL | NB-IoT cell table | summary line only | table missing | S |
| 52 | GSM-R | CELLULAR &amp; RAIL | GSM-R call/burst tables | summary line only | table missing | S |
| 53 | LBJ | CELLULAR &amp; RAIL | LBJ train message table | summary line only | table missing | S |
| 54 | POCSAG | PAGERS | message table | cards + message table | none | - |
| 55 | FLEX | PAGERS | message table | cards + message table (shared with POCSAG) | none | - |
| 56 | ISM sensors | IoT &amp; SENSORS | sensor device table (rtl_433 style), plots | summary line only | table missing | S |
| 57 | LoRaWAN | IoT &amp; SENSORS | LoRaWAN frame table | summary line only | table missing | S |
| 58 | Meshtastic | IoT &amp; SENSORS | mesh node + message tables, map | summary line only | table missing; map missing | S |
| 59 | MeshCore | IoT &amp; SENSORS | mesh node + message tables, map | summary line only | table missing; map missing | S |
| 60 | Bluetooth LE | IoT &amp; SENSORS | BLE device table | summary line only | table missing | S |
| 61 | Zigbee / 802.15.4 | IoT &amp; SENSORS | Zigbee frame table | summary line only | table missing | S |
| 62 | Wi-Fi HaLow (802.11ah) | IoT &amp; SENSORS | HaLow frame table | summary line only | table missing | S |
| 63 | Datawell Buoy HF Link | HF UTILITY | Datawell packet table, audio | summary line only | table missing | S |
| 64 | CODAR / HF ocean radar | HF UTILITY | CODAR sweep table, plots | summary line only | table missing; plot missing | M |
| 65 | Multitone Paging | PAGERS | multitone page table, audio | summary line only | table missing | S |
| 66 | NOAA Weather Radio SAME | WEATHER &amp; SCIENCE | SAME alert table | summary line only | table missing (alerts invisible apart from count) | S |
| 67 | ANT+ sensors | IoT &amp; SENSORS | ANT+ sensor table, plots | summary line only | table missing | S |
| 68 | DECT cordless phones | IoT &amp; SENSORS | DECT table | summary line only | table missing | S |
| 69 | LoRa (raw) | IoT &amp; SENSORS | raw LoRa frame table | summary line only | table missing | S |
| 70 | Wi-Fi networks | IoT &amp; SENSORS | AP/network table | summary line only (needs Wi-Fi radio source) | table missing; needs radio feature not on phone | L |
| 71 | Radiosonde | WEATHER &amp; SCIENCE | sonde table, map, plots | summary line only | table missing; map missing | M |
| 72 | Time signals | WEATHER &amp; SCIENCE | time-signal decode, plots, audio | summary line only | cards missing; plot missing | S |
| 73 | Ionospheric sounders | WEATHER &amp; SCIENCE | ionogram plots, tables | summary line only | plot missing; image missing | M |
| 74 | Radio astronomy | WEATHER &amp; SCIENCE | drift-scan plots, image | summary line only | plot missing | M |
| 75 | FT8 | AMATEUR - DIGITAL | decode list (FT8) | hfdig: summary only | table missing (decode list) | S |
| 76 | FT4 | AMATEUR - DIGITAL | decode list, waterfall | summary line only | table missing (decode list) | S |
| 77 | FT2 | AMATEUR - DIGITAL | decode list, waterfall | summary line only | table missing (decode list) | S |
| 78 | WSPR | AMATEUR - DIGITAL | spots list, map | summary line only | table missing | S |
| 79 | JT65 | AMATEUR - DIGITAL | decode list, waterfall | summary line only | table missing (decode list) | S |
| 80 | JT9 | AMATEUR - DIGITAL | decode list, waterfall | summary line only | table missing (decode list) | S |
| 81 | Q65 | AMATEUR - DIGITAL | decode list, waterfall | summary line only | table missing (decode list) | S |
| 82 | FST4 | AMATEUR - DIGITAL | decode list, waterfall | summary line only | table missing (decode list) | S |
| 83 | MSK144 | AMATEUR - DIGITAL | decode list, waterfall | summary line only | table missing (decode list) | S |
| 84 | PSK31 | AMATEUR - DIGITAL | decoded text, plots | summary line only | text list missing | S |
| 85 | JS8Call | AMATEUR - DIGITAL | decoded text list | summary line only | text list missing | S |
| 86 | RTTY | AMATEUR - DIGITAL | decoded text, spectrum | summary line only | text list missing | S |
| 87 | CW | AMATEUR - DIGITAL | decoded text, plots | summary line only | text list missing | S |
| 88 | SSTV | AMATEUR - DIGITAL | live SSTV image + text | summary line only | image missing | M |
| 89 | HamDRM | AMATEUR - DIGITAL | decoded text/image, constellation | summary line only | text list missing; image missing | M |
| 90 | Olivia | AMATEUR - DIGITAL | decoded text, plot | summary line only | text list missing | S |
| 91 | Contestia | AMATEUR - DIGITAL | decoded text, plot | summary line only | text list missing | S |
| 92 | MFSK | AMATEUR - DIGITAL | decoded text, plot | summary line only | text list missing | S |
| 93 | THOR | AMATEUR - DIGITAL | decoded text, plot | summary line only | text list missing | S |
| 94 | DominoEX | AMATEUR - DIGITAL | decoded text, plot | summary line only | text list missing | S |
| 95 | Hellschreiber | AMATEUR - DIGITAL | decoded text, plot | summary line only | text list missing | S |
| 96 | FreeDV | AMATEUR - VOICE &amp; PACKET | voice audio (codec2), call text | summary line only | audio missing (codec2 not shipped); text missing | M |
| 97 | M17 | AMATEUR - VOICE &amp; PACKET | call table, text, map, audio (codec2) | summary line only | table missing; audio missing | M |
| 98 | D-STAR | AMATEUR - VOICE &amp; PACKET | heard-call table, text, map, audio (AMBE) | summary line only | table missing; audio missing | M |
| 99 | APRS (AFSK) | AMATEUR - VOICE &amp; PACKET | station table, frame list, map | cards + station table + frame table | map missing | M |
| 100 | LoRa APRS | AMATEUR - VOICE &amp; PACKET | station + frame tables, map | summary line only | table missing; map missing | S |
| 101 | MeshCom | AMATEUR - VOICE &amp; PACKET | station + message tables | summary line only | table missing | S |
| 102 | HF ALE | HF UTILITY | ALE call/link table, audio | summary line only | table missing | S |
| 103 | HF beacons | HF UTILITY | beacon table, map, audio | summary line only | table missing | S |
| 104 | Signal workbench | GENERAL &amp; TOOLS | workbench plots, tools | summary line only | plot missing; needs workbench UI | L |
| 105 | General receiver | GENERAL &amp; TOOLS | demod/squelch controls, level, audio | GeneralPanel (demod, squelch, level) + core audio | none (no spectrum-zoomed tuning, no bandwidth/AGC controls) | S |
| 106 | Spectrum explorer | GENERAL &amp; TOOLS | spectrum explorer plots, signal table, image | summary line only | plot missing; table missing | M |
| 107 | Spectrum analyser | GENERAL &amp; TOOLS | spectrum analyser plots, markers | summary line only | plot missing | M |

## 3. Top 10 gaps by user value

1. Audio for broadcast and voice modes: DAB/DAB+, DRM, HD Radio, DVB/ATSC (FFmpeg stubbed; build FFmpeg for Android, L), then DMR/P25/NXDN/D-STAR/TETRA (patented vocoders, policy: user-installed library per docs/CODECS.md, L). FM, AM/Airband/General already play via core AudioOut.
2. Map for ADS-B, AIS, APRS, ACARS, sondes (in progress for ADS-B; needs a tile decision, M). Tables alone already work for ADS-B/AIS/APRS.
3. Decoded-text/decode lists for HF digital: FT8/FT4/WSPR/JS8/RTTY/PSK/CW/Olivia etc. (modes 75-95, about 21 modes) show only a summary; one shared S change in results_json.cpp (hfdig/psk/js8/cw telemetry) covers all.
4. Image channel (bitmap JSON or shared buffer): SSTV, Weather fax, Meteor LRPT, GOES HRIT, Elektro-L, HRPT, analog TV (M, 10 modes at once once the channel exists).
5. TV video (DVB/ATSC/ISDB/DTMB/DVB-S/C): needs FFmpeg for Android and a SurfaceView/Bitmap path (L). A cheaper first step: service table and lock/BER/MER cards (S).
6. Voice-network call tables: DMR, P25, NXDN, dPMR, TETRA (talkgroup/call/source tables; S each, no audio needed to be useful).
7. DAB service/ensemble table + DLS text and slideshow (S for table, M for slideshow).
8. Plot composable (line/XY from JSON arrays): constellation, spectrum, sky plot, ionogram (about 25 modes use plots; M).
9. SAME alerts, Mode S uplink, ACARS-adjacent (VDL2, HFDL, Inmarsat) lists: small S changes, high value for pager/aviation users.
10. IoT/ISM sensor and BLE/Zigbee tables (rtl_433-style sensors are very popular; S).

## 4. Recommended plan for one Sonnet agent, about 2 hours

Rule: keep Kotlin changes minimal (another agent is editing ResultsTab.kt); do everything in `native/src/results_json.cpp` plus a new Kotlin file for anything new.

1. (35 min) Add table/card exporters, copying the existing `Table`/`Out` helpers, for: HFdig/FT8-family decode list, PSK, JS8, CW (decoded text); VDL2, HFDL, Inmarsat-C/Aero, DMR, P25, NXDN, dPMR, TETRA, DSC/NAVTEX; ISM sensors, BLE, SAME, DAB service list. Read each `*_tel.h` for the fields; cap rows (60-120). Extend the switch in `resultsJson()` and the includes.
2. (10 min) Add column widths for any new wide column names in `colWidth()` (one-line edit; coordinate with the ResultsTab owner after they finish) or leave default.
3. (25 min) Render `details` cards for the DVB/ATSC/ISDB/DTMB family in Kotlin (a few lines in a new file, called from ResultsTab after the other agent is done), so lock/BER/MER show.
4. (30 min) Plot composable: export `plot` JSON ({name, x[], y[]}) for constellation/spectrum of the TV modes and GNSS sky; draw with Canvas in a new `PlotView.kt`.
5. (15 min) Build (`tools/build_native.sh`), run `tools/check_jni.py`, load each mode with the test signal and confirm the tables render, using screens/ screenshots (downscale before reading).
6. Not in the 2 h: image channel, maps, FFmpeg, vocoders (state them as follow-ups).

## 5. Done 2026-10-16 (results_json.cpp)

Tables/cards added: DMR, P25, NXDN, dPMR, D-STAR, TETRA, HFDL, VDL2, Inmarsat Aero, Inmarsat-C, DAB (service table, via a DabEnsemble passed from nativeTelemetry), SAME, BLE, ISM, Iridium, Meshtastic/mesh, HF digital (FT8-family decode list, RTTY and keyboard-mode text), PSK31, CW, JS8Call; lock/SNR/MER/error cards for DVB-T2, DVB-T, ATSC, DVB-S/S2, DTMB, DVB-C (done in C++, not Kotlin). ISDB-T cards, plots and details-in-Kotlin not done.

Follow-ups: ISDB-T/ATSC 3.0 cards; Plot composable (needs constellation/spectrum arrays in JSON); image channel (SSTV, wefax, LRPT, HRPT, ATV); maps; FFmpeg audio/video; vocoders; remaining S-effort tables (Mode S uplink, DSC, NAVTEX, VDES, GNSS, Orbcomm, cell/NR/NB-IoT, GSM-R, LBJ, Zigbee, HaLow, LoRa raw, LoRaWAN, ANT+, DECT, FLARM, DJI, RID, CubeSat, Globalstar, ALE, HF beacons, Datawell, multitone, sonde, time signals, VOR/ILS/NDB); hfdig decode table also shows FT8 decodes while RTTY is active.
