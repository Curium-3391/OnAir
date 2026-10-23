# ISDB-S (BS satellite television, Japan)

Mode "ISDB-S (BS)": family 154, engine standard 156 (active standard 155), poll slot 154. Receive only, clear streams only.

**Not tested on a real signal.** No IQ recording of a BS transponder was available. Everything below has been checked only against this code's own generator.

## What it needs

* A dish pointed at 110 degrees east and an LNB with the usual 10.678 GHz oscillator. BS-1 at 11727.48 MHz comes out at 1049.48 MHz on the cable; the odd channels BS-1 to BS-23 are 38.36 MHz apart.
* A radio that samples at **39 Msample/s or more**. The transponder runs at 28.86 Msymbol/s with roll-off 0.35, so it is 34.5 MHz wide. A HackRF (20 Msps), an RTL-SDR or an Airspy cannot do it; a PlutoSDR, bladeRF, LimeSDR or USRP can. The mode asks for 48 Msps.
* Power for the LNB (bias tee) from outside. Nothing here switches 13 / 18 V.

Scrambled (conditional access) streams come out as they were sent, still scrambled. Nothing is descrambled and there is no key handling.

## Sources

* **ITU-R BO.1408-1** (04/2002, free from itu.int): the ITU description of ISDB-S. Annex 1 describes the system and Appendix 1 gives the BS parameters. Most of the transmission side below comes from it.
* **ARIB TR-B15** v4.6 (operational guidelines, English): TMCC use, dummy slots, relative TS numbers, the phase-reference burst.
* **ARIB STD-B21** v5.12 (receiver, English): front end, energy dispersal, the 48 slot transport.
* ARIB STD-B20, the transmission standard itself, is only in Japanese and costs money. It was **not** used.

No other implementation was looked at.

## What is now from the documents

| Item | Value used | Source |
|---|---|---|
| Symbol rate, shaping | 28.86 MBd; root raised cosine, roll-off 0.35 | BO.1408 Table 4 (28.9 MBd in 34.5 MHz), section 9 |
| Frame | 39936 symbols: 192 header symbols, then 38976 data symbols with a 4 symbol burst after every 203 | Table 3; section 7 (a burst every 203 symbols when the sync byte is not sent) |
| Slots | 48 a frame, one 204 byte RS packet each; only 203 bytes are sent (the 0x47 is put back by the receiver) | 3.1, section 5, Table 3 |
| Super-frame | 8 frames; the slot layout stays the same for the whole super-frame | Table 3, section 5 |
| Slot order | Groups go from the most efficient to the least, at most 4 groups. Streams of one modulation sit together, in ascending relative number order | 3.2, Fig. 8 note; TR-B15 6.1.1, 6.2.3 |
| Dummy slots | Slots that carry nothing take up the air time of the slower modulations and are not sent. TMCC slot counts and relative numbers include them | 3.2, eq. 4, Fig. 4, section 8; TR-B15 6.2.2, 6.2.3, Table 7.1.5 |
| Outer code | RS(204,188) over the whole packet, sync byte included | section 2 |
| Energy dispersal | PRBS 1 + x^14 + x^15, loaded with 100101010000000 at the second byte of every super-frame, skipping each slot's first byte. It runs over the RS-coded data in frame/slot order | section 4, Fig. 5; B21 4.4.5 |
| Interleaving | Byte interleaving across the 8 frames of a super-frame: the 8 x 203 bytes of a slot position are read out by byte, frame after frame | section 5, Fig. 6 |
| TMCC contents | 384 bits: change counter 5, 4 x (word 4 + count 6), 48 relative TS numbers of 3 bits, 8 TS ids of 16 bits, emergency flag 1, uplink 4, reserved 62. Unused groups use word 1111 with count 0; unused relative numbers use TS id 0xFFFF | 6.1, Table 1, Table 3; TR-B15 6.2.4 |
| TMCC coding | RS(64,48), the main code shortened, no interleaving. It is split into 8 blocks of 64 bits, one per frame. Energy dispersal (the same PRBS) starts right after W1 of the first frame and skips the sync words | 6.2, 6.4, Table 3 |
| Sync words | W1 = 1B95 starts every frame's header. W2 = A340 ends the first frame's TMCC block and W3 = 5CBF ends the others. The first 6 bits of each word only flush the code's memory, so 10 bits of each are a fixed pattern | 6.3, Fig. 11 |
| Header | W1 + 64 TMCC bits + W2/W3 = 96 bits through the rate 1/2 code, BPSK (TMCC and bursts use the most robust scheme): 192 symbols | 6.2, section 7, Fig. 12 |
| Inner code | One K = 7 (171,133) code over the frame's whole TDM stream: header, then the packet-carrying slots in order. The puncturing restarts at each group and the patterns match DVB-S. BPSK sends P1 then P0; QPSK puts P1 on I and P0 on Q | section 9, Figs. 13, 14, Table 2 and its note |
| TC8PSK | Of each bit pair (B1, B0) of a byte, B0 goes through the code (giving X, Y) and B1 goes out uncoded. Label (W, Y, X): 000 at 0 degrees, then 001, 011, 010 every 45 degrees; W = 1 is the opposite point | Fig. 14, Fig. 15 a |
| QPSK / BPSK maps | QPSK (P1, P0) = 00 at 45 degrees, 10 at 135, 11 at 225, 01 at 315; BPSK 0 is +1 and 1 is -1 | Fig. 15 b, c |
| Burst | 4 BPSK symbols, spread by a 9th order PN sequence. Receivers must not assume the burst is unmodulated | BO.1408 section 7; TR-B15 6.5 |
| TMCC timing | A new TMCC goes out two super-frames before the layout changes, with the change counter stepped | TR-B15 6.2.1 |
| Net rates | The code gives BO.1408 Table 4's rates (e.g. TC8PSK 52.2, QPSK 1/2 26.1, BPSK 13.0 Mbit/s); a test checks them | Table 4 |

## Still this code's own reading (may be wrong on the air)

* **Order of fields inside the TMCC.** Field widths and the overall order are from BO.1408. Not given: whether a combination sends the word or the count first (here the word first), and the order of the emergency flag, the uplink bits and the reserved bits (here in that order).
* **Bit and byte order of the TMCC through RS(64,48).** Read here as MSB first, parity last, like the main signal.
* **Where the dummy slots sit inside a group.** Here, within each unit of slots (2 for QPSK 1/2, 3 for 2/3, 4 for 3/4 and BPSK, 6 for 5/6, 8 for 7/8), the packet-carrying slots come first and the dummies follow, as BO.1408 Fig. 4 draws a single unit. Where they go in larger groups is not stated.
* **Whether the PRBS keeps clocking over the bytes it skips.** For the main signal it clocks through the sync bytes (as DVB-S does). For the TMCC it clocks only over TMCC bits, not over the sync words.
* **Interleaving with several slots of one modulation.** Fig. 6 draws one slot for each modulation; here every slot position is interleaved across the 8 frames on its own.
* **The burst PN sequence.** Only "9th order" is known. The generator uses 1 + x^5 + x^9 started at all ones in every frame. The receiver does not use it: carrier phase comes from the squared bursts plus W1, so data on the bursts would not hurt it (one test sends random bursts).
* **Applying the TMCC two super-frames late.** TR-B15 describes this operating rule; STD-B20's exact timing was not seen. Right after lock, the latest decoded TMCC is used until the one for the current super-frame is in.
* **Emergency, uplink and reserved bits.** They are decoded but not acted on.

## Chain

DVB-S front end (matched filter, timing loop; the loop is narrowed once frames lock, because a symbol slip at low Es/N0 costs a whole super-frame) -> carrier from the 8th power, tried together with 0 Hz and now and then with a grid over +-1.9 MHz -> frame sync on the 20 fixed coded symbols of W1 and of W2/W3, one frame apart -> fine carrier from the squared bursts. These leave a 1/414 cycle per symbol ambiguity, which the header Viterbi settles because it decodes only at the right frequency. Phase is tracked from W1 and the 192 squared bursts -> header Viterbi (W1 and W2/W3 forced, 64 TMCC bits; W2 marks the first frame) -> RS(64,48) of the super-frame's TMCC -> Viterbi over the selected stream's slots (windows with 240 steps of margin, ending on the next frame's W1). For TC8PSK the uncoded bit is decided once the coded path is known -> de-interleaving across the 8 frames, energy dispersal, RS(204,188) -> transport stream to the TV player, service list and recorder (as for DVB-S).

Test signal (`isdbs::Generator`, also the mode's built-in synthetic source): the same chain backwards, plus carrier offset, symbol clock offset, noise, and optional random burst data. All 48 slots must be allocated, as on the air.

## Tests

`tests/isdbs/test_isdbs.cpp` (ctest `isdbs`) covers:

* the documents' numbers (Table 3 and 4, units of slots);
* TMCC pack/unpack, group order, dummy slots and 0xFFFF;
* RS(64,48), including 8 corrected byte errors and a 12-byte-error case that is reported;
* W2/W3 coding, PRBS period, 8PSK map;
* a frame coding loop (FrameEncoder -> Viterbi) noise-free and at 11 dB;
* the whole receiver against the generator: three streams in three modulations (each one decoded in turn); 16 dB with 120 kHz carrier and 15 ppm clock offset; 40 Msample/s input; -1.7 MHz, +400 kHz, -950 kHz and +70 kHz carrier offsets at low Es/N0 (BPSK 1/2 at 3 dB); random bursts; a TMCC change with no lost packet; noise only must not lock.

`test_isdbs sweep` prints thresholds, and `test_isdbs one <mod> <dB> <Hz>` runs one case. On generated signals at 2 samples per symbol, the lowest Es/N0 tried with no Reed-Solomon failure was: TC8PSK 2/3 9 dB, QPSK 1/2 3 dB (2 dB fails), QPSK 3/4 6 dB, QPSK 7/8 9 dB, BPSK 1/2 1 dB. Those are the lowest points the sweep tries; only QPSK 1/2 was tried below its figure. With +-60 ppm clock offset and +-1.7 MHz carrier offset TC8PSK at 14 dB was clean.

`tests/app/test_isdbs_mode.cpp` checks the wiring in the sources. There is no engine-level test (the engine path was only compile-checked).

## Known limits

* Acquisition below about 4 dB Es/N0 with a carrier offset beyond 70 kHz relies on the grid search, which runs only on every fourth failed window.
* One stream is decoded at a time (selectable), and only its slots go through the Viterbi decoder.
* No ISDB-specific service information is parsed beyond what the shared MPEG-TS layer understands. ARIB SDT/EIT with Japanese text is not decoded.
* Nothing has been checked against a real BS transponder.
