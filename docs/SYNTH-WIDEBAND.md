# Synthetic wideband: several narrow radios as one wide radio

> **Status (2026-10-23): unbuilt and unverified.** The code below was written without being compiled or run (the owner's laptop was on
> battery). No number in this document is measured unless it says **measured**. The phase study (section 6) is pending. Section 8 lists what
> a later session must build and test.

## 1. What it is

A *radio group* is 2 to 5 ordinary radios (for example five RTL-SDRs), tuned side by side so that each one covers a slice of the band
and neighbouring slices overlap a little. OnAir lines the slices up and joins them into **one wide stream**. In the radio list the group
is one entry ("Group: name (5 radios)"), so the spectrum, waterfall, wide bank, channelizer, scanner and every other mode treat it as one
wide radio.

An RTL-SDR runs at about 2.4 Msps, of which about 1.9 MHz is clean. Five of them with overlaps give about 8 MHz. That is enough to
watch a whole DAB block group, a DVB-T channel's spectrum, or several MHz of a band at once with a waterfall. One dongle can't do that.

How it works (`core/include/dect2/dsp/synth_wide.h`, `core/include/dect2/source/synth_group.h`):

- Each radio delivers into its own bounded queue (about 0.4 s). A worker thread removes each radio's DC spike and feeds the stitcher. It
  writes the wide stream into the receiver's ring.
- The stitcher measures every neighbour pair from the signal both radios hear in their overlap:
  - frequency error (coarse and fine)
  - sample delay
  - gain
  - phase
  - clock drift (fed back into a resampler, so it also follows temperature drift)

  It then cross-fades the slices over each overlap.
- **Phase tracking.** Every FFT hop (about 0.2 ms) the phase of each seam is measured again on that hop's own overlap. A small Kalman
  filter smooths it, and the stitch of the same hop uses the result, so there is no prediction lag.
- A radio that fails to start, stops delivering, or is switched off is left out. Its neighbours are faded over the gap and the status
  says which radio it is. The group keeps running with at least 2 radios.

## 2. Setting up five dongles

- **Same model, same rate.** All members run at one sample rate (automatic: 2.4 Msps for RTL-SDRs). Mixed radios work if they all reach
  the rate.
- **USB.** Each RTL-SDR at 2.4 Msps sends 4.8 MB/s, so five send 24 MB/s. A USB 2 hub carries about 30-35 MB/s reliably. Above 30 MB/s
  OnAir warns. Spread the dongles over several ports or controllers if you see "Delivered" below 100 % in the group window.
- **Power.** Use a **powered hub**. Each dongle draws up to about 300 mA and gets warm. Short USB extension cables keep the dongles apart,
  so they don't heat each other (heat is drift).
- **Antenna.** All radios must hear the same antenna, through a splitter.
  - A 2-way splitter loses about 3.5 dB per output, 4-way about 7 dB, and 8-way about 10 dB. A 5-radio group needs a 6- or 8-way splitter
    (or a 4-way plus a 2-way), so expect 8-10 dB of loss.
  - For weak signals, put a low-noise amplifier **before** the splitter, not after.
  - Unused splitter outputs should be terminated with 50/75 ohm.
- **Overlap.** The dialog computes it. Keep the default: at least about 150 kHz at 2.4 Msps, about 400 kHz for an 8 MHz span with five
  dongles. The alignment needs a signal both neighbours hear in the overlap. A broadcast multiplex or a busy band has one; empty noise
  hasn't (the status then shows a poor mismatch, which is harmless when there is nothing to see).
- **Shared clock (optional, advanced).** Free-running dongles each have their own crystal, so the phase between slices wanders. Feeding
  all dongles from one reference removes the frequency and drift differences and most of the phase wander. The phase study (section 6)
  is meant to show how much this matters.
  - A KrakenSDR/KerberosSDR does exactly this in hardware.
  - On ordinary dongles the usual modification is to remove each dongle's crystal and distribute one oscillator's output to the crystal
    input of each, with short equal cables and a buffer/splitter.
  - This needs soldering and voids warranties. It is described here only in general terms.

## 3. Using it

1. In the radio list (SRC), choose **Group radios…**.
2. Add 2-5 radios, **lowest slice first**, and give the group a name.
3. Set the centre frequency and the total span. The window shows:
   - each radio's centre frequency
   - the overlap
   - the span reached (and the most these radios can reach)
   - the stitched rate
   - USB warnings
4. **Save group.** The group is saved by the radios' serials, so it comes back when the same dongles are plugged in. It appears in the
   radio list when at least 2 of its radios are found.
5. Pick the group in the radio list and start.

While it runs, the window shows:

- **For each radio:** on/off (hot remove and add back), locked, frequency offset, drift (ppm), the samples delivered against the nominal
  rate, and dropouts.
- **For each seam:** the overlap mismatch (about -30 dB good, above -10 dB poor), gain, coherence, and the per-hop phase movement.

**Recalibrate** starts the alignment from nothing. Changing the frequency recalibrates by itself.

Not supported yet:

- Adding a radio that was not part of the group while it runs. Edit the group and restart.
- Offset tuning. A group is tuned on its centre; each radio's own DC spike is removed by a slow DC filter.

## 4. What it suits

| Suits | Notes |
| --- | --- |
| Spectrum, waterfall, band survey, scanner | The main use. Seams are amplitude-matched. |
| FM, AM, NFM, SSB, narrow digital voice, pagers, ADS-B-like narrow bursts | A narrow signal inside one slice is untouched. One on a seam sees a slowly varying phase, which these demodulators tolerate. |
| Wide bank / multi-carrier voice | Each channel is narrow. |

| Experimental / not suited | Why |
| --- | --- |
| DVB-T/T2, DAB, ATSC, other wide OFDM or QAM across seams | Phase differences between slices act like extra noise on the carriers near and across seams. With phase tracking it may decode at lower orders (QPSK, 16-QAM); **not measured yet**. A shared clock should help. |
| Direction finding, beamforming, GNSS | Needs true coherence and calibration (KrakenSDR; `core/include/dect2/df/rtl_group.h` is that path). |

## 5. Expected numbers

**None measured yet.** The stitcher's existing unit test (`tests/dsp/test_synth_wide.cpp`, from the first commit) checked these with
3 synthetic radios and many carriers:

- frequency error to within 40 Hz
- drift to within 2 ppm
- seam mismatch below -15 dB
- carrier levels to about 0.7 dB mean

That test has not been re-run since the changes on this branch.

## 6. Phase study (PENDING)

Planned and not run. The plan:

1. Extend the synthetic test with oscillator models, independent for each radio:
   - R820T PLL phase noise: a 1/f² Wiener process with a Lorentzian linewidth parameter (1, 3, 10, 30 Hz), plus a white floor of about
     -100 dBc/Hz.
   - TCXO: ±1 ppm, about 0.002 ppm/s thermal drift.
   - Ordinary crystal: ±30 ppm, about 0.03 ppm/s while warming up.
   - The sample clock shares the LO's ppm (one crystal per dongle).
   - A shared-clock variant: common ppm, drift and Wiener part, with an independent PLL residual (an Ornstein-Uhlenbeck process, loop
     bandwidth about 50 kHz).
2. Feed the stitched stream into OnAir's DVB-T receiver (`core/src/dvbt`). Use 5 slices, each radio at the channel's native rate / 4 (so
   the stitched rate is the native rate), for 8, 7 and 6 MHz channels, with QPSK, 16-QAM and 64-QAM. Measure:
   - lock and TPS
   - packets good and bad after settling
   - MER
   - RS corrected and failed

   Run each case with phase tracking off and on, for free-running and shared clocks. Use one wide radio with the same oscillator as the
   reference. DVB-T2 256-QAM would only be estimated from the 64-QAM MER margin.
3. If per-hop tracking is not enough, add an opt-in DVB-T hook: a separate common phase per slice from the continual pilots. The normal
   DVB-T path stays unchanged.

## 7. Files

- `core/include/dect2/dsp/synth_wide.h`, `core/src/dsp/synth_wide.cpp`: the stitcher. Changed on this branch:
  - 5 slices
  - `SynthCfg::trackPhase` / `trackLinewidthHz`, the per-hop Kalman seam tracking
  - `recalibrate()`, `sliceCentreHz()`, `overlapHz()`
  - `PairStatus::tracking` / `trackRmsDeg`
- `core/include/dect2/source/synth_group.h`, `core/src/source/synth_group.cpp`:
  - `planSlices`
  - saved groups (`synthGroupsToString/FromString`, `setSynthGroups`)
  - `synthGroupDevices` (list entries)
  - USB load warning
  - `SyntheticWideSource` (IqSource) and the status/command functions the app uses
- `DeviceInfo::Group` (source.h). `listRadios` lists groups, `makeSource` opens them, `claimDevice` claims every member, and offset tuning
  is off for groups (engine_start.cpp).
- `app/ui/synth_group_ui.cpp`: the "Group radios" window. Wiring is in app.h, prefs.cpp (`synthGroups`), app_state.cpp, draw_ui.cpp and
  toolbar.cpp.

## 8. NEEDS BUILD/TEST

Nothing on this branch has been compiled. A later session, on mains power:

1. **Build** `dect2core`, `test_synth_wide`, then the app target. Fix compile errors in `synth_wide.cpp` (tracking code),
   `synth_group.cpp`, `device_claims.cpp`, `source.cpp` and `synth_group_ui.cpp` (ImGui calls, `tr()` format strings).
2. **Run `test_synth_wide`** and check that the first commit's checks still pass with `trackPhase` on (the default). If not, compare with
   `trackPhase = false`. Watch especially for these:
   - the lock condition now uses the tracked cross-spectrum
   - the "mean residual" kick in `solve()` (`t.phi += dp.phase0`) might fight the per-hop measurement; drop it if seams get worse
3. **Add tests and register them in CMakeLists.txt:**
   - `planSlices`: 5 × 2.4 Msps at 8 MHz gives n = 1024, overlap ≥ 64 bins, out rate 9.6 Msps; too wide a span is clamped with a note
   - the group string round trip
   - `synthGroupDevices` with fake DeviceInfos (needs ≥ 2 members)
   - `SyntheticWideSource` with a `ChildFactory` of fake IqSources: start/stop, one child failing to start (group runs degraded), one
     child stopping delivery (status shows "no samples", the stitch goes on), `setMemberEnabled` off then on, retune recalibrates, and a
     rate mismatch rejected
   - a phase-noise case in `test_synth_wide`: 5 slices with a Wiener phase of 30 Hz linewidth per radio; the mismatch with tracking on
     must be clearly better than with it off
4. **Phase study** (section 6). Write `tests/dsp/study_synth_dvbt.cpp` (short default for ctest, full matrix by environment variable)
   and fill in sections 5 and 6 with the measured numbers.
5. **UI.** Hidden isolated `--shot` of the Group radios window (no real radios: check the empty state and the "Pick at least 2 radios"
   path). Run `test_i18n`: the new strings are untranslated, which is a warning only.
6. **Real hardware (owner).** 2-5 RTL-SDRs on a splitter. Check:
   - FM band span: are the seams invisible on the waterfall, and does "Delivered" stay near 100 %?
   - a DAB or DVB-T multiplex: mismatch and lock, decode or not
   - unplug one dongle while running

### Points found by reading (fixed or to watch)

Fixed while reading:

- `planSlices` FFT-size loop rounded up to 2048 at 2.4 Msps; it now picks 1024.
- The child rate check rejected drivers that round the rate. Up to 50 ppm is now accepted (the drift loop absorbs it).

To watch:

- **Chain dependency.** Each slice is aligned to its left neighbour. If a middle radio drops out, the slices to its right keep their last
  correction and drift uncorrected until it returns. A wider realignment across the gap is not implemented.
- `synthGroupSetMember` starts a radio while holding the group-registry lock. That is safe, but the UI waits for the radio's driver to
  open.
- The worker drops a child's backlog when its queue is half full. The stitcher then sees a delay jump and realigns that slice (about
  0.1-0.3 s of a bad seam).
- Only one receiver should run a given group. Claims prevent opening the same dongles twice.
