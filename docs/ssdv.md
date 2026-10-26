# SSDV pictures

Mode list: **WEATHER & SCIENCE > SSDV pictures**. Receive only.

SSDV (Slow Scan Digital Video) is the picture format of high-altitude balloons and some amateur satellites: a baseline JPEG cut into 256-byte
packets (sync 0x55, type 0x66 with Reed-Solomon (255,223) or 0x67 without, callsign, image id, packet number, picture size, the entropy-coded
data, CRC-32 and 32 parity bytes). OnAir receives the packets from an FM / FSK channel, builds the picture while it arrives, and saves it.

## What it does

- One FM channel (the packet front end: offset, AFC, channel filter, discriminator) feeds a bank of bit slicers that run at the same time,
  each with its own clock loop and both frequency polarities:
  - **plain bit streams** at 1200, 2400, 4800, 9600 and 19200 baud (2-FSK / GFSK / MSK, NRZ);
  - **RTTY-framed bytes** (start bit, 8 data bits, one or two stop bits) at 300, 600 and 1200 baud, as sent by some UKHAS balloon trackers.
- The sync word 0x55 0x66 / 0x55 0x67 (one bit error allowed) starts a packet. The CRC-32 and the Reed-Solomon code decide: up to 16 wrong bytes per
  packet are corrected; anything else is dropped.
- Good packets are kept per picture (callsign and image id). The JPEG is rebuilt as packets come in, in any order, repeats ignored; **missing
  packets show as grey** until they arrive on a later pass.
- A finished picture (all packets up to the last one) is saved as a **JPEG** in `<Save pictures to>/SSDV`; a picture that stays incomplete for 20 s is
  saved as `...-partial.jpg` and replaced if more packets arrive. "Save PNG" writes a PNG copy to the picture folder.
- Advanced (the Tuner section): a fixed speed instead of Automatic.

## Not handled

These are not handled: SSDV inside AX.25 / IL2P or G3RUH scrambled frames (some satellites), Wenet (115 kbaud 2-FSK with LDPC; needs a much wider
channel than this front end), Horus 4-FSK layers, LoRa SSDV, packets of other sizes than 256 bytes. The Horus balloons mode decodes the Horus telemetry,
not the pictures.

## Licence

The packet check, the Reed-Solomon code and the JPEG rebuild are the code of **fsphil/ssdv** (Philip Heron, M0RPH; GPL-3.0-or-later), vendored in
`third_party/ssdv` (the Reed-Solomon routines are Phil Karn's, LGPL), see THIRD_PARTY.md and the credits in the program. The only change to the
original files is that its diagnostic messages to stderr are compiled out. JPEG pictures are shown with stb_image (public domain / MIT).

## How it was checked

With the SSDV encoder's own packets of a test picture (the encoder of the same library) as 2-FSK / GFSK in the generator: clean at 1200 - 19200 baud
and RTTY-framed at 300 / 600 baud; carrier offsets of +-3 kHz, swapped frequencies, a clock off by 1000 ppm, a carrier drift of 200 Hz/s, noise (Eb/N0
16 dB), all of them together, packets with 6 wrong bytes (the Reed-Solomon code repairs them); a fixed wrong speed gives nothing; noise gives nothing.
The picture counts as received when its JPEG is byte for byte the one the packets give directly. No real recording was used.

Tests: `ctest -R ssdv` (`tests/ssdv`), wiring in `tests/app/test_ssdv_mode.cpp`.
