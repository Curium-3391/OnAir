# Auto gain

One system that sets the gain of every radio for every mode. Code: `core/include/dect2/source/autogain.h`,
`core/src/source/autogain_radio.cpp` (radio models, stage planner), `autogain_measure.cpp` (measurements, mode targets),
`autogain.cpp` (controller, memory, runner, engine adapter, JSON). App: `app/panels/autogain_ui.cpp`. Tests:
`tests/source/test_autogain.cpp` with the simulated radio `tests/source/autogain_sim.h`.

It replaces, in the toolbar, the old AGC checkbox (SmartAgc, `docs/auto-gain.md`), "Auto-tune" (GainSweep) and "Optimise gain"
(GainOptimiser). Those classes stay in core (their tests still run); the app no longer drives SmartAgc.

## Using it

The gain row has **Auto gain** with four choices and a gear:

| Choice | What it does |
|---|---|
| Off | the gain stays where you set it; the meters keep running |
| Auto | a slow continuous loop (below). Touching a gain control turns it off, as the AGC did |
| Optimise once | searches the gain range and the stage splits for the mode's best quality, then stops. About 15-40 s. Undo puts the old gain back |
| Safe start | starts at the lowest gain and ramps up until the noise floor rises by X dB, stopping before the ADC clips or intermodulation appears |

Under it, the **meter strip**: the ADC headroom bar (full scale at the right, a tick at the mode's target, green / amber / red / blue for
too quiet) with a clip lamp held for 2 s, the headroom in dB, the noise floor over the ADC's own noise, and the first warning in plain
language (all of them in its tooltip), for example *Strong signal nearby: reduce the gain or add a filter (band-pass, or an FM / GSM notch)*.

The **gear** opens the advanced popup: own headroom target, max gain, step size, loop speed, settle and measure times, Safe start rise,
the quality metric, which stages may be adjusted (per radio), the amp permission, the optional intermodulation probe for Auto, a history
graph of gain and quality, the log of changes, the remembered gains (with *Use the remembered gain* and *Forget*) and the radio's gain
chain with what each stage does.

Settings are saved (`ag.*` in the settings); the stage mask per radio (`ag.mask.<radio>`); the remembered gains in `ag.mem`.

## 1. Measurement

| Measurement | Source | Used for |
|---|---|---|
| peak, 99.9 % envelope (robust peak), rms, crest factor, clipped fraction | the engine's ADC statistics (`fromSignalStats`; `measureBlock` for raw IQ) | headroom (full scale over the robust peak), overload |
| noise floor per bin (median of the quietest half outside the channel), channel power, channel SNR, power outside the channel, strongest outside bin | the spectrum (`estimateFloor`) | noise floor over the ADC's quantisation noise, the channel's SNR (not the span's), strong neighbours |
| lock, SNR, MER, pre-FEC BER, FEC blocks decoded | the mode's telemetry (`qualityFrom`) | the metric, the lock freeze, the quality guard |
| intermodulation | two spectra 3 dB (or one sweep step) apart (`imdCompare`) | a third-order product rises about 3 dB per dB of gain, a real signal 1 dB per dB: a line that rises more than twice the gain change is a product of the front end |
| wideband power vs gain slope | the same two spectra, over the lines that were clear of the floor | less than 0.6 dB per dB: the front end compresses |

A line hidden in the ADC's own noise at the lower gain comes out of it faster than the gain (unmasking): it is not counted, and the power
slope ignores the ADC's own noise (which does not follow the gain).

## 2. Algorithms

**Auto** (`tickAuto`):

* **Overload** (clipping above the mode's limit, or a peak at full scale) cuts at once by 6 to 20 dB by how bad it is, after only the radio's
  own delay; a burst that takes several cuts is one episode. A soft ceiling 3 dB under the gain that clipped holds 20 s (90 s after three
  episodes in a minute); the split goes to linearity for 30 s. With the amp allowed, the amp goes off first.
* **Too hot** (headroom more than the dead band under the target): down by the error, every 0.8 s at most.
* **Room** (headroom more than the dead band over the target): up by at most the mode's step (4-6 dB) every 2.5 s, only while gain buys
  sensitivity: not when the noise floor is already 15 dB over the ADC's own noise ("gain wasted"); when it is 18 dB over, it comes down,
  but only if it stays above the "wasted" line after the step (no ping-pong on 6 dB-step radios).
* **Noise margin**: inside the dead band, a floor less than the mode's goal over the ADC's noise raises the gain 3 dB at a time.
* **Hysteresis**: right after coming down, going up needs 1.5 times the dead band for 8 s.
* **Lock freeze**: while a decoder is locked on good data it holds, except for overloads, a level too hot, or an ADC far too quiet.
* **Quality guard**: an upward step that made the decoder's MER / SNR / BER 1 dB worse is undone, with a ceiling for 60 s.
* **Stage policy**: the mode's (sensitivity, balanced, linearity), switched to linearity after an overload, near a strong neighbour or
  after intermodulation was seen; a policy change re-splits the stages at the same total.
* **Intermodulation probe** (optional, off by default): every 90 s a 3 dB step up and back; spurs or compression set a ceiling for 5 minutes.
* **Retune**: a new acquisition on the new frequency. The loop speed scales all hold and settle times.

**Optimise once** (`tickOnce`): about 10 gains over the range, ascending; each is measured for the measure time after the radio's settle
time plus the loop's settle time. A step that clipped, ran out of headroom or showed intermodulation against the step below drops every
higher step of that split. Then a refinement halfway to the neighbours of the best, and the other stage splits at the best total (radios with
several stages). The winner: the mode's metric (only if it was there for most of the step), else the channel SNR (or sensitivity: the floor
over the ADC's noise, worth nothing past goal + 6 dB), else the headroom closest to the target; within 0.5 dB of the best, the step whose
headroom is closest to the mode's target (less than the target counts double), then the lower gain. Nothing safe: the lowest gain.

**Safe start** (`tickSafe`): from the lowest gain in steps of 3 dB (or the step size); stops when the noise floor has risen the mode's
X dB (or your own) over the first step's: the antenna's noise is then above the radio's. One step back when the ADC clipped or the headroom
ran out, two after intermodulation. Without a spectrum it stops at the headroom target.

**Per-mode targets** (`modeTargetFor`):

| Kind | Modes (engine standard) | Metric | Split | Headroom | Floor goal / Safe rise |
|---|---|---|---|---|---|
| broadcast TV | DVB-T2/T, ATSC, ATSC 3.0, ISDB-T, DVB-S/S2, DTMB, analog TV, DVB-C | MER | balanced | 7 dB | 8 / 4 dB |
| wideband OFDM | DAB, other channels >= 1 MHz | SNR | balanced | 7-8 dB | 8 / 4 dB |
| FM broadcast | FM, HD Radio | SNR | linearity | 9 dB | 5 / 3 dB |
| pulsed | ADS-B, Mode S | sensitivity | sensitivity | 3 dB (pulses may come close to full scale) | 12 / 8 dB |
| GNSS | GNSS | sensitivity | sensitivity | 12 dB | 10 / 10 dB |
| wide span | analyser, astronomy, cells, Wi-Fi, sounders | headroom | linearity | 12 dB | 6 / 3 dB |
| narrowband voice | DMR, M17, D-STAR, NXDN, dPMR, TETRA, P25, airband, general, ALE | channel SNR | sensitivity | 14 dB | 10 / 6 dB |
| narrowband data | everything else | channel SNR | sensitivity | 14 dB | 10 / 6 dB |

A capture more than 50 times wider than the channel keeps 2 dB more headroom (more strong signals reach the ADC).

## 3. Radio models

`radioProfiles()`; `radioProfileFor(DeviceInfo)` picks one by board and name (an RTL-SDR Blog V4 by its name). The driver's reported range
wins over the table's. Each stage has its step table, whether it is settable, whether the driver counts it as a reduction, a typical noise
figure (and how much worse it gets turned down) and IIP3, and a note for the user. `planStages` spreads a total over the stages (front
first for sensitivity, back first for linearity, the front to 60 % first for balanced) with a final pass that reaches totals the greedy
split misses; `cascade` gives the plan's noise figure (Friis) and cascaded IIP3.

| Radio | OnAir sets | Notes |
|---|---|---|
| HackRF One / Pro | LNA 0-40 dB (8 dB), VGA 0-62 dB (2 dB) | amp +14 dB: the user's only |
| RTL-SDR R820T/R820T2, R828D, Blog V3, Blog V4, V4 Lite, E4000; rtl_tcp | the tuner gain list (29 steps; E4000 14) | tuner LNA / mixer / IF VGA listed, set by the driver; rtl_tcp settles longer |
| Airspy R2 / Mini | the 22-step gain table (linearity or sensitivity: the radio's Gain mode setting) | LNA / mixer / VGA listed |
| Airspy HF+ | 0-48 dB in 6 dB steps | the top 6 dB switch the preamp on: kept at 42 dB unless the amp is allowed |
| SDRplay RSP | one gain (the driver makes RFGR + IFGR of it); the LNA state itself when the driver lists `rfgr` | RFGR / IFGR shown as gains, applied as reductions |
| Pluto, LimeSDR, USRP B2xx, bladeRF, Fobos, RX888 | the overall gain | the chip's own stages listed |
| SpyServer, KiwiSDR | the server's gain | remote: settle longer |
| anything else | its reported range | |

## The amp and the antenna power

The amp is off by default and **never** switched by auto gain unless you allowed it in the popup (*Let auto gain switch the amp*, saved
per radio as `agcamp.<radio>`, the same switch the AGC had). Without it, an amp you switched on stays on, through overloads too, and its
gain is part of every total. Auto gain only *suggests* it: *Weak signal: switching the amp on may help (...)*. The bias-tee is never
touched; with a weak signal on a radio that has one, the advice is to check the antenna and its power.

With the permission, Auto may switch it on for a weak signal (channel SNR under 12 dB, or a decoder that does not lock) with nothing strong
near and no overload in the last minute (the total stays the same: the other stages come down 14 dB), and switches it off first on an
overload. Optimise once and Safe start never switch it.

## 5. Integration

* **Engine API**: `ag::autogainRun(Engine&, TuneSettings&, const DeviceInfo&, int stdMode, AgMode, AgSettings, timeout, cancel)` runs
  Optimise once or Safe start on a running engine and leaves it at the result (or puts the gain back when cancelled or out of time). The
  general form `autogainRun(AgRadioIo&, ...)` takes any radio behind `AgRadioIo`.
* **Scanner**: *optimise the gain on every signal found* (`ScanConfig::autoGain`): a quick Optimise once (3x speed, 0.5 s measurements,
  at most 12 s) on each occupied channel before it is decoded; `ScanResult::gainDb` holds it; the next channel starts from it.
* **First-run wizard** (and automations): `autogainRun(App&, how)` (1 Auto, 2 Optimise once, 3 Safe start), `autogainBusy`,
  `autogainResult`.
* **Web server / remote API** (read-only): `/status` carries `"autogain"`: mode, phase, radio, target, gain, per-stage dB, meters
  (headroom, peak, rms, crest, clipped %, floor, margin, channel SNR, overload, intermodulation), advice and the last changes.

## 6. Tests

`test_autogain` (ctest `autogain`, about 5 s): every profile (steps ascending, notes, the amp the user's); detection (Blog V4 by name,
rtl_tcp remote, unknown radios on their own range, SDRplay's `rfgr` as an index round trip); the planner (LNA first has the lower noise
figure, VGA first the higher IIP3, the amp never switched, a front cap, masked stages, every HackRF total reachable, RTL totals on the
table); the measurements (noise and tone crest factors, clipping, the histogram's robust peak, floor and channel SNR, an intermodulation
product found and a clean pair passed, compression); the mode targets. Against the simulated radio (stage tables, Friis noise figure, a
front end with IIP3 and compression, an ADC that clips and quantises, delayed gain changes, a decoder that locks): Auto converges within
60 s from the bottom, middle and top of the range on nine radios, never clips after converging, does not hunt (at most two reversals in
120 s; also over 400 s on a TV channel), gives a weak signal more gain than a strong one; a strong burst is cut within 1.5 s and the gain
recovers after it; the amp is never switched on (or off) without permission, is suggested for a weak signal, comes on with permission and
goes off first on an overload; Optimise once finishes, never ends clipping, picks the higher gain for a weak signal, lands within 1.5 dB of
the best reachable MER, keeps every step for the settle time, stays linear when two neighbours' product falls in the channel, uses the
channel SNR when the mode has no number, protects ADS-B from a strong neighbour; Safe start starts at the bottom, never clips, stops when
the floor rose by the mode's X dB, and stops early for a strong signal; the memory round trip and its band fallback; the runner, its
cancel (gain put back) and the JSON; a radio with one coarse gain (0/10/20/30 dB) in every mode kind.

**Not verified on hardware.** The thresholds are tuned on the simulation; the stage figures are typical values, not calibrated per unit.
