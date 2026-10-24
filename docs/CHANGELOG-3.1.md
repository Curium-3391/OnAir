# OnAir 3.1

3.1 follows 3.0. It is mostly fixes, a calmer interface and a lot of speed work.

**Measured** numbers below come from OnAir's own test signals (noise, carrier offset, clock error, echoes, fading), not from over-the-air recordings, unless a line says so. CPU numbers are for the receiver alone on a PC-class server: "CPU-s/s" is CPU seconds per second of signal, so 0.10 means one tenth of one core.

## Fixes

- **Live transcription works from the start.** It was off by default in FM and the other modes, the music filter threw away FM speech, language "auto" returned no text, the default model was missing, and the program could crash on quit while Whisper was running. It now picks the best installed model and tells you in plain words what it is doing ("Listening for speech", "Model missing...").
- **The spectrum axis shows real frequencies** (MHz) instead of offsets such as 0, 1, 2, -1, -2 M.
- **Teletext** no longer shows the red "conflicting ID" box (the Fastext buttons shared one ID).
- **Panel polish:** no gap under the Gain and Source headers, the Amp warning wraps, "Clear all" and the top-bar buttons stay inside the window, the volume row icons, and "Save bug report" wraps. The Listen options have clearer names with tooltips (Skip silence, Keep recording, Hear this channel, Outputs...).
- **Selected tab** is a blue fill with no white bar. The RX / TX / Ham switch is now flat tabs in the same blue.
- **Aircraft map:** the toolbar wraps and the map fills the panel.
- **3D spectrum:** the 3D button works, and there is a 2D button and a Reset view. The TV player controls are grouped into transport, audio and record rows.
- **FT8 on several bands:** PSK Reporter and WSPRnet now get every band's spots with that band's dial frequency; the Band and Dial MHz columns are filled in.
- **Red Pitaya:** digital gain 0 to +40 dB, with a note that it has no analog gain.
- **Windows:** the server no longer crashes at start (#291); the status tiles scale correctly at 125 to 200 %.
- **Reports from users:** extra gain stages for Airspy and SDRplay (#233), a Hellschreiber font file option (#225), Whole span for VDL2 and HFDL (#236). New environment variable ONAIR_NO_CRASH_REPORT.
- **World map (map.on-air.dev):** icons no longer disappear when you zoom. There is an optional "Your name on the map" with "Last heard by" and a user search (needs the 3.1 app to send names).

## New

- **Auto gain redesign:** targets depend on the band, a quick 4 to 6 point search runs at start, retune and "Re-optimise", the result is remembered per radio and band, and "Why this gain?" explains it. In simulation it lands 0.82 dB from the best gain on average (75 runs).
- **Whole span for DVB-S and DVB-S2:** finds every transponder, runs one decoder each and lists them in a table. The DVB-T/T2 switch is renamed Whole span and is always visible.
- **onair-server is in every download:** the macOS app bundle, the .deb, the portable Linux tarball (with a systemd unit that is not enabled), and the Windows zip and installer (with a Start-menu entry).
- **Interface:** a simple tuner look (red centre line, grey limit lines; Classic is still selectable), an optional Unified layout (spectrum and waterfall always visible), two optional looks (Dev A graphite, Dev B terminal), and a shared OnAir UI kit. Layout and Look both default to Classic.
- **ADS-B** repairs up to 3 uncertain bits (correction level 2, now the default).
- **Directory and "How to start"** at web.on-air.dev.

## Faster and more sensitive

Faster means less CPU for the same signal:

- **Start-up:** the resampler tables are built when first needed. At 20 Msps the start takes 0.15 s instead of 9.9 s of CPU and uses 460 MB instead of 1020 MB.
- **KAVB** 3.38 to 1.40 CPU-s/s. **Zigbee** 0.59 to 0.38, **DECT** 0.35 to 0.28, **BLE** 0.33 to 0.22, **ANT+** 0.32 to 0.22, **Wi-Fi** 1.21 to 1.03, **Remote ID** 1.82 to 1.61.
- **Z-Wave** at 2 Msps 0.233 to 0.027, at 4 Msps 0.452 to 0.049. **Globalstar** at 10 Msps 0.125 to 0.090. **Iridium** burst detector 0.086 to 0.068 with the same bursts found.
- **HFDL** 0.645 to 0.170 (2 Msps) and 1.305 to 0.280 (4 Msps). **VDL2** 0.26 to about 0.11 (2 Msps) and 0.42 to about 0.14 (4 Msps).
- **DVB-S** 0.42 to 0.29, **DVB-S2** receiver thread 0.36 to 0.22; a whole DVB-S2 8PSK 2/3 decode 2.08 to 1.64, 32APSK 4.13 to 2.80 (still more than one core at 10 Msps).
- **ATSC 1.0** 2.89 to 1.94 and a faster DVB-T Viterbi (AVX2).
- **DVB-T2:** AVX2 LDPC, demapper and resampler. A 40 Mbit/s 32K 256-QAM signal goes from 1.54 to 1.20 (-22 %), a mid-size 8K signal from 0.88 to 0.74. Locking on a signal with a strong echo is much faster (0.35 s instead of 1 to 2 s) and such a signal now decodes down to about 20 dB instead of only above 30 dB.
- **FT8:** a busy 40-signal slot costs 1.12 instead of 3.22 CPU-s, with the same decodes. **Multi-band HF digital monitoring** and **LoRa** use one shared channeliser from three channels on.
- **DAB** 0.131 to 0.120 at 2 Msps (bit-exact output).
- The faster code paths for x86 switch on only when the CPU supports them.

More sensitive (90 % decode threshold, noise in the channel, synthetic):

- NXDN48 9 to 8 dB, M17 6 to 4 dB, APRS (packet) about 7 to 5 dB.
- ADS-B share decoded at 8 to 11 dB 37.3 % to 43.3 % in a 60 s simulation, with no false decodes.

## Known limits

- **Not tried on real radios:** auto gain, transcription, the FT8 multi-band fix, Red Pitaya gain, the map name feature, and the speed and sensitivity work above.
- New strings are English only.
- DVB-T/T2/S2/ATSC, DAB and very wide (32 Msps) captures are still heavy on the CPU. DVB-T2 256-QAM needs about 3 to 5 dB more signal than it should; DVB-S2 needs more than one core at 10 Msps.
- The M17 and NXDN narrower filters were not tested with carrier errors above about 3 kHz. D-STAR and P25 need a front end that follows the carrier offset to get the same gain (3.2).
- Some CPU figures were single runs on a shared server, so expect about 10 % noise. Compared with 0.2.8, some modes in 3.0 used 7 to 30 % more CPU (DVB-C, DVB-S, FM); the cause is not found yet.
- The Nix package is not part of 3.1 (its check does not pass yet).

## Thanks

drelectro (#292 P25 tune offset), ffishman (#286 Brazil band plans), vlig-alt (#290 DMR LRRP and map), and everyone who filed the issues fixed here.
