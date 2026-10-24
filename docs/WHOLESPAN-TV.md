# Whole span for TV (DVB-T, DVB-T2, DVB-S, DVB-S2)

One switch, **Whole span**, at the top right of the spectrum, decodes every occupied signal of a wide capture at once. One receiver runs per signal;
the one you click in the table plays and shows its details, the others keep decoding and show their lock, mode and number of services. It is the same
name as the Whole span of the pager, EDACS, LTR and Fusion modes. It is saved in the settings as before (`mtvOn`, once called "Multi-channel").

## Where it is

| Standard | Switch | Needs |
|---|---|---|
| DVB-T2, DVB-T | always shown | a capture of at least two channels of the chosen width (14 MHz for two 7 MHz channels, 16 MHz for two 8 MHz). Narrower: the switch looks off and the tooltip says "capture at least N MHz for two M MHz channels". |
| DVB-S, DVB-S2 | always shown | a capture of 3 Msps or more. A transponder can be anywhere in the span and have any width. |

Right click on the switch: **Receivers at once** (default 3 on a computer with 8 cores or more, 2 on a smaller one) and, for DVB-T/T2, where the
channel raster sits. When more signals are found than receivers are allowed, the strongest are decoded first.

## DVB-S / S2: how the transponders are found

`core/src/engine/transponders.cpp`, a pure function on the averaged spectrum (`detectTransponders`):

1. The spectrum (4096 bins, an average of a few dozen frames) is smoothed over a sixteenth of the narrowest carrier looked for (1 Msym/s, or the
   receiver's own minimum for the capture rate).
2. The noise floor is a line fitted through the quiet parts of the span, so the tilt of an LNB's IF (5-10 dB) does not hide a weak transponder or
   invent a strong one.
3. The 0 Hz spike of the SDR (a few bins wide) is replaced by the level around it, so it is not a carrier and a transponder sitting on DC keeps its
   flat top.
4. Every run above the floor plus 5 dB is a candidate. A valley of 3 dB or more inside a run cuts it into two carriers (transponders a few MHz apart,
   or edge to edge with overlapping shoulders). Each part must be wide enough and have a flat top (most of its middle half within 3 dB of the plateau)
   on its own, so a tone, a spur or two narrow spurs close together are never merged into one transponder.
5. For each carrier: the centre and the -3 dB width (= the symbol rate of a root raised cosine carrier, within about 5 %), the roll-off from the
   shoulders (snapped to 0.35 / 0.25 / 0.20 / 0.15 / 0.10 / 0.05), the occupied bandwidth Rs (1 + roll-off), the level and the SNR over the floor.
   A carrier that runs into the edge of the usable span (46 % of the rate each side) is not started: half a transponder cannot be decoded.

`MultiTv` (sat mode) follows the candidates over time: a transponder is "present" after two looks in a row (a look is a quarter second), is kept
while its receiver is locked, and is dropped after eight looks without a signal. Each present transponder gets a `ChannelFilter` (mix to 0 Hz,
resample to 3.2 samples per symbol, 2 Msps at least) and its own `DvbsReceiver`, which finds the exact symbol rate, roll-off, modulation and code
rate itself (the "auto symbol rate" of the single-carrier mode). The table shows frequency, symbol rate, state, mode (DVB-S2 QPSK 2/3 ...), number of
services, MER and the receiver's load.

## CPU budget

A DVB-S2 receiver at 5 Msym/s is mostly the LDPC decoder, a few tens of percent of one core on a recent computer; a DVB-S one is lighter. Each
receiver is a thread of its own, fed from a queue of 0.2 s of signal; when a receiver falls behind on a live radio it loses samples (the Load
column and `dropped` show it) instead of slowing the others. The channel filter of each receiver also costs a pass over the whole capture rate, so a
60 Msps capture with three receivers is three such passes. The default cap keeps a laptop responsive; raise it with Receivers at once when the
Load column stays low.

## Limits

* All receivers share one capture: the transponders must be inside the span (roughly 90 % of the sample rate; the converter's roll-off eats the
  edges) and the LNB's polarisation and band choice apply to all of them.
* A 10 Msps HackRF capture holds one 5 Msym/s transponder (about 6.7 MHz wide): Whole span then shows one row. Use 20 Msps for two or three.
* The recorder, the guide and the network tuner follow the selected transponder.

## For other standards

`wholeSpanTvState()` in `app/gfx/plots.cpp` is the one helper the switch is drawn from: add a `case` with the family, the engine standard code and
whether the channels sit on a raster; then add the standard code to `Engine::multiChannelSupported` and a receiver to `MultiTv`. DVB-C, ISDB-T,
ATSC and DTMB can join this way.

## Tests

`tests/engine/test_transponders.cpp`: three carriers of different rates (one far off centre), a weak carrier on a tilted floor beside a strong one,
adjacent transponders with a guard, edge to edge and with a 12 dB step, the DC spike alone and under a carrier, two close false peaks, a tone, noise
only and the lower rate limit.
