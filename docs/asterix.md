# ASTERIX surveillance data

The mode **ASTERIX** (Aviation group) decodes EUROCONTROL ASTERIX surveillance data that a radar, an ADS-B or multilateration ground station or a tracker sends on a network,
or that was recorded to a file, and shows it as a track table and a map. There is **no radio demodulator for radar**: a radar's pulses are not something a software radio can
turn into targets, so this mode does not touch the radio. The built-in test source only keeps the engine running. ADS-B and Mode S from the air (1090 MHz) are the **ADS-B**
and **Mode A/C** modes.

## Input

| Input | How |
|---|---|
| UDP | **Listen** on a port (default 8600). Closed until you press Listen; listens on this computer only (loopback) unless **let other computers send** is ticked. A multicast group address makes it join the group (an address from 224.0.0.0 to 239.255.255.255). Each datagram holds one or more data blocks. |
| File | **Open file...**: raw data blocks one after another, or a classic pcap capture (Ethernet, Linux cooked or raw IP; IPv4, not fragmented; UDP payloads). pcapng: save as pcap first (the mode says so). Files up to 512 MB, read on a worker thread. |
| Demo | **demo traffic**: made-up aircraft around the radar position in all three categories (a CAT 048 radar, a CAT 021 ground station, a CAT 062 track server), sent through the same decoder. |

Only listen to a feed you are allowed to receive. Nothing is sent anywhere.

## Categories and what is read

| Category | Edition used | Items read into the table |
|---|---|---|
| **021** ADS-B target reports | edition 2.6 user application profile (the FRN order: 010, 040, 161, 015, 071, 130, 131, 072, 150, 151, 080, 073, 074, 075, 076, 140, 090, 210, 070, 230, 145, 152, 200, 155, 157, 160, 165, 077, 170, 020, 220, 146, 148, 110, 016, 008, 271, 132, 250, 260, 400, 295, RE, SP) | 010 source, 040 type, 161 track number, 130 / 131 position, 080 address, 071 / 073 time, 140 height, 145 flight level, 151 true airspeed, 155 / 157 vertical rate, 160 ground vector, 170 callsign, 070 squawk |
| **048** monoradar target reports | 1.x | 010, 140 time of day, 020 type, 040 range and azimuth, 070 squawk, 090 flight level, 220 address, 240 callsign, 161 track number, 042 Cartesian position, 200 ground speed and heading, 110 3D height |
| **062** system track data | 1.x | 010, 070, 105 position, 100 Cartesian position, 185 velocity, 060 squawk, 245 callsign, 380 (address, identification, true airspeed, ground speed), 040 track number, 136 / 130 / 135 level and altitude, 220 climb rate, 390 (callsign, departure, destination), 080 status |

Every other item is skipped by its length (fixed, extendable with FX, repetitive, explicit, compound). An item whose length is not known to this decoder (for example item 120 of
CAT 048, a free-form compound subfield list, or an unknown FRN) ends the decoding of that record: the record is shown as cut and counted, the values read before it are kept.
Other categories are counted by number.

- **Track table.** One row per source, category and track number (or aircraft address when there is no track number). Rows update in place; live tracks are dropped after a time
  without a report (60 s, adjustable); tracks read from a file are kept. A record with neither a track number nor an address (a plot) is counted, not tabled.
- **Positions.** A WGS-84 position in the record (CAT 021 and 062) is used as sent. A CAT 048 target has only range and azimuth from the radar, and CAT 062 may have only Cartesian metres:
  these are placed with the **radar / system centre** position you enter (slant range corrected by the flight level, spherical earth, no refraction, a local projection of the
  system treated as flat) and marked **~** because they are approximate. Without that position such targets have no map position.
- **Map.** The ADS-B map: tracks with heading triangles, trails, labels (callsign and flight level), colours by category, and a click to select.
- **Sources, Records.** SAC/SIC of each sender with its record counts, datagram and block counters, and the last 80 records one per line.

## What is verified and what is not

- The record layouts follow the public EUROCONTROL category descriptions; the field widths, scaling and the FRN order of the three profiles were checked against a public machine-readable
  description of the same documents (the definition files of the open source `asterix` decoder, read as a reference only; no code was taken). Records built byte by byte by hand decode to the expected values (`tests/asterix/test_asterix_proto.cpp`), and the
  decoder is fuzzed with random blocks under the address and undefined-behaviour sanitisers.
- The UDP input, the file readers (raw and pcap in both byte orders), aging and the demo are tested on the loopback interface and generated files
  (`tests/asterix/test_asterix_engine.cpp`).
- **Not tested against a live radar, a real recording or another vendor's encoder.** Other editions of a category (other editions may use a different item order, as CAT 021 does; CAT 048 and 062
  later editions add items) may decode partly or not; CAT 062 items after item 390 in a record are not read.
- Item 090 of CAT 021 is skipped by its FX chain; of CAT 062 item 080 only the monosensor and SPI bits are shown.
