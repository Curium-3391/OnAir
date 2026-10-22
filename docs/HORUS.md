# Horus balloons (Project Horus telemetry)

The **Horus balloons** mode receives the telemetry of amateur high-altitude balloons that use the Project Horus formats
(horus-gui / horusdemodlib on the receiving side, RS41ng and similar trackers on the payload):

- **Horus Binary v1** (22-byte packet, payload IDs 0 - 255) and **Horus Binary v2** (32-byte packet, 16-bit payload ID, 9 custom bytes):
  100 baud 4FSK, 270 Hz tone spacing by default, Golay (23,12) FEC, bit interleaver and scrambler, CRC16-CCITT.
- **Horus RTTY**: UKHAS sentences `$$CALL,count,HH:MM:SS,lat,lon,alt,...*CRC16` at 50, 100 and 300 baud, 7N2 or 8N2 (7N1 is read too,
  the framer only checks the first stop bit), any shift from about 150 Hz to 1.2 kHz.

Every packet is decoded to latitude, longitude, altitude, speed, temperature, battery voltage and satellites (binary), and the
custom fields of v2 when the payload's format is known. Receive only.

Mode list: *Weather & science* > **Horus balloons**. Family 148, engine standard 150, active standard 149 (`horus_modes.h`), id `horus`
(`dect2cli --standard horus`).

## Receiving

- **Radio (IQ):** tune the dial to the flight's frequency. The default is 434.660 MHz (Project Horus); the frequency list has the
  sigidwiki examples (434.200 MHz Australia, 437.600 MHz Poland, 431.050 MHz California) and the usual 434.2 - 434.9 MHz span.
  The app asks for 960 kS/s and tunes 30 kHz off the signal to keep it away from the DC spike.
- **Sound card:** choose the sound-card source and feed it the USB audio of a receiver tuned about 1 kHz below the signal (the tones
  land at roughly 500 - 2500 Hz). The audio source gives analytic samples, so the same receiver works on it.
- **Several balloons:** with *span search* on (Decoder panel, default) the receiver looks across the whole captured width every second
  and opens a channel on any narrow signal (up to 2.5 kHz wide, 8 dB over the noise, seen twice) more than 3.5 kHz from the
  channels it has. Channels that decode nothing for two minutes and whose signal is gone are closed. *Channels* sets how many run
  at once (1 to 8; 1 = the dial only). At 960 kS/s centred on 434.55 MHz the whole 434.2 - 434.9 MHz span is covered. Above
  2.4 MS/s the input is first decimated once to 2 - 4 MS/s for all channels, and only the middle 60 % of that band is searched.
- Doppler is negligible for balloons; the tone estimator follows the payload's own drift.

## Screens

- **Map:** every payload's track on the map widget (the ADS-B / sonde map, offline tiles work), a balloon marker (a diamond once it
  falls), the callsign and altitude; click to select, *follow* keeps the selected one centred.
- **Table:** every payload heard: type, altitude, position, climb rate (over the last 30 s - 3 min), temperature, battery,
  satellites, SNR, packet count, frequency offset, time since heard.
- **Packets:** the last 200 packets as UKHAS sentences (the form `horus_demod` prints), with SNR, offset and channel.
- **IDs and fields:** the two user tables (below), with *Apply* and *Defaults*.
- **Detail card:** the last packet of the selected payload: frequency, modem details (baud, tone spacing, Golay corrections), SNR in
  3 kHz, ID, sequence and time, position, altitude and the highest so far, climb, speed, satellites, temperature, battery, the custom
  fields, the distance, bearing and elevation from your location when it is set, the sentence (copy buttons).
- **Bottom row:** altitude against time of the selected payload, and the dial channel's averaged spectrum with the tones the
  demodulator follows (the frequency estimator's view).
- **Receiver tab:** the channels: offset, what they decode, the tones, SNR, packets and syncs.

## Tables (user-editable, nothing downloaded)

- **Payload IDs** - one `id, callsign` per line, `#` starts a comment (commas, tabs or spaces; the shape of horusdemodlib's
  `payload_id_list.txt`, so lines from it can be pasted). OnAir ships only `256, 4FSKTEST-V2`, the ID the wiki's test packet shows;
  add the IDs of the payloads you follow. Unknown IDs are shown as `ID <n>`.
- **Custom fields** - the JSON of horusdemodlib's `custom_field_list.json` as the wiki documents it:
  `{"CALL": {"comment": "...", "struct": "<hhBHxx", "fields": [["ascent_rate", "divide_by_100"], ...]}}`. Struct characters
  `<` `>` (byte order), `B` `b` `H` `h` `f` `x` and repeat counts; post-processing `none`, `battery_5v_byte`, `divide_by_10`,
  `divide_by_100` (unknown names are treated as `none`). The default is the wiki's example entry for `4FSKTEST-V2` (RS41ng fields).
  OnAir extension: `"sentence": ["name", ...]` names the RTTY sentence fields after the altitude for that callsign.
- Both are saved in the settings (`horusIds`, `horusFields`) and pushed to the receiver when it starts and on *Apply*.

## Sources and what was used

Clean-room from public documents, except where stated:

- horusdemodlib wiki, *2 Modem Details*: 4FSK at 100 baud, natural binary tone mapping, 270 Hz spacing, preamble `0x1B1B1B1B`, unique
  word `0x2424`, Golay (23,12) + interleaver + scrambler; the RTTY modes (7N1 / 7N2 / 8N2), sentence and CRC16.
- horusdemodlib wiki, *4 Packet Format Details*: the v1 and v2 layouts, the payload ID ranges.
- horusdemodlib wiki, *5 Customising a Horus Binary v2 Packet*: the custom field syntax and functions, the worked example and the
  published test packet and decoded line. These are the unit-test vectors: they fixed the CRC (polynomial 0x1021, initial value
  0xFFFF, no final XOR, stored little-endian), the little-endian field order and the 5 V / 255 battery scaling.
- The wiki says the Golay encoding, interleaving and scrambling are done by `horus_l2_encode_packet` in `src/horus_l2.c` and gives no
  parameters. Those were read from horusdemodlib's `src/horus_l2.c` and `src/golay23.c` (LGPL-2.1, David Rowe and Tomas Härdin) and
  implemented again in `core/src/horus/horus_proto.cpp` (no code copied; credited in THIRD_PARTY.md): the parity bits of all
  codewords packed most significant bit first after the data, the generator 0xC75, interleaver `j = (b * i) mod nbits` with b = 337
  (v1, 43 bytes) and 389 (v2, 63 bytes) over the bits after the unique word counted from the least significant bit of each byte, the
  additive scrambler (state 0x4a80, output = bit 1 xor bit 0, shifted right with the output into bit 14), interleave then scramble.
- sigidwiki *Horus Binary 4FSK - v2* for the example frequencies; the horusbinary wiki for 434.660 MHz.
- The demodulator (channel filter, tone estimator, non-coherent correlators, energy-peak symbol clock, four tone-to-bit mappings tried
  for the sync, RTTY framers) is our own design, not the codec2 modem.

**Not done, and why:**

- **LDPC for v2:** the task asked for v2 "with the documented LDPC". The wiki documents v2 with the same Golay chain as v1 and no LDPC
  for it, so there is nothing documented to implement; v2 here is the Golay frame (65 bytes with the unique word).
- **Horus Binary v3** (ASN.1, 32 - 128 bytes, CRC first): its ASN.1 definition is only in the `horusv3dev` branch; not decoded.
- **Wenet imagery (SSDV):** the SSDV packet format is public (Philip Heron's SSDV), but Wenet's own framing is not: its wiki gives the
  2FSK rate, 0x55 preamble, 0xABCDEF01 unique word, 256-byte packets and CRC, but the LDPC code (a rate 0.8 repeat-accumulate code by
  Bill Cowley) is only defined in `ldpc_enc.c` / `drs232_ldpc.c` and the v2 scrambling sequence only in `radio_wrappers.py` (GPL-3.0).
  Without them no Wenet packet can be received, so Wenet and the image decoder were left out.

## Performance (loopback)

`tests/horus/test_horus_loopback.cpp --sweep` (48 kS/s, +1.2 kHz frequency error, +1000 ppm clock error, 200 s per point, SNR in
3 kHz of the signal while it transmits):

| SNR (dB, 3 kHz) | -7 | -6 | -5 | -4 | -3 | -2 | -1 | 0 | +2 | +3 | +4 | +6 | +10 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Binary v2 | 57 % | 92 % | 98 % | 98 % | 98 % | 98 % | 98 % | 98 % | 98 % | 98 % | 98 % | 98 % | 98 % |
| Binary v1 | 58 % | 91 % | 97 % | 98 % | 98 % | 98 % | 98 % | 98 % | 98 % | 98 % | 98 % | 98 % | 98 % |
| RTTY 50 Bd 7N2 | 0 % | 6 % | 17 % | 56 % | 78 % | 94 % | 94 % | 94 % | 94 % | 94 % | 94 % | 94 % | 94 % |
| RTTY 100 Bd 7N2 | 0 % | 0 % | 0 % | 0 % | 9 % | 24 % | 68 % | 91 % | 97 % | 97 % | 97 % | 97 % | 97 % |
| RTTY 300 Bd 8N2 | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 0 % | 13 % | 41 % | 72 % | 99 % | 99 % |

The missing few per cent at high SNR is the first packet, sent before the tone estimator has seen the signal. Binary decodes to
about -7 dB (50 %), close to what hard-decision Golay on non-coherent 4FSK allows. The SNR shown with each packet comes from the
averaged spectrum and reads about 1 dB low on a signal that pauses between packets.

Other checks (tests/horus): a mirrored spectrum (LSB), +3 kHz offset, -3000 ppm clock, RTTY 300 Bd mirrored at +2000 ppm, noise
alone (nothing decodes), three balloons in one 192 kS/s capture (v2 on the dial, v1 +12 kHz, RTTY -18 kHz: the span search opens
channels for the other two), 5 MS/s with the shared decimator, the track and climb rate, the results file round trip and the tables.

**Not verified on real signals:** no Horus recording or live flight was available, so everything is checked against our own
generator (which uses the same layer 2 code as the receiver) and the wiki's test vectors. The layer 2 constants, the order of the two
bits in a symbol and the tone order (all four mappings and both byte bit orders are tried, so a wrong guess there costs nothing) and
the RTTY framing against a real tracker are the parts a recording should confirm first.

## Test signals

Synthetic source, `SynthConfig` options (`horus_gen.h`): `modeOpt[0]` 0 v2, 1 v1, 2 RTTY 100 Bd 7N2, 3 RTTY 300 Bd 8N2, 4 RTTY 50 Bd
7N2, 5 three balloons; `modeOpt[1]` tone spacing; `modeOpt[2]` 1 = mirrored; `modeVal[0]` clock error in ppm; `snrDb` per balloon in
3 kHz; `cfoHz`. The simulated flight starts at Kaunas, climbs at 5 m/s and drifts east.

## Files

`core/include/dect2/horus/` (`horus_modes.h`, `horus_proto.h`, `horus_tel.h`, `horus_rx.h`, `horus_gen.h`), `core/src/horus/`
(`horus_proto.cpp` CRC / Golay / layer 2 / packets / sentences, `horus_fields.cpp` the tables, `horus_modem.h/.cpp` the demodulators,
`horus_rx.cpp` the receiver, `horus_gen.cpp` the generator), `app/horus/horus_ui.cpp`, `tests/horus/`.
