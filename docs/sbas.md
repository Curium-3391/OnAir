# SBAS (L1 augmentation messages)

The mode **SBAS** (Satellite and navigation group) receives the geostationary augmentation satellites on 1575.42 MHz (WAAS, EGNOS, MSAS, GAGAN, SDCM, BDSBAS and the
like) and decodes what they send. It is receive only and it does **not apply** the corrections: it shows them. It is a separate mode from GNSS because it needs no GPS
lock first, its screens are about the messages, and it can run on an antenna that sees only the southern sky.

## What it does

- **Acquisition and tracking.** The GNSS receiver is used with the search restricted to the SBAS PRNs 120 to 158 (the GNSS mode searches them only after a GPS satellite
  is locked). Same 4 Msps default, same frequency windows (a radio without a TCXO is found with the wider windows that follow), same Viterbi decoder and CRC-24Q check.
- **Message viewer.** Every message with a good CRC is decoded by type and kept per satellite: PRN mask (type 1), fast corrections with their UDRE (types 2 to 5 and the first
  half of 24), integrity flags (6), fast correction degradation (7), GEO navigation message (9), degradation parameters (10, raw fields), SBAS network time and UTC offset (12),
  GEO almanac (17), ionospheric grid masks (18), long-term satellite corrections (25 and the second half of 24), ionospheric delay corrections (26). Types 27 and 28 are
  counted and named, not decoded; types the L1 signal does not carry are named "other / reserved".
- **Screens.** Satellites (state, C/N0, Doppler, messages, which of the 64 types have been received), Messages (type counters and a log with one line per message),
  Fast corrections (per satellite: slot, the satellite the slot stands for from the PRN mask, correction, UDRE, IODF, IODP, degradation factor), Ionosphere (per band: grid points,
  blocks received, delays with minimum, mean and maximum, worst GIVE index), Navigation (GEO position and clock, time offset, almanac, degradation parameters, long-term corrections).
- `dect2cli --standard sbas --freq 1575.42` prints the satellites with the number of satellites in the mask and the fast corrections held.

## What is checked and what is not

- The bit layouts follow the public description of RTCA DO-229 (message types, field widths and the table of UDRE and GIVE variances); the layouts were cross-checked against
  the public RTKLIB source for the types it decodes, and the layouts of types 10, 12 and 17 are from the description alone. Nothing was copied from a decoder.
- Tests: field helpers; the CRC; hand-checked tables; a synthetic service (PRN mask of 15 satellites, 51 fast corrections, an ionospheric grid band, GEO navigation message
  at geostationary radius, time offset, almanac, long-term corrections) built bit by bit and decoded again; the whole chain on generated IQ with three satellites at clean level,
  +-5 kHz, a sample clock 20 ppm off (the carrier 31 kHz off) and 38 dB-Hz at the zenith.
- **Not checked against a real satellite or a recording.** The message content in the test signal is made up. Whether the PRN mask of a real service decodes to the satellites
  it should has not been seen on air.
- The grid points are shown by their place in the band's mask. Their latitude and longitude come from a table of the SBAS description that is not in this mode, so there is no
  ionospheric map.
- The type 10 fields are shown as raw integers; the units are in the description and are not guessed.
- The service name beside a PRN (EGNOS, WAAS, ...) is the usual assignment and is informal; assignments change. The satellite's own messages are the truth.

## QZSS

The QZSS L1 C/A satellites (PRN 193 to 202) are tracked and used in the fix by the GNSS mode and are in its test (`gnss_qzss_sbas`). The QZSS **L1S** signal (the SBAS-like
sub-meter augmentation, SLAS) and the **L6** signal (centimetre augmentation) have their own codes and message formats and are **not decoded**: their code and message
descriptions (IS-QZSS-L1S, IS-QZSS-L6) were not available to build them clean-room, and nothing was invented in their place.

## Limits

Needs an active GNSS antenna (bias tee) and a clear view toward the equator. A radio at 2.046 Msps or more. Geostationary satellites at low elevation (far from the
sub-satellite longitude) are weak. Receive only; no corrections are applied to a position.
