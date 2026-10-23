# ISDB-S (BS satellite television, Japan)

Mode "ISDB-S (BS)": family 154, engine standard 156 (active standard 155), poll slot 154. Receive only, clear streams only.

## What it needs

* A dish on 110 degrees east, an LNB with the usual 10.678 GHz oscillator (BS-1 at 11727.48 MHz is 1049.48 MHz on the cable; the odd channels BS-1 to BS-23 step by 38.36 MHz).
* A radio that samples **39 Msample/s or more**. The transponder is 28.86 Msymbol/s with roll-off 0.35 (34.5 MHz wide). A HackRF (20 Msps), RTL-SDR and Airspy cannot do it; a PlutoSDR, bladeRF, LimeSDR or USRP can. The mode asks for 48 Msps.
* Power for the LNB (bias tee) from outside. Nothing here switches 13 / 18 V.

Scrambled (conditional access) streams are received as they are, still scrambled. Nothing is descrambled and no key handling exists.

## Chain

DVB-S front end (matched filter, timing loop) -> carrier from the 8th power of the symbols (BPSK, QPSK and 8PSK all go to one point), tried together with 0 Hz and, now and then, with a grid over +-1.9 MHz -> frame sync by the 32 symbol BPSK word, two frames apart -> carrier frequency and phase from the 192 burst blocks (4 known BPSK symbols each; their phasors are 207 symbols apart so the two halves of the sync word settle which multiple of 1/207 the carrier is) -> TMCC (64 bits a frame: change counter, modulation of up to four slot groups, slot counts, the stream of every slot, transport stream ids; 8 frames make a super-frame) -> for each slot of the chosen stream a Viterbi decoder (BPSK / QPSK, K = 7 with the DVB-S puncturing) or a trellis decoder (TC8PSK, 8 states), byte sync, convolutional de-interleaver, Reed-Solomon (204,188), de-scrambling -> packets of all slots of a stream put back in order -> the TV player, service list, recorder (as DVB-S does).

Test signal (`isdbs::Generator`, also the built-in synthetic source of the mode): the same chain backwards, root raised cosine shaping, carrier offset, symbol clock offset, noise.

## What is from the standard and what is not

Taken from public descriptions of ARIB STD-B20 (not from the text of the standard, not from any other implementation): symbol rate and roll-off; the frame of 39936 symbols with 192 header symbols and 192 blocks of 203 data + 4 burst symbols; 48 slots of 4 blocks (block j in slot j mod 48); 8 frames per super-frame; BPSK header; BPSK / QPSK / TC8PSK slots in hierarchical groups; K = 7 (171,133) with DVB-S puncturing; Reed-Solomon (204,188); MPEG-2 transport stream.

**Chosen here and probably different on the air:**

* the 32 symbol sync word, the Walsh frame number words, the bit layout and coding of the TMCC (here 48 information bits + CRC-16, each bit sent twice), the burst PN sequence and the filler of unused slots;
* the 8 state 8PSK trellis code (Ungerboeck, h0 = 11, h1 = 02, h2 = 04 octal) and its labelling; the real BS code may differ;
* energy dispersal and the byte interleaver are the DVB-S ones (groups of 8 packets, I = 12); the real ones may not be;
* how packets lie on the slots: here every slot is its own continuous coded stream carrying 204 byte blocks with the sync byte kept (the real system removes the sync byte and fits one packet to a slot of TC8PSK); packets of the slots of one stream are merged by their position in the slot streams, which needs all slots of a stream to share one modulation (a stream with mixed slots is marked "mixed" and may arrive out of order);
* the TMCC change rule: a new TMCC takes effect with the first frame of a super-frame.

A real BS transponder is therefore **not expected to decode**. The code, the generator and the tests agree with each other, and with the public numbers above, nothing more.

## Tests

`tests/isdbs/test_isdbs.cpp` (ctest `isdbs`, about 6 s): TMCC pack / check, sync and Walsh words, the slot chain for every modulation (noise-free, with noise, joining late), the whole receiver against the generator (clean hierarchical signal with three streams, 16 dB with 120 kHz carrier offset and 15 ppm clock offset, 40 Msample/s input, offsets of -1.7 MHz / +400 kHz / -950 kHz at low Es/N0, a TMCC change in mid-stream, noise only must not lock). `test_isdbs sweep` prints thresholds: TC8PSK 2/3 decodes from 10 dB Es/N0, QPSK 1/2 from about 4 dB, QPSK 3/4 from 6, QPSK 7/8 from 9, BPSK 1/2 from about 3 (generated signals, 2 samples per symbol). `tests/app/test_isdbs_mode.cpp` checks the wiring in the sources. There is no engine-level test (the engine path was compile-checked only).

## Known limits

* Acquisition at Es/N0 under about 4 dB with a carrier offset beyond 70 kHz relies on the grid search, which runs only every fourth failed window.
* One stream is decoded at a time (selectable); all slots of that stream are decoded.
* No EPG / ISDB service information is parsed beyond what the shared MPEG-TS layer already understands; ISDB-specific tables (SDT variants with Japanese text, EIT of ARIB) are not decoded.
* No IQ recording of a real BS transponder was available, so nothing was checked against one.
