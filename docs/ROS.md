# ROS: what is publicly known, and what OnAir does with it

ROS is a weak-signal HF chat mode written by José Alberto Nieto Ros, EA5HVK. The program is proprietary and its protocol has never been
published. This page lists what public sources say about the signal, what they leave out, and what OnAir builds from that.

The rules for this work: we used only public web pages. We did not download, run, decompile or reverse engineer the ROS program, and we did
not copy any code. Everything below comes from the pages cited.

## Sources

| # | Source | What it gives |
|---|--------|---------------|
| S1 | [Signal Identification Wiki: ROS](https://www.sigidwiki.com/wiki/ROS) | USB, 1.8 to 30 MHz, 2 kHz bandwidth, ACF 4 ms. Describes the signal as FHSS + DSSS + "2G CDMA" and a "144 MFSK" phase-continuous single-tone modem. Sample modes ROS 4, 8 and 16 Bd (RX4, RX8, RX16). The latest version has only the ROS-2K 4/8/16 modes. Lists other variants (see below), v7.4.0 coding change (9 November 2016, not compatible with older versions) and the v7.4.0 frequency list. |
| S2 | [radioscanner.ru signal base, ROS (signal 236)](http://signals.radioscanner.ru/base/signal236/) | Measured: about 15.5 Bd, or 1 Bd for the very slow mode; up to 144 carriers about 15.5 Hz apart; 2250 to 2300 Hz bandwidth; lower edge near 400 Hz audio; a start tone near 1481 Hz for about 4.2 s; ACF about 18662 ms; SSB. |
| S3 | [amateurradio.com, "Yet Another Digimode"](https://amateurradio.com/yet-another-digimode) | About 2.2 kHz wide, a choice of 16 Bd or 1 Bd. The author claims the 1 Bd mode works 35 dB below the noise (not measured by the article). |
| S4 | [IK1AMC, "ROS Mode" (calvibit.net)](http://www.calvibit.net/ROS.html) | 16, 8 and 4 Bd in a 2.2 kHz passband; all three received at once; FHSS in v7.2.7, removed in v7.2.8; claims of -35 dB S/N links. |
| S5 | [amateurradio.com, "ROS digital mode now legal in USA"](https://www.amateurradio.com/ros-digital-mode-now-legal-in-usa/) | Reports that after the author sent a technical specification to the FCC, ROS was treated as an RTTY/data emission (97.309), not spread spectrum. The specification itself is not public. |
| S6 | [ROS blog (rosmodem.wordpress.com)](https://rosmodem.wordpress.com/) | The author's site. Announces a new error-correction code from v7.4.0, gives no parameters, and links several third-party PDFs (not read, see below). |
| S7 | [14GIM035 "Le mode ROS"](http://14gim035.xp3.biz/rosmode.htm) | A CB activity log: 27.245, 27.635 and 27.935 MHz USB, decodes tagged CRC-OK / BAD-CRC, reported S/N from -19 to +15 dB. |

Third-party PDFs linked from S1 and S6 (a user guide, a F.A.Q., analyses in French and Spanish) were not read, because the rules did not
allow file downloads. They may hold more detail, and a later pass could read them as web pages if they are published that way.

## What is known (and how sure)

| Parameter | Value | Sources | Confidence |
|-----------|-------|---------|------------|
| Sideband | USB | S1, S2, S7 | stated by all |
| Occupied bandwidth (HF) | about 2.2 to 2.3 kHz (S1 rounds to 2 kHz) | S1, S2, S3, S4 | measured (S2) and stated |
| Modulation | MFSK, one tone at a time, phase-continuous | S1 ("144 MFSK ... single tone"), S2 (MFSK-16 to MFSK-144 family) | stated; the S1 text is partly garbled |
| Number of tones | up to 144 | S1, S2 | stated |
| Tone spacing | about 15.5 Hz | S2 (measured) | measured once. 144 x 15.625 Hz = 2250 Hz matches the measured width, but 15.625 Hz is our inference, not a published value |
| Symbol rates (current) | 4, 8, 16 Bd (ROS-2K) | S1, S4 | stated |
| Symbol rate (older) | 1 Bd | S2, S3 | stated, older versions |
| Measured rate | about 15.5 Bd | S2 | measured |
| Start tone | about 1481 Hz for about 4.2 s | S2 | measured once, version unknown |
| ACF | 4 ms (S1), about 18662 ms (S2) | S1, S2 | the two disagree; probably different versions or a different structure |
| Spreading / hopping | "FHSS + DSSS + 2G CDMA" (S1); FHSS only in v7.2.7 (S4); not spread spectrum per the FCC report (S5) | S1, S4, S5 | contradictory |
| Error detection | a CRC exists (decodes are tagged CRC-OK / BAD-CRC) | S1, S7 | observed in decoder output; polynomial and length unknown |
| Error correction | a new code from v7.4.0, not compatible with older versions | S1, S6 | announced; the code itself is not described |
| Other variants | narrow HF 2000 Hz and 500 Hz; MF1 / MF2 / MF7 at 100 Hz (UK 500 kHz trials); 50 Hz EME mode on 144 MHz with a 1500 Hz AFC range; "Wide-Hop" baseband FHSS concept mode; repeater, beacon and SMS features | S1 | names and widths only |
| Frequencies (v7.4.0) | 1.840, 3.558, 5.367, 7.046, 10.130, 14.103, 18.108, 21.122, 24.192, 27.635 (CB), 28.140, 50.245, 144.980 MHz | S1 | whether dial or centre is not stated |
| Frequencies (CB activity) | 27.245, 27.635, 27.935 MHz USB | S7 | activity log |

## What is missing

None of the public pages describe any of the following, and a decoder needs all of them:

- the mapping from data to tones: the alphabet or varicode, how many bits a symbol carries, any Gray coding
- the frame: preamble or sync sequence (apart from the measured start tone), header, length, end of message
- the FEC: code type, rate, constraint length or block size, puncturing, and the post-v7.4.0 code
- the interleaver
- the spreading or hopping sequence, if there is one (the sources disagree on whether there is)
- the CRC polynomial and length
- the exact tone spacing and audio placement of each sub-mode, and whether 4, 8 and 16 Bd share one tone grid
- anything at all about the MF, EME, 500 Hz and Wide-Hop variants beyond their names and widths

**So ROS cannot be decoded from public information.** OnAir does not decode it, does not read any text from it, and does not pretend to.
Getting those details would mean reverse engineering the program, and this project does not do that.

## What OnAir does

OnAir identifies ROS HF from the publicly reported physical layer.

- **Identifier** (`core/include/dect2/ros/ros_detect.h`, `core/src/ros/ros_detect.cpp`). It moves the signal to 0 Hz, filters it to about
  6 kHz and takes a short-time spectrum (40 ms Hann window, 10 ms hop). Frames that hold one clear tone give that tone's frequency. Runs of
  such frames make tone segments, about one per symbol. From the segments it measures:
  - the tone grid: the largest spacing on which all the tones sit, found by the vector strength of f / spacing
  - the span and how many grid positions are used
  - the symbol rate: the tone changes fall on a time grid of 1 / rate
  - the single-tone purity: the share of the power above the noise that sits in the strongest tone

  It calls a signal ROS when all of these hold: the grid is 12 to 20 Hz and clearly present, the span fits about 2.2 kHz for the number of
  symbols seen, there is one tone at a time, and enough different tones are used. The rate then picks the sub-mode: 16, 8 or 4 Bd within
  18 %. Any other rate gives "ROS-like", marked as weak. A capture with fewer than 14 symbols is also marked weak, which is normal for 4 Bd in
  the identifier's 2 s capture.
- **Signal Identifier**. There is one "ROS" row in `core/src/engine/sig_known.cpp`. `sig_ident.cpp` runs the ROS check on any signal
  1.2 to 3.6 kHz wide, and the row is proposed only when that check finds the grid, because the width alone fits too many modes. It opens in
  the General receiver (USB) so you can listen. The candidate line names the sub-mode, for example "ROS 16 baud".
- **Frequency database and spectrum labels**. The v7.4.0 frequencies from S1 are in `data/frequencies/hf.tsv` under the General receiver,
  marked "identified only (not decoded)". The spectrum's frequency-database labels show them.
- **Test signals**. `ros::synthReportedGrid()` draws random tones out of 144 on a 15.625 Hz grid at the chosen rate. It is **not** ROS
  encoding. It is only a signal with the reported physical parameters, used to check the identifier.
- **Tests** (`tests/ros/test_ros_detect.cpp`):
  - 4, 8 and 16 Bd at +10, 0 and -6 dB SNR in 2.5 kHz, with up to ±80 Hz of offset: at least 7 of 8 correct
  - complex samples at 48 kHz with a strong neighbour
  - a rate that is not a reported one (ROS-like, weak)
  - noise: no false alarms in 40 runs
  - other modes that must not be called ROS: Olivia 32/1000, 32/2000 and 8/250, MFSK16, MFSK8, JT65-like, FT8-like, a swept carrier
  - the Signal Identifier: ROS ranked first on the test signal and never proposed on noise

### Limits

- The thresholds come from the public figures and our own test signals, not from real ROS recordings. A real recording, for example one
  attached to the sigidwiki page, should be checked when one can be obtained legitimately.
- A fast drift (more than about 1 Hz over the capture) blurs the 15.6 Hz grid. SSB mistuning (a fixed offset) does not matter.
- The 1 Bd, MF, EME, 500 Hz and Wide-Hop variants are not identified. Their tone structure is not public.
- If the 4 and 8 Bd modes turn out to use another tone spacing, the identifier still accepts any grid from 12 to 20 Hz, but the sub-mode name
  rests on the rate alone.
