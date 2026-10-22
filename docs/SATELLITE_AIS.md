# Long-range and satellite AIS

OnAir's AIS mode receives the two long-range channels and message 27 alongside AIS 1 and AIS 2. It also has a satellite mode for IQ that a receiver in low earth orbit picked up.

Background: Li, Chen and Chen, "Long-range AIS message analysis based on the TianTuo-3 micro satellite", *Acta Astronautica*, 2017, doi:10.1016/j.actaastro.2017.02.014. The message formats follow ITU-R M.1371-5.

## The channels

| Channel | Frequency | Letter (table, `!AIVDM`) | Carries |
|---|---|---|---|
| AIS 1 (87B) | 161.975 MHz | A | all AIS messages |
| AIS 2 (88B) | 162.025 MHz | B | all AIS messages |
| Long range 75 | 156.775 MHz | C | message 27 (class A, for satellites; WRC-15) |
| Long range 76 | 156.825 MHz | D | message 27 |

NMEA 0183 has no letter for channels 75 and 76. OnAir writes `C` and `D`, as some satellite AIS receivers do.

The receiver decodes the channels in two pairs, each 50 kHz wide: AIS 1 + 2 around 162.000 MHz, and 75 + 76 around 156.800 MHz. If the radio's centre frequency is known, each pair is placed by its own frequency, and every pair that fits in the capture is decoded at the same time:

- **All four from one capture.** Tune 159.400 MHz with at least 5.4 Msps. The AIS preset picks 8 Msps there; 6 Msps is the practical minimum. The tuner panel has an "all four (159.400)" button.
- **A narrow radio** (RTL-SDR, 2–3 Msps) holds one pair at a time. "Hop" alternates between them: AIS 1 + 2 for 60 s, then 75 + 76 for 200 s. A class A ship sends message 27 every 3 minutes, using each long-range channel once every 6 minutes, so 200 s on the pair hears every ship in range once. The plan comes from `ais::aisTunePlan()`.
- **A recording without a known centre** works as before: AIS 1 and 2 are taken at −25 and +25 kHz of the dial. A recording of 5 Msps or more uses the file's centre frequency, so a wide capture gets all four channels.

From the shore, the long-range channels are usually quiet. Ships send message 27 only where no base station covers them, and only class A ships send it.

## Message 27 (ITU-R M.1371-5, table 79)

There are 96 bits: type, repeat, MMSI, position accuracy, RAIM, navigational status, longitude (18 bits) and latitude (17 bits) in 1/10 minute (about 185 m), speed in whole knots (63 = not available), course in whole degrees (511 = not available), and the GNSS position latency flag (0 = under 5 s, 1 = 5 s or more). Message 27 has no heading and no time stamp, because the frame is kept short. It ends about 7 ms before the end of its 26.7 ms slot, which leaves room for the delay to a satellite up to about 2000 km away.

`ais::encodeLongRange()` / `ais::decodeLongRange()` (`core/include/dect2/ais/ais_longrange.h`) encode and decode the message. `AisMsg` carries `posAccuracy`, `raim` and `posLatency`.

Where message 27 shows up in the interface:

- **Table:** "LR" in the Pos column when the position came from message 27, and the channel (1 / 2 / 75 / 76).
- **Detail panel:** how many message 27s arrived, the latency flag, the receive time in UTC, and a "(1/10 min)" mark on a coarse position.
- **Map:** a dashed orange ring around every ship that sent message 27. The shared map's info line says "long-range AIS (message 27)".
- **Counters:** a column for every channel in the capture.

**Time stamps:** message 27 carries no time, so every good message gets an `AisRxRecord` with the UNIX time of its burst start. That time is the time base plus the signal time, sample-accurate within one run. `AisTelemetry::recent` holds the last 500 records. The time base is the computer's clock at the first sample. For a recording, enter the recording's start time in the Satellite view.

## Satellite mode

Turn it on in the tuner panel ("Satellite mode"), or let the pass arming do it. It is meant for IQ from a receiver in low earth orbit: an AIS payload whose samples you have, or a balloon or aircraft. A receiver on the ground hears ships directly within line of sight, about 40 nm, and does not need this mode.

A satellite receiver differs from a coast receiver in four ways (500 km orbit):

| | Coast | Satellite (500 km) | What the mode does |
|---|---|---|---|
| Doppler | ±0 | ±3.8 kHz (`aisMaxDopplerHz`), up to ~50 Hz/s | Channel filter 12 / 18 kHz instead of 7.5 / 15; every burst's own frequency is measured, then the narrow second filter; tracking per channel (fit over 60 s: shift and rate, shown in the Receiver tab) |
| Delay | ~0 | 1.7–8.6 ms (`aisSlantRangeKm`, `aisDelayMs`) | Nothing is slot-locked: bursts are found by power. A burst that runs into the next slot's burst is split at the power steps and each part decoded |
| Footprint | one SOTDMA cell | some 2000–2300 km wide above 20° elevation (`aisSwathKm`; about 2250 km in the paper) | — |
| Collisions | rare | many cells that do not hear each other share a slot | Frequency candidates (the shifts of recent bursts, the expected shift of each known ship from the satellite's elements, a ±4.5 kHz grid in 1.5 kHz steps) through the narrow filter; successive cancellation: a decoded burst is modulated again, fitted (timing to a fraction of a sample, frequency, amplitude and phase) and subtracted, and the rest is decoded again, up to three layers |

The collision decoder is `aisDecodeCollided()` in `core/include/dect2/ais/ais_sat.h`. In `tests/ais/test_ais_satellite.cpp`, it decodes:

- every burst of a −4.5 to +4.5 kHz Doppler sweep at 18 dB,
- at least 19 of 20 bursts in back-to-back slots overlapping by 6 ms (the plain decoder gets 0),
- both bursts of a two-burst collision 8 dB apart, using cancellation,
- most of a three-burst collision at 0 / −8 / −16 dB,
- two equal-power bursts 5.2 kHz apart, when the tracker supplies their frequencies.

**Limit:** when two bursts collide at almost the same power (within about 4 dB) and their shifts are close, both are usually lost. That is the remaining loss in the busy-slot test (60 bursts in 70 slots: coast mode 42, satellite mode 46).

The mode costs CPU: every burst can take up to about 30 decode attempts.

## The Satellite view (AIS tab > Satellite)

- **Passes:** pick the receiving satellite from the TLE store of the Passes window (View > Passes; download or paste the elements), filtered by name or NORAD number. The view lists its passes over the area of interest (the receiver position) for the next 24 hours. "Arm the passes" starts AIS in satellite mode at AOS and stops it at LOS if it started it. Every armed pass opens a pass in the statistics.
- **Map (the "eavesdropper" diagram):** every ship heard. Orange with a ring means a "new-type" ship, one that sent message 27; blue means the others. Size grows with the number of passes a ship was heard in. Click a pass in Statistics to see only its ships, with the satellite's ground track in yellow.
- **Statistics:**
  - totals: receptions, ships, passes;
  - new-type ship count and ratio, and the share of them still sending messages 1–3 / 18 / 19;
  - detections per pass;
  - message-type mix;
  - SNR against the satellite's elevation as seen from the ship, in 10° bins (needs the elements);
  - detection probability per month. With elements, this is the ships heard over the known ships that were inside the footprint during the pass. Without elements (marked *), it is the ships heard per pass over the ships heard that month.
- **Export:** four tables as CSV, or JSON by file name: receptions, passes, vessels, months.

**Passes without elements:** a pass is a stretch of receptions without a silence of 15 minutes or more.

The statistics are kept by `AisSatStats` (`ais_sat.h`). It is fed from `AisTelemetry::recent`, takes each record once, and is tested in `test_ais_satellite.cpp`.

## What is not verified

- Nothing has been tested on a real satellite recording. Only synthetic signals were used: the AIS generator, `renderAisBursts`, with channels C and D added for 75 and 76.
- The pass arming was not run live.
- The SNR-against-elevation figure uses the ship's last known position.
- Equal-power collisions (see above) stay a limit.
