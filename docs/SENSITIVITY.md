# Sensitivity and lock-time measurement (`sens_sweep`)

Goal of the campaign: every decoder locks faster and decodes at a lower SNR. Without numbers nobody can claim that, so every mode is
measured the same way, with the same signals, noise and impairments, and a change is compared against a committed baseline.

## What it measures

For a mode and a *scenario* (an impairment, or none) it plays the mode's clean test signal through the impairment chain at a series of SNRs into
a **fresh receiver**, many seeded trials per point, and reports:

| figure | meaning |
|---|---|
| success rate | a trial succeeds when the receiver counted at least half as many *valid units* as it does on a clean run. A valid unit is what the receiver itself counts as good: its CRC / Reed-Solomon / LDPC+BCH / FIB check passed (`blocksOk`, `rsClean+rsCorrected`, `fibOk`, RDS groups). FM audio: the 1 kHz tone is 20 dB over everything else. |
| time to lock | seconds of signal from the start of the trial to the first valid unit (polled every 50 ms of signal, so 50 ms resolution). Median and 90th percentile over the successful trials, at the lowest measured point 3 dB or more above the threshold. |
| threshold SNR | lowest SNR with at least 90 % success, scanning down from the top until the first point below 90 % |
| PER | `bad / (good + bad)` of the receiver's own counters at the threshold (mean over trials) where the decoder exposes a failure counter |

A trial starts at a random point of the test signal (seeded), so lock time includes waiting for the next frame/sync like a user tuning in.
Trials are deterministic: seed = hash(mode, scenario, SNR, trial); the result of a trial does not depend on which thread ran it
(`DECT2_NOPACE` is set so threaded decoders wait instead of dropping frames). Gaussian noise is `PortableNormal`, identical on every platform.
The compiler and standard library can still change a floating point result in the last digit, so the committed baseline is made on the
Linux runner (the `Sensitivity sweep` workflow) and compared with runs from the same place.

### SNR definition

`SNR = Pa / (N0 * B)`. `Pa` is the mean power of the *active* part of the clean signal (blocks of 512 samples within 10 dB of the strongest
2 %, so a bursty mode such as ADS-B or Iridium is measured on its bursts, not diluted by the idle time). `B` is the mode's noise bandwidth
(`Mode::bwHz`, the channel width the standard quotes its C/N in: 7.61 MHz for DVB-T/T2 8 MHz, 1.536 MHz DAB, 200 kHz FM, 25 kHz for most
narrowband data modes...). `N0` is the white noise density, so the noise power per complex sample is `Pa * (fs / B) / 10^(SNR/10)`. For test
signals with several carriers (ADS-B aircraft, ACARS channels, AIS 1+2) `Pa` is the sum, so the per-carrier SNR is lower; use the figures to
compare versions, not as a datasheet sensitivity. FM and the single-signal modes are directly comparable with the usual C/N numbers.

### Scenarios

| name | impairment |
|---|---|
| `awgn` | white noise only |
| `cfo2ppm` / `cfo20ppm` | carrier error of a 2 / 20 ppm radio at the mode's frequency (20 ppm capped at a quarter of the channel) |
| `offcentre` | channel moved off the centre by 20 % of its width + 5 % of the sample rate |
| `sro50ppm` | sample clock 50 ppm fast (cubic interpolation) |
| `echo` | static echo, -6 dB, at about half the mode's guard interval (`Mode::echoUs`) |
| `tu6` | Rayleigh fading, COST 207 6-tap Typical Urban, Jakes Doppler (`Mode::fdHz`, 10 Hz default: pedestrian) |
| `rician` | the same with a line of sight, K = 10 dB |
| `phasenoise` | Wiener phase noise, 50 Hz linewidth |
| `combined` | 2 ppm carrier + 20 ppm clock + echo + phase noise, together |

Profiles are scaled to unit total power, so the mean signal power (and the SNR) is unchanged by multipath. DAB RA4 and DVB-T Portable/Pedestrian
profiles are not separate: TU6 with the mode's Doppler is the standard stand-in; add a profile in `sens_impair.h::profileTaps` for others.
Wraps/clipping and lost samples are covered by the existing `tests/*/test_*_impair.cpp` tests (pass/fail), not swept here.

### Search

Coarse: 4 dB steps from the mode's top SNR down, `trials/3` trials a point, until the first point below 90 %. Fine: 1 dB steps (2 dB in
`--quick`) below the last passing coarse point with the full trial count, until the first point below 90 %. Every measured point is in the JSON.

## Running it

```
cmake -S . -B build -G Ninja && cmake --build build --target sens_sweep
tools/sens/run.sh --quick --mode 'dab|fm'            # AWGN, TU6, 20 ppm carrier, combined; 4 trials a point; a few minutes
tools/sens/run.sh --full  --mode dvbt                # all scenarios, 1 dB steps, 12 trials (--trials N to change)
tools/sens/run.sh --quick --mode dvbt2 --scenario 'awgn|tu6'
./build/sens_sweep --list                            # modes and scenarios      ./build/sens_sweep --gaps   # every mode of the tuning table: test signal? in the harness?
```

`run.sh` writes `build/sens-out/sens-<mode>.json` and `sens-baseline.json` and prints the delta table against
`tests/sens/baseline/sens-baseline.json` (`tools/sens/compare.py`). It exits 1 when a threshold is worse by more than 0.3 dB, a median lock time
is more than 20 % (and 50 ms) longer, or a scenario lost its threshold. Quick runs have 2 dB threshold steps, so against a full baseline the dB
tolerance is 2 dB: quick catches big regressions, full claims fractions of a dB. `--update-baseline` merges the run into the committed file.

**On GitHub** (the Mac's battery and build queue are busy): Actions > *Sensitivity sweep* > Run workflow (`.github/workflows/sens-quick.yml`),
inputs: mode regex, tier (quick/full), scenario regex, trials, update_baseline. It builds `sens_sweep` only, runs it, compares with the baseline
and uploads the JSON as the artifact `sens-results`. Start it with `gh workflow run sens-quick.yml -f modes='dab|fm' -f tier=quick`, wait with
`gh run watch <id> --exit-status`.

### For a per-mode agent

1. Before changing anything: `tools/sens/run.sh --full --mode <id>` (or the workflow) and keep `sens-<id>.json` as *before*.
2. Make the change, run again, `python3 tools/sens/compare.py before.json after.json` (both are `{"modes":[...]}` files; `sens-baseline.json` has that shape).
3. Claim a gain only from a full run with a threshold change above the step and the same seeds; then `--update-baseline` and commit the file.
4. A mode missing from the registry: add a block to `tools/sens/sens_modes.h` (clean generator, receiver set-up as the engine does it, `blocks<Tel>` or your own measure, `bwHz`).

## Coverage

In the harness (generator + receiver wired like `tests/engine/test_iq_modes.cpp`): `dvbt2 dvbt atsc dab isdbt fm fm-rds dvbs2 dtmb atv dmr drm
adsb sonde ais acars aero iridium mesh(heavy) pager packet vdl2 dstar nxdn dpmr tetra p25`. A mode whose clean reference run gives no valid
output is reported as "skipped" (receiver wiring in the harness needs a look) instead of producing numbers.

**Gap list** (`sens_sweep --gaps` prints the current state):

* No built-in test signal at all: `wb dgps dme eloran mpt1327 fusion ltr edacs milhf marinevhf p25p2` (DVB-C has the `tests/dvbc` generator, DVB-T2 MISO/other
  PLP configurations only the one in the table).
* Test signal exists, not yet in the harness (needs a receiver block in `sens_modes.h`; most are `blocks<Tel>` plus a prepGeneric, a few need long
  signals): `gnss` (C/N0 defined, not SNR), `marine` NAVTEX/DSC (25 s signals), `inmc` (36 s), `atsc3`, `dvbc`, `hfdl`, `hdr cdr hfdig airband ism ndb vorils m17 cell cw
  psk js8 gsmr specx specan uplink orbcomm mac astro ale ble iono qo100 wifi general rid nr time meteor goes cubesat globalstar vdes hrpt zigbee flarm
  hfbeacon dji nbiot halow loraraw codar datawell multitone same ant dect tetrapol elektro lbj vdl4 uat wisun eczas wmbus tones sarsat mt63 zwave fpv
  mscatter cquam fsq ifkp kaal kavb kapp kaft kait kaws ardop goesdcs modeac bletracker satbeacon klw tdcs nfc sbas asterix argos sigfox isdbs horus
  lorawanwide lorasat`, and the weak-signal HF modes FT8/FT4/WSPR (no generator found in the repo), APT/LRPT (`meteor`).
* Not measured by design: speech quality of the voice modes (only the signalling/CRC units), and the image content of satellite modes.

## Baseline

`tests/sens/baseline/sens-baseline.json`: full sweep (8 trials a point, 1 dB steps) of 25 modes, made on the GitHub Linux runner 2026-10-23 (push to branch
`sens-baseline-run-030` runs the workflow; the build workers run the same as ctest `sens_full`, label `sens-full`, about 2 h). Not in it: `tetra` (no valid
output on its clean test signal: the harness wiring needs a look, probably `setCenterHz`/scene) and `mesh` (heavy, `--heavy`). Regenerate with the workflow
(`update_baseline`). Known harness limits: the sample clock uses a 4-point cubic (a fraction of a dB loss at the band edge of critically sampled OFDM);
multipath delays are rounded to whole samples; lock time has the 50 ms poll resolution (T2 and other threaded decoders also lag by their queue).
