# NFC / RFID at 13.56 MHz

OnAir's NFC mode listens to the 13.56 MHz contactless band and decodes the three public high-frequency standards as far as
their public specifications allow. It is **receive only** in normal use; an **active reader** transmitter exists behind the TX
gate (Phase 2 below). Nothing is cloned, no encrypted content is decoded, and no keys are attacked.

This is clean-room work from public summaries of ISO/IEC 14443-2/-3, ISO/IEC 14443-4, ISO/IEC 15693 and the NFC Forum Type 2 /
NDEF descriptions. No proprietary stack or GPL source was read.

## The physics

- **Carrier**: 13.56 MHz, inside the 13.553–13.567 MHz ISM band. The reader (PCD) runs the carrier continuously and powers the
  tag (PICC) through it; the tag has no transmitter of its own.
- **Reader → tag**: amplitude-shift keying of the carrier.
  - *ISO/IEC 14443 Type A*: 100 % ASK, **modified Miller** coding, 106 kbit/s (fc/128). A bit is 128 carrier cycles (~9.44 µs);
    a "pause" is a short gap in the carrier. A logic 1 puts the pause in the second half of the bit; a logic 0 puts it in the
    first half when it follows another 0 (or starts the frame), and omits it when it follows a 1. Each byte is sent least
    significant bit first with an odd parity bit after it.
  - *ISO/IEC 14443 Type B*: 10 % ASK, NRZ, 106 kbit/s.
  - *ISO/IEC 15693*: 10 % or 100 % ASK with **pulse-position** coding, either 1-of-256 (long range, one pause position in 256 per
    byte) or 1-of-4 (fast, two bits per symbol).
- **Tag → reader**: the tag cannot transmit; it **load-modulates** the reader's field, which the reader sees as sidebands around
  the carrier.
  - *ISO/IEC 14443 A/B*: a subcarrier at fc/16 = **847.5 kHz**. Type A switches the subcarrier on and off (OOK) with Manchester
    coding; Type B phase-modulates it (BPSK), NRZ-L. 106 kbit/s.
  - *ISO/IEC 15693*: a subcarrier at fc/32 = **423.75 kHz** (single subcarrier) or a second one at 484.28 kHz (double
    subcarrier), Manchester, at the high (~26 kbit/s) or low data rate.

Because the answer sits on sidebands at ±847.5 kHz, the receiver must sample fast enough to see them. The mode asks for 4 Msps
and refuses below 2 Msps (±1 MHz still holds the 847.5 kHz subcarrier; the transmit side keeps 2.4 Msps). Any HF-capable receiver works: RTL-SDR V4 in direct-HF mode, HackRF,
Airspy HF+, or an RX888.

## What OnAir decodes

ISO/IEC 14443 Type A: REQA / WUPA, ATQA, the anticollision loop and SELECT at cascade levels 1–3 (with the 0x88 cascade tag),
SAK and the UID size (single 4-byte, double 7-byte, triple 10-byte), the BCC and CRC_A checks, HLTA, RATS / ATS, and generic
APDU framing shown as hex. For NFC Forum Type 2 tags (NTAG, MIFARE Ultralight) the bytes returned by READ (0x30) are parsed for
an NDEF TLV, and text and URI records are decoded.

ISO/IEC 14443 Type B: REQB / WUPB and ATQB (PUPI, application data, protocol info) with CRC_B.

ISO/IEC 15693: the inventory request, the inventory response (DSFID + 64-bit UID, shown most-significant byte first, starting
0xE0 with the manufacturer byte named), and Get System Information (AFI, DSFID, block layout, IC reference).

The panel shows a **frame log** (time, reader/tag, protocol, bytes, decoded meaning, CRC / parity state), a **tag table**
(UID, type, ATQA, SAK, ATS, how many times seen) and the **decoded NDEF** records.

A synthetic generator produces a scripted reader↔tag exchange (Type A poll/select/read with an NDEF URI, a 15693 inventory, a
Type B REQB/ATQB) so the receiver and the loopback tests have a signal without any real card present. The loopback tests check a
clean capture, a 20 dB-SNR capture, a capture with a 12 kHz carrier offset, and 40 noise-only runs that must not fabricate a
CRC-valid frame.

## Weak recordings, and what is not decoded (#271)

A phone or reader held next to a small SDR antenna gives a strong field and almost no answer: at 8 bits the tag's load modulation is
far below one quantisation step (a real 2 Msps HackRF capture of a phone reading a MIFARE Classic card showed a carrier of about 4 LSB
and no subcarrier at all). OnAir therefore decodes the **reader's frames exactly** (REQA / WUPA as 7-bit frames, anticollision,
SELECT with the UID, AUTH, HLTA, RATS and the rest, with the end-of-communication pause read as the standard defines it) and fills
the tag table from the reader's SELECT when the tag's own answer is not heard. A subcarrier burst is shown as a tag frame only when it
has the shape of one (an ATQA, a UID with its BCC, a CRC_A frame, a long frame with at most a stray parity error), so the
quantisation noise and the ringing after a pause do not produce frames.

A MIFARE Classic authentication (AUTH A / B, `60`/`61` + block) is named in the log. Everything after it, until the next wake-up or
halt (the reader's nonces, the reads and writes, and their parity bits), is Crypto1-encrypted: those frames are shown as the bytes
that were sent, labelled "encrypted (MIFARE Classic Crypto1), not decoded". OnAir does not search for or use keys.

## Antenna / loop notes

13.56 MHz is a **near-field** band: you couple to the magnetic field of a reader–tag pair that is a few centimetres across, so
range is centimetres, not metres. Use a small **tuned loop** (a few turns, resonated near 13.56 MHz with a trimmer) placed close
to the reader. A direct-sampling SDR (RTL-SDR V4, RX888, Airspy HF+) with a short loop into the HF input works; a broadband whip
will usually hear nothing.

## Phase 2: the active reader (gated transmit)

With a transmit-capable radio (a HackRF, plus a second radio — an RTL-SDR V4 — for the answer, or a full-duplex radio) OnAir can
act as a simple reader. It generates the 13.56 MHz carrier with 100 % ASK modified-Miller frames for REQA, anticollision CL1 /
CL2, SELECT, READ (Type 2) and GET_VERSION. **The carrier stays continuous** between frames (only the brief Miller pauses), so a
tag stays powered; the waveform is pre-built and looped.

Transmit is tightly gated. It only runs through OnAir's existing TX path: the operator must **Arm**, confirm the licence /
notice, and the radio must be tuned inside the **13.553–13.567 MHz ISM band** (`nfcTxFrequencyAllowed()`). A **low amplitude
cap** is applied in the generator as a courtesy power limit, and the panel's output trim can only lower it further. In the test
suite the session uses the dry-run null sink, so **nothing is ever keyed by a test**.

Antenna for transmit: a small tuned **loop** matched to the HackRF, kept close to the tag. Keep the TX loop and the RX loop
**physically separated and/or detuned from each other** so the reader's own strong carrier does not swamp the RX front end — the
wanted answer is ~60–80 dB weaker than the carrier. A full-duplex radio avoids the two-loop problem but still needs carrier
cancellation to hear the load modulation.

## Limits

- Range is centimetres (near field). This is not a long-range reader.
- Encrypted or proprietary content (MIFARE Classic Crypto1 sectors, DESFire application data, etc.) is **never** decoded, and no
  key recovery is attempted. Only the clear anticollision / selection layer, open NDEF, and the fields the public specs define
  are shown.
- No cloning or emulation: the active reader polls and reads; it does not impersonate a tag.
- **Pet / animal microchips are not in this band.** FDX-B implants (ISO 11784 / 11785) work at **134.2 kHz**, far below 13.56 MHz
  and out of reach of a HackRF or RTL-SDR tuned here. FDX-B is a 134.2 kHz ASK/FSK ID frame (a preamble, then a 128-bit block
  with a 38-bit animal ID, a country code, and a CRC-16), per the public ISO 11784 / 11785 description. A radio or downconverter
  that reaches 134.2 kHz (an RTL-SDR V4 in direct-HF mode, an RX888, or a soundcard behind an LF loop) could decode it; OnAir
  documents it here as a separate low-frequency extra and does not fold it into the 13.56 MHz mode.

## What is unverified

The protocol layer (CRCs, frame parsing, NDEF) and the synthetic-to-receiver loopback are covered by tests. The demodulator's
behaviour on **real captures** — real pulse shapes, weak real load modulation (tens of dB below the carrier), reader/tag timing,
and real noise — has **not** been checked against hardware. Thresholds, filter bandwidths and the Manchester / Miller timing are
the author's own engineering choices from the public timing rules, and may need tuning against a real reader and tag.
