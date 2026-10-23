# Network radios: where they are saved, and recovering from a hang

Radios added by address (SoapyRemote, rtl_tcp, SpyServer, KiwiSDR, Pluto, ...) are saved in the settings file, in the line `netRadios=...`
(entries separated by `;`). The file is `settings.conf` in:

- macOS and Linux: `$XDG_CONFIG_HOME/onair/` or `~/.config/onair/`
- Windows: `%APPDATA%\OnAir\`

If OnAir hangs at start-up because of a saved network radio:

1. Start it once with `--safe-start`: no radio is chosen or opened by itself.
2. Close OnAir, open `settings.conf` in a text editor, delete the entry you do not want from the `netRadios=` line (or the whole line), save.

Looking up a SoapyRemote radio runs in the background (it gives up after a few seconds); press the rescan button to see its radios once the answer is in.

## SoapyRemote (a radio on another computer)

1. On the computer with the radio: install SoapySDR, the radio's SoapySDR module and SoapyRemote, then run
   `SoapySDRServer --bind="0.0.0.0:55132"`. Check it there first: `SoapySDRUtil --find` must list the radio.
2. Open TCP port 55132 in that computer's firewall (and UDP, see 4).
3. In OnAir: **Add network radio**, type **SoapyRemote**, the server's address and port 55132. Leave **Remote driver** empty to list what the
   server offers, or name one (`sdrplay`, `miri`, `rtlsdr`, ... or `driver=sdrplay,serial=...`). A server that also has SoapyAudio lists the sound
   card first: pick the radio, not "audio".
4. The samples travel over UDP by default. On a LAN that is the fastest; through the internet, a VPN or a NAT router UDP often does not get
   through: add `prot=tcp` to the Remote driver field (`driver=sdrplay,prot=tcp`; OnAir passes it as SoapyRemote's `remote:prot`).
5. Test from the OnAir computer with `SoapySDRUtil --probe="driver=remote,remote=tcp://SERVER:55132"`. If that fails too, the problem is between
   the two computers (firewall, versions), not in OnAir. Client and server should run the same SoapyRemote version.

## Network discovery is off by default

When OnAir builds the radio list it looks only for radios on USB (and PCIe), plus the network radios you added by address. It does not search
the network by itself: SoapyRemote's server search, SoapyPlutoSDR's mDNS / `pluto.local` lookup, UHD's broadcast for network USRPs, librfnm's
network search and the other SoapySDR modules that look on the LAN are not run (issue #215: a Pluto on somebody's network must not be contacted
just because OnAir started). To have them back, open the Radio list and tick **Search the network for radios**. Adding a radio by address
never needs it.

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
