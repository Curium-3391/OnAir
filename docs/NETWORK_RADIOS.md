# Network radios: where they are saved, and recovering from a hang

Radios added by address (SoapyRemote, rtl_tcp, SpyServer, KiwiSDR, Pluto, ...) are saved in the settings file, in the line `netRadios=...`
(entries separated by `;`). The file is `settings.conf` in:

- macOS and Linux: `$XDG_CONFIG_HOME/onair/` or `~/.config/onair/`
- Windows: `%APPDATA%\OnAir\`

If OnAir hangs at start-up because of a saved network radio:

1. Start it once with `--safe-start`: no radio is chosen or opened by itself.
2. Close OnAir, open `settings.conf` in a text editor, delete the entry you do not want from the `netRadios=` line (or the whole line), save.

Looking up a SoapyRemote radio runs in the background (it gives up after a few seconds); press the rescan button to see its radios once the answer is in.

## TCI (network), receive only

TCI is the Transceiver Control Interface of Expert Electronics' ExpertSDR3 (SunSDR radios); other programs offer a TCI server too. Add it in
**Network radios** with type **TCI (network)**: the address of the computer running the TCI server and port 50001 (the default), after turning the
server's TCI option on. OnAir connects with a WebSocket (`ws://host:50001`), reads the server's start-up commands (device, protocol, `vfo_limits`,
`trx_count`, ... `ready`), then sets the IQ sample rate (48, 96, 192 or 384 kHz: the smallest at or above what the mode needs), puts the receiver's
DDS (panorama centre) and VFO on the tuned frequency and starts the IQ stream (`iq_start`) of one receiver (the **Receiver** setting appears when the
server has more than one). Float32, 24-bit and 16-bit samples are all understood. The frequency range comes from `vfo_limits`. TCI has no RF gain for
the receiver, so the gain slider does nothing here; the server's AGC stays as it is.

- OnAir never transmits through TCI: it sends only `iq_samplerate`, `dds`, `vfo`, `iq_start` and `iq_stop`, and no audio or CW command.
- When the connection drops, OnAir reconnects by itself and counts the samples lost.
- Tested only against a fake server that follows the TCI 1.9 specification. Unverified without a real ExpertSDR3: the URL path, whether the server
  accepts the VFO being moved with the DDS, and the exact meaning of `length` in the block header (the payload size is used).

## rtl_tcp sample rates and what limits them

OnAir asks an rtl_tcp-style server for the rate a mode needs. A real RTL-SDR (greeting `RTL0`, a tuner type 1 to 6) is held at 2.56 Msps because it loses samples above that. A server that greets with anything else (rsp_tcp for SDRplay, airspy_tcp, hackrf_tcp, SDR++'s server) is taken as another radio and gets any rate from 0.25 to 20 Msps. If a mode needs more than 2.56 Msps from an `RTL0` server in Auto, the rate is let through up to 20 Msps.

To lift every limit by hand, open the radio's settings and tick **Allow any sample rate (advanced)** (0.1 to 40 Msps). The **Sample rate** field below it overrides what the mode asks (0 = as the mode needs). Both are saved per server.

The status line shows the measured stream: Mbit/s, the rate that really arrived next to the rate asked, and the bytes lost. If the rate that arrives is below what was asked, the server or the network is the limit, not OnAir.

Plain-words ceilings: rtl_tcp sends 8-bit IQ, 2 bytes per sample, so every 1 Msps is 16 Mbit/s.

| Rate | Needs | Works over |
|---|---|---|
| 2.4 Msps | 38 Mbit/s | any Wi-Fi |
| 3.2 Msps | 51 Mbit/s | good Wi-Fi, Ethernet |
| 8 Msps | 128 Mbit/s | 5 GHz Wi-Fi close to the router, Ethernet |
| 10 Msps | 160 Mbit/s | gigabit Ethernet; not most Wi-Fi |

OnAir reads with a 16 MB socket buffer (less if the system refuses) in its own thread, so a busy receiver does not stall the stream.
