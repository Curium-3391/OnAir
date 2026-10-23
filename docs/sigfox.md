# Sigfox

Receive only. Mode "Sigfox" (IoT and sensors), family 149, engine standard 151. Code: `core/src/sigfox/` (frame layer, mode wrapper, test signal) and the demodulator in `core/src/ism/ism_sigfox.cpp`, which the ISM sensors mode also uses on its bands. Panel: `app/sigfox/`, tests: `tests/sigfox/` and `tests/ism/test_ism_sigfox.cpp`.

## Frequencies

From the public Sigfox radio specification (EP-SPECS Rev. 1.3, February 2019, build.sigfox.com): the uplink uses a 200 kHz macro channel around the zone's centre.

| Zone | Uplink | Downlink | Uplink rate | Where |
|---|---|---|---|---|
| RC1 | 868.130 MHz | 869.525 MHz | 100 bit/s | Europe, Middle East, Africa |
| RC2 | 902.200 MHz | 905.200 MHz | 600 bit/s | USA, Mexico, Brazil |
| RC3 | 923.200 MHz | 922.200 MHz | 100 bit/s | Japan |
| RC4 | 920.800 MHz | 922.300 MHz | 600 bit/s | Latin America, Australia, New Zealand, Asia-Pacific |
| RC5 | 923.300 MHz | 922.300 MHz | 100 bit/s | South Korea |
| RC6 | 865.200 MHz | 866.300 MHz | 100 bit/s | India |

The receiver tries 100 and 600 bit/s in every zone. The downlink (GFSK 600 bit/s, whitened BCH frames) is not decoded.

## What is decoded

| Field | How |
|---|---|
| Burst | D-BPSK, about 100 Hz wide (600 Hz at 600 bit/s), anywhere in +-96 kHz of the dial; found as a narrow carrier on a 100 Hz spectrogram |
| Preamble, frame type | 19 bits `1010...1`, then a 13 bit frame type (Table 3-3) naming the container size (8, 9, 12, 16 or 20 bytes) and the rank; up to 1 bit error is corrected (the 18 values are at least 3 bits apart) |
| Repetitions | a message is one frame or three: rank 1 plain, rank 2 under the convolution code 1 + X + X^2, rank 3 under 1 + X^2; they are merged into one message with their frequencies |
| Container | length indicator (2 bits), downlink request flag BF, REP, message counter (12 bits), device ID (32 bits, bytes in reverse order), payload 0 - 12 bytes or a single bit (Table 3-1), authentication code 2 - 5 bytes |
| CRC | X^16 + X^12 + X^5 + 1 over the container, inverted; checked: a frame is only shown when it matches |
| Authentication | shown as received, never checked: it is AES-128 with the device's secret key |
| Payload | hex; often encrypted by the application, never decrypted |

## Not stated in the public text

- The CRC register's start value: 0 is used. It reproduces the one public example uplink we have (the README of the renard project: `aaaaa35f01a0db334d00abad1deadc32758a`, device 004D33DB, counter 0x1A0, payload ABAD1DEA), which is a test in `tests/sigfox/test_sigfox_proto.cpp`.
- The convolution coder's start state: 0 is assumed (ranks 2 and 3 are not verified against a real device).
- Which bit value is a phase change in the D-BPSK: the receiver tries both, the frame type and the CRC decide.

## Test signal

`makeSigfoxSynth` (see `sigfox_gen.h`): four made-up devices in turn (4 bytes, a single bit, 12 bytes, 3 bytes asking for a downlink), each message as three frames on different frequencies within +-80 kHz. Options: 600 bit/s, one frame per message, a carrier drift during each frame (Hz/s). The authentication bytes are made up.

Tests: `tests/sigfox/test_sigfox_rx.cpp` (clean, 14 dB in 100 Hz, +-1.5 kHz radio offset, 15 Hz/s drift, 600 bit/s, one frame at 2.048 Msps, noise only), `tests/sigfox/test_sigfox_proto.cpp` (the example frame, the tables, every payload length and rank, the convolution codes, a damaged frame).

## Not verified

No real Sigfox recording was available: only the generated signal and one public example frame (bits, not IQ). The demodulator's sensitivity on real bursts, the convolution coder's start state and the phase change convention are not verified.
