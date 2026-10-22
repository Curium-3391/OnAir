# Idle and background power use

Why: OnAir drained Mac batteries. This page covers what the app does when no radio runs, or when the window is minimised, and what was changed.
Scope of 0.3.0 work item `perf-power-030`. The drawing loop and the DSP kernels are covered elsewhere.

## Policy (core/include/dect2/power/power.h)

| Setting (View > Power) | Values | Default |
|---|---|---|
| Low power mode | off / automatic / always | automatic: on when running on battery or when the machine is hot |
| Save power when hidden | off / on battery / always | on battery |

* Battery: macOS `IOPSGetProvidingPowerSourceType` (IOKit), Windows `GetSystemPowerStatus`, Linux `/sys/class/power_supply`.
  Heat: macOS `NSProcessInfo.thermalState` serious or critical. Read at most every 20 s, cached, no thread.
* Low power mode: spectrum / telemetry updates 30 per second become 15, the analysis loop polls its ring every 4 ms instead of 2 ms, scanner dwell
  waits poll every 25 ms instead of 10 ms. `maxFftSize()` and `spectrumRateScale()` are there for the drawing code.
* Save power when hidden (window minimised, via the GLFW iconify callback): spectrum / telemetry go to 5 per second, scans wait until the window is
  back. Receivers, decoding and audio are NOT stopped (a minimised window with the radio playing is a normal use). Stopping receivers is open.
* App Nap: an activity assertion (`NSActivityUserInitiatedAllowingIdleSystemSleep`) is held only while an engine runs (`power::ReceivingActivity`
  in `Engine::start` / `stop`). With no receiver running macOS may nap the idle app.
* Thread QoS: `namedThread()` now sets the macOS QoS class from the thread name: log, scan, msgdb, feed, wspr, psk, discord, upload, tile, sweep,
  survey, update, hist threads run as `QOS_CLASS_BACKGROUND`; `-rx`, `-dec`, `-dsp`, `-fft`, `-spec`, `-pool`, `-work` threads as
  `QOS_CLASS_UTILITY`. Windows: below-normal priority for the background ones. Linux: unchanged. Raw `std::thread` users are not covered.

## Measured idle wakeups

Harness: `tests/power/test_power_idle.cpp` (ctest `power_idle`). It switches on the share server and the ADS-B feed servers (and the remote API in
the full build) with nobody connected, then counts wakeups with the OS: macOS `task_power_info` interrupt wakeups, Linux voluntary context switches
of all threads. It fails above 40 per second. `DECT2_IDLE_SECONDS` sets the measuring time (default 3).

Measured on an Apple Silicon Mac, standalone build without the remote API (needs the Rust net crate), 3 s windows:

| Case | Before | After |
|---|---|---|
| share server + feed servers listening, nobody connected | 49.7 / s | 7.0 / s |

Per thread (from the code, not separately measured): remote API accept loop 20/s to 4/s when no connection is open; share server `select` 50/s to
4/s without clients; feeds thread 25/s to 4/s; push-feed targets thread 25/s to 4/s; rotator thread 20/s (50 ms sleep) to 2/s (condition variable,
wakes on a queued command or stop, position poll every 500 ms as before); msgdb writer 2/s to 0 (plain wait); WSPRnet uploader 4/s to 0.03/s when
the queue is empty; Discord presence thread 5/s to 1/s (only while Rich Presence is on).
The first-listed threads exist only while their feature is switched on; with everything off the app has no polling thread of this kind.

## Audit: what else polls (not changed)

* `AudioOut` opens the device in `start()` and keeps it open while the receiver runs (silence when squelched). Receivers start it, the engine
  stop closes it. Closing it after N seconds of silence is open.
* `source_audio.cpp` 1 to 2 ms ring sleeps, `source.cpp` 1 ms sleeps, `native_common.h` 10 ms: only while a source streams.
* `scanner.cpp` 10 to 20 ms: only during a scan.
* `app/rfi/rfi_ui.cpp` 200 ms read loop for a GPS: only while a GPS is open. `rooms_send.cpp` 100 ms sleeps for a stop flag: only while sharing.
* `tv/player.cpp` 20 to 100 ms condition waits: only while playing.
* `analysisLoop` runs at realtime priority while receiving; the 2 ms ring poll remains (a blocking ring would be the fix, it is in the sample
  path owned by the DSP work).
* Not yet reviewed: map tile loaders (`adsb_map.cpp` 50 ms stop polling during a download), update checks, whisper threads.

## Open

* Stop receivers when hidden (opt-in); close idle audio devices; blocking ring in the analysis loop.
* Wire `spectrumRateScale()` / `maxFftSize()` into the drawing code (owned by perf-ui-030): waterfall rate and FFT size in Low power mode.
