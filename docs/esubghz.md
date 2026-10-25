# Flipper Zero Sub-GHz chat

Talk to Flipper Zero devices that run the **Enhanced Sub-GHz Chat** app ([twisted-pear/esubghz_chat](https://github.com/twisted-pear/esubghz_chat)) or the
firmware's own CLI Sub-GHz chat. OnAir receives with any radio and sends through a radio that can transmit (HackRF, PlutoSDR, LimeSDR, USRP, bladeRF).
The mode is in the *IoT & sensors* group, and its screen is **Flipper chat**.

## Using it

1. On each Flipper: Apps > Sub-GHz > ESubGhzChat, type the frequency (433920000 is the app's default), choose the encryption (none, a password,
   a hex key or one read from another Flipper over NFC) and enter the chat.
2. In OnAir: choose *Flipper Zero Sub-GHz chat*, set the same frequency (the Tuner has 433.92, 315, 390, 868.35 and 915 MHz buttons) and press Start.
   Set *Encryption* to the same password, or to *Hex key* with the key the Flipper shows under Key > Display. The password or key stays in memory only:
   it is never saved.
3. Type a line and press Enter. It goes out as `"<Name>: <text>"` (the *Name* box, "OnAir" at first), encrypted when a key is set.

The chat shows what was heard and what was sent, in order. A heard line's tooltip has its level, SNR, frequency offset, packet count and octets. Lines
that do not open with the key are named, not shown; a repeat of an encrypted line is dropped, as a Flipper drops it. The *Packets* view lists every
packet with its CRC result and payload in hex.

**Half duplex.** A HackRF cannot receive and transmit at once. When the receiving radio is also the one that sends, the receiver stops for the send
(about a second with a HackRF) and starts again after it; the chat says so beside the Send button. A line a Flipper sends during that second is
missed. With a second radio for receiving, or a full-duplex one, the receiver keeps listening.

**Sending.** The first send asks for the same one-time notice as the Transmit window. The radio is claimed for the send and given back after it;
lines typed while one is going out wait their turn. The signal is made 300 kHz from the radio's centre, so the radio's own carrier leak stays off the
chat frequency. The *TX gain* slider is the radio's own TX gain (a Flipper in the same room needs very little); the HackRF's +14 dB amplifier stays
off. You need the right to use the frequency where you are.

The general Transmit window also has the mode (`esubghz`): it sends one plain line, typed in its Text box, once.

## The format

From the app's and the firmware's sources (the CC1101 preset `FuriHalSubGhzPresetGFSK9_99KbAsync` over the chip's reset values, and
`SubGhzTxRxWorker`):

- GFSK, 9.99 kbaud (MDMCFG4 / MDMCFG3 = 0xC8 / 0x93), 19.04 kHz deviation (DEVIATN 0x34), Gaussian BT 0.5, a 1 is the upper frequency.
- Packet: 4 octets of preamble 0xAA, sync word 0x46 0x4C, one length octet, up to 60 octets of payload, CRC-16 (polynomial 0x8005, start 0xFFFF, over
  the length and the payload, high octet first). No whitening and no FEC, MSB first.
- A message longer than 60 octets goes out as several packets about 10 ms apart; the receiver joins what arrives until 500 ms pass without a packet.
  A message is at most 1024 octets.
- Plain message: `"<name>: <text>\r\n"`. The app also sends `"<name> joined chat."` and `"<name> left chat."`.
- Encrypted message: run ID (8 octets) | counter (4, big endian) | IV (12) | tag (16) | ciphertext. AES-256-GCM, the run ID and counter are the
  additional authenticated data, the key is SHA-256 of the password or the 32 octets typed in hex. A receiver drops a message whose counter is not
  above the last one it accepted from that run ID. OnAir picks a random run ID each time it starts and counts from 1.

## Receiver

The channel is mixed to DC, low-passed (65 kHz) and decimated to about 200 ksps. An energy detector marks each packet; the burst is FM demodulated,
its carrier offset taken as the middle of the two FSK frequencies, the bit timing found on the preamble, and the sync word searched with both
frequency polarities (a spectrum inverted by swapped I and Q still decodes, and is marked). The radio is tuned 300 kHz above the chat frequency to keep
the channel clear of the DC spike; 200 ksps is enough. In the tests it decodes every line at 9 dB SNR in 100 kHz, with offsets of up to +-15 kHz.

## Tested

With a HackRF One and a Flipper Zero on 433.92 MHz: plain lines from the Flipper were received, and plain lines from OnAir (one packet and two packets)
arrived complete on the Flipper; in the app, plain and password-encrypted chats worked both ways, with the HackRF's receiver paused and started
again around each send. The encryption was also checked against the GCM specification's AES-256 test vectors and against Python's `cryptography`
AES-GCM.

## Tools

`esubghztool rx <file.cs8|file.cf32> [--rate Hz] [--offset Hz] [--password P | --hex KEY] [--packets]` reads a recording (`hackrf_transfer -r` writes
.cs8); `esubghztool tx <out.cs8> "<line>" [--rate Hz] [--if Hz] [--password P | --hex KEY]` writes one message for `hackrf_transfer -t`.
