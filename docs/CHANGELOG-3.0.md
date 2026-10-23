# OnAir 3.0 (draft, not released)

3.0 comes after 0.2.9. The next versions are 3.1, 3.2 and so on. Everything below is in the integ-30 branch. **Experimental** means a
mode or feature tested only on signals OnAir generated itself. Nobody has confirmed it on a real transmission yet, so it may fail on
the air.

## New modes

- **NFC / RFID 13.56 MHz** (receive only): ISO/IEC 14443 Type A and B, ISO/IEC 15693, UIDs, APDUs and NDEF. *Experimental.*
- **SBAS**: the WAAS / EGNOS / MSAS / GAGAN / SDCM / BDSBAS correction messages, decoded without a GPS lock. *Experimental.*
- **ASTERIX**: radar track data (CAT 021, 048 and 062) read from the network or a file, shown on the shared map. It needs no radio.
- **Argos**: platform uplinks on 401.65 MHz and the satellite downlink. A3 / Argos-4 bursts are only listed. *Experimental.*
- **Sigfox**: uplinks in all regions (receive only). *Experimental.*
- **Marine VHF voice**: channels 1 to 88 with regional plans (INT, EUR, BAL, USA, CAN), a channel-16 watch, a scan over the whole
  span, a call log and a recording per call. A DSC call that names a channel opens this mode on that channel.
- **ISDB-S (BS)**: Japanese BS satellite TV through an LNB. *Experimental.*
- **P25 Phase 2** (TDMA): calls, talkgroups and sites. EDACS can now follow analog voice channels. *Experimental.*
- **Horus balloons**: Horus Binary v1 / v2 and RTTY telemetry, with a map and an altitude chart.
- **LoRaWAN (all channels)** across a regional plan, and **LoRa satellites** with Doppler tracking. *Experimental.*
- **ROS** is now recognised by the signal identifier (identify only: the protocol is not public, so it cannot be decoded).
- **Mode Builder**: describe a digital protocol (modulation, sync, framing, FEC, CRC, fields) and decode it, with no code.
- **Signal Lab**: blind analysis of an unknown digital signal (modulation, symbol rate, line code, framing). It can export a Mode Builder
  definition.

## Better receiving

- DAB: audio continuity fixes. Clock error is followed inside a frame, the FFT window copes with SFN echoes, and short fades no longer
  end the lock. Clicking inside a wide capture no longer slides the spectrum (#262).
- DRM: smooth audio through fades, Doppler and multipath, without the old 4-5 s restarts.
- APRS / packet: six slicers per tone pair (better with weak or tilted audio), plus Emphasis and Fix bits settings.
- Whole-span detector: per-cell noise floor and drifting carriers kept under one id. Every wide-bank mode replays the half second before
  a carrier was found.
- Satellite and long-range AIS (channels 75 and 76, message 27).
- TETRA: choose which carrier and timeslot to listen to, and see talkgroup / ISSI of clear calls.
- Iridium: voice calls through a user-installed codec plug-in, plus ACARS, packet and pager views (#281).
- DTMB TV sound in DRA through the user-installed DRA add-on (#266).
- TV: HD / SD pairing, a guide language choice (#268), the live service list follows the multiplex, and a "scrambled" badge only when
  the content really is scrambled.
- Signal identifier: many new measurements (tone spacing, constellation, OFDM spacing).
- Fixes for DTMB timing, DAB transmitter lists (#249), radios (KiwiSDR #257, network discovery #215, LimeSDR), pagers, satellites
  (Meteor LRPT dropouts #79), crashes on CPUs without AVX2 (#226), HaLow channel widths (#192) and CDR codec folders (#266).

## Using OnAir

- **Audio** pop-up in every mode: per-mode volume, squelch (level / SNR / CTCSS / DCS), filters, EQ, notch, AGC, recorder and output device.
- **Auto gain** for every radio and mode (Auto, Optimise once, Safe start), with radio gain tables and ADC headroom meters.
- **First-run setup** in six steps, and lessons.
- **Presets** per mode (tuning, gain, audio and the mode's own options), with import and export.
- **Modes shown**: hide the modes you never use.
- **Spectrum and waterfall**: aligned waterfall (#270), 30 fps when unfocused (#246), FFT up to 256k (#243), a noise blanker (#264),
  more colour maps and markers.
- **Voice monitor**: one call at a time across slots and carriers, allow / mute / priority lists and per-call recordings.
- **Voice keys**: decrypt P25 and NXDN voice with keys you add yourself. Nothing is decrypted without your key, there is no key search,
  and TETRA TEA is never implemented.
- **Scanner** view built on the memories, **Scheduler** (times, repeats, satellite passes), **Datasets** manager, **Backup and restore**.
- **Frequency (ppm) calibration** wizard.
- **Search** over decoded messages (Ctrl+K), shareable frequency lists (CHIRP, SDR++, SDR#), and GeoJSON export.
- **Status strip** with labelled cells.
- **View > Power**: Low power mode (off / automatic / always) and Save power when hidden. Lower idle CPU and fewer wake-ups.
- **Time display**: UTC or local time.
- WEFAX gallery, image enhancement for every picture mode, a propagation tool rework, an **antenna designer**, ionosphere from your own
  receptions, and the DSC ship list import and map.
- **TX studio** (a signal bench with measurements, no radio) and cleaner TX signals in every mode.
- **Ham view**: one operating screen (decode list, QSO panel, log, awards, map, spots), more modes, rig control and a safety layer
  (country band plan, duty limits, audit log). You can share ham contacts on the public map (off by default).
- **Remote API**: more endpoints (gain, squelch, volume, recording, memories, scheduler), events over WebSocket, a rigctl TCP server and
  UDP JSON lines.
- X-Midas BLUE IQ files, RTL-SDR Blog V4 (V4L) support, and a NixOS package.

## Known limits

- Every mode marked *Experimental* above has been tested only on OnAir's own generated signals.
- Voice decoding of DMR / P25 / NXDN / D-STAR / dPMR / Yaesu / TETRA / Iridium needs a codec you install yourself (docs/CODECS.md).
  Real-signal checks are still open for P25 Phase 2, NXDN, dPMR, D-STAR and Iridium.
- Ham: the SDR transmit path (with its band filter confirmation) exists in the core but is not yet wired into the Ham view, which keys a
  Hamlib rig and a sound card.
- TETRA: talkgroup / ISSI come from D-SETUP only. That layout was written from memory and has not been checked against the standard.
- EDACS message layout is this program's assumption, not a published fact.
- MHEG-5: a structure parser only. Nothing is rendered yet.
- Translations: new texts are untranslated in many languages.
- This draft lists what was merged into integ-30; the full build and test status is in scratchpad/INTEG-30-LOG.md.
