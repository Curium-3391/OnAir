> Historical: the plan of the 0.2.6 app, kept for reference. The 0.3.0 rework (android/ in the OnAir repo) is described in ARCHITECTURE.md.

# OnAir for Android: state and plan (0.2.6)

Not part of the OnAir git repo; never pushed. Receive only: no transmit code, no map-server upload.

## What this is
- `native/` builds `libonair.so` = `dect2core` (the desktop receiver library, built from a worktree of the OnAir repo on branch
  `android-app-0.2.6`) + `src/jni_onair.cpp` (the bridge) + `src/results_json.cpp` (cards and tables of the active mode as JSON)
  + `src/stubs_ext.c` (stand-ins for libusb, libhackrf and FFmpeg entry points the core links to). `tools/build_native.sh` builds,
  strips (about 15 MB) and copies it to `app/src/main/jniLibs/arm64-v8a/`. The old per-mode contract is in `native/_superseded/`.
- `app/` is Kotlin + Jetpack Compose, minSdk 28, one permission (INTERNET). Files come through the system picker as `/proc/self/fd/N`.
- `tools/gen_modes.py` regenerates `Modes.kt` and `res/values/modes.xml` from the desktop's `kModes` table (app/toolbar.cpp).
  `tools/check_jni.py` checks the Kotlin `external fun`s against the C++ `FN(...)` signatures.

## Phase 1 (this release)
Sources: test signal, rtl_tcp (the "SDR driver" app serves a USB RTL-SDR on 127.0.0.1:1234), SpyServer, raw IQ files.
Audio: the core's own AudioOut (miniaudio, which uses AAudio on Android). Results: generic cards/tables for every mode, dedicated
panels for FM and the general receiver, tables with the desktop's column names for ADS-B, AIS, ACARS, pagers and APRS.

## Phase 2: native USB (not started)
1. `UsbManager` permission flow in Kotlin (device filter for RTL-SDR 0bda:2832/2838, HackRF 1d50:6089, Airspy 1d50:60a1, HF+ 03eb:800c).
2. Hand the opened file descriptor to native code; build libusb 1.0.27 for Android (it supports `libusb_wrap_sys_device` on a fd and
   `LIBUSB_OPTION_NO_DEVICE_DISCOVERY`) and replace the stubs in `stubs_ext.c` for libusb.
3. The core's own HackRF driver (`hackrf_usb.cpp`) already talks libusb: wrap the fd there. RTL-SDR: build librtlsdr (fd variant) and use
   the core's `source_rtlsdr.cpp`; Airspy and Airspy HF+ the same way.
4. Foreground service while receiving (screen off, audio continues), notification with stop.
5. Then: map for ADS-B/AIS/APRS (needs tiles: decide offline vs online first), FM Mono switch (needs a core setter), SigMF/WAV through the
   picker (the sidecar `.sigmf-meta` needs a second picked file), FFmpeg for Android (TV pictures, DAB+/AAC), x86_64 build for Intel emulators.
