# OnAir 3.2

3.2 follows 3.1. It fixes a long list of reports from users, adds a few features that were asked for, and changes the default look for new installs.

**Measured** numbers come from OnAir's own test signals, not from over-the-air recordings, unless a line says so.

## Things to know first

- **Dev A is the default look for new installs.** People who already use Classic keep it; they see a one-time notice that says Dev A exists and where to switch back. Small screens start in the touch interface (#139).
- **Updates: "check yes, download no" for new installs.** OnAir looks for a new version by default and tells you; it downloads one only when you turn that on in Settings. Existing settings are not changed.
- **Transmit is no longer blocked by OnAir.** The refusal lists, the licence and band checks and the remote-API refusal are gone; transmit warns instead. You are responsible for what you send. The one rule left: nothing is uploaded to the map server while you transmit or for 10 minutes after. In OnAir Web, transmit by listeners is a host setting ("web_allow_tx"), off by default. 11 m (CB) can transmit with a warning.
- **A new first-run wizard** (4 screens, with the FM step after the region).
- **Bandwidth controls** for VDL2, TETRA, TETRAPOL and the PSK analyser show the mode's standard bandwidth by default; tick Custom to change it. Anyone who had saved a width keeps it.
- **One clock for all modes (#218):** the computer clock or the signal's own time, chosen once.

## Fixes

- **DTMB / CDR (#275):** the receiver goes back to the middle channel when the strongest neighbour brings no lock.
- **SDRplay (#319):** Stop Receiver can no longer freeze the window, and quitting does not abort.
- **rtl_tcp (#201):** waits for a silent local driver instead of closing the connection.
- **HaLow (#192):** the scan reaches the outer channels of an 8 MHz BSS.
- **DRM (#299):** the text message is cleaned before it is shown.
- **ISDB-T (#313):** the one-seg service plays when the PAT is missing.
- **Network radios (#314):** one shared list of added network radios for all receivers.
- **SpyServer (#321):** the tuning range includes the server's IQ centre range.
- **Pluto (#316):** a scanned network Pluto uses the native driver.
- **USRP (#294):** receive on RX2 and transmit on TX/RX at the same time.
- **KiwiSDR (#257):** waits for the rest of a frame after a receive timeout.
- **TCI (#248):** starts a stopped ExpertSDR3 radio; clearer errors and docs.
- **Interface:** the big readout digits work in Radio-centre mode and the spectrum / waterfall divider moves both ways (#304); the bottom status cells show Chinese captions (#297); moving Min / Max stops the automatic dB range (#297); "Click centres the spectrum" works in the General Receiver (#301); the View menu had two "Reset layout" items with one ID (#301); the Voice codecs overview no longer asks to accept an accepted notice (#293).
- **TX:** the TX settings stay readable on the Light skin while transmitting (#320); the APRS / packet Destination field no longer shares an ID with the Destination combo (#302).
- **ADS-B (#235):** dragging the map no longer deselects the aircraft.
- **DMR (#300):** UTF-16LE text sent under the big-endian label is read correctly.
- **VDL2:** the "Advanced: receive bandwidth" control wraps with the rest of the toolbar instead of being cut off at the right edge.

## New

- **Receive bandwidth by hand for TETRA, TETRAPOL and VDL2 (#274):** up to 2 MHz, Auto by default.
- **VDL2 session log (#309):** every frame with UTC date and time, exported as CSV, JSON or text.
- **VDES (#303):** positions from AIS-like ASM frames on the map and as NMEA over UDP.
- **DVB-S (#322):** a single-service signal plays by itself and follows retunes.
- **PSK (#308):** channel bandwidth by hand, 2.5 to 200 kHz.
- **NFC (#271):** decodes real 2 Msps captures of a phone reading a card.
- **TV (#318):** picture shape box: stretch 4:3 to 16:9, zoom, fill.
- **LimeSDR (#169):** a hint in the radio list when a Mini is plugged in but LimeSuite is missing.
- **Documentation:** FDM-DUO through its USB sound card (#251); analogue TV status (#73); RTL2832P DVB sticks (#180).
- **First-run setup, version 2.** Four short screens, each skippable: the radio (found automatically, with a help link per radio, or the test signal), the location (city search, locator or coordinates; sets the country, ITU region, band plan, FM step, DAB band III, TV channels, the ADS-B position and the time zone), the interests (which modes are starred first, the starting mode and starter presets) and a summary with the tour on offer. It shows only when there are no earlier settings, so an update never shows it to existing users. Run it again from Help > Setup and lessons > Run setup again.

- **Fast scan v2:** Whole span, two-sweep confirmation and spur / image flags; a HackRF firmware sweep with a retune fallback; a `dect2fastscan` test tool.
- **New modes:** ROS (detection only, partial), SSDV pictures (uses the GPL-3 fsphil/ssdv code, credited in THIRD_PARTY.md) and Eurobalise / EBICab at 4.234 MHz.
- **Icom IC-R8600:** a native driver with firmware import (based on work by Topol in collaboration with AI).
- **Ham:** an 11 m RX button, a ham voice band plan, command line `--list-devices`, `--device` and `--antenna`, and Inmarsat-C EGC area, subject and save.
- **Hosting (OnAir Web):** TLS (OpenSSL) in the macOS, Linux and Windows packages, the certificate store found automatically, `web/onair-web` included in every package, the origin fix behind a proxy, docs/HOST-A-RECEIVER.md.
- **Decoder quality:** the DMR coarse-carrier search no longer follows a strong adjacent channel; P25 and NXDN capture frequencies beyond the channel filter; the AGC no longer ratchets down on repeated bursts.
- **Windows fixes:** the AVX2 Viterbi stack alignment on MinGW (#324), UTF-8 wide paths for the DRM USAC codec search (#329); also RTL-SDRs that share a serial number (#328) and analog TV / FPV full screen (#326).
- **Credit:** Icom IC-R8600 work by Topol, in collaboration with AI.

## Pull requests merged, with thanks

- #327 HFDL: the bits of each ICAO octet are reversed, so aircraft addresses match dumphfdl (frank-1225).
- #295 P25 trunking follows Phase 2 calls from the Phase 1 control channel, and the exact CIC decimator and 8 to 48 kHz upsampler for the voice receivers (Mike, drelectro).
- #315 Chat mode for the Enhanced Sub-GHz Chat app on the Flipper Zero (A2ureIm4la).
- #305, #306, #307 DMR: position history kept across a reset, a bounded and followable map card, NMEA positions in text messages (Vladimir, vlig-alt).
- #311 TETRA: the GSSI of a group call sent to an SSI with a usage marker (Dmitry Obedkov, drfruct).
- #298 Band plan: Portuguese (Portugal) UI picks the CEPT plan, not Brazil's (ffishman).

## Known issues

- #254 could not be reproduced; it stays open.
- `dvbt_real` (a real 7 MHz DVB-T recording) can still differ in the number of damaged packets on x86 CPUs, because the floating-point order differs from arm64; decoding is not broken.
- Decoder quality: P25 with a +40 dB adjacent channel went from 75 % to 67 % decoded, and DMR next to a +30 dB adjacent channel still does not decode.
- **Needs a user test (not tried on real hardware or signals):** the HackRF firmware sweep, the IC-R8600 driver, ROS and EBICab, Tailscale / Cloudflare hosting of OnAir Web, and the Windows fixes #324 and #329.
- Tests that fail in the project's CI on main as well as in 3.2 (not new): `dvbt_real`, `halow_engine`, `ham_js8` (Linux), `fastscan` (macOS), `native_netradio`, `scan_soapy`, `tx_quality_*` and `wifi_engine` (timing), `time_ntp`, and on the Windows (MinGW) build about 20 tests such as `isdbs`, `presets`, `latest_slot`, `backup`, `native_tci`, `mesh_proto_rs`, `voice_keys`, `dvbs_vlsnr`, `iridium_bits` and `hfdig_kbd`. Slow runners also make `native_kiwisdr`, `updater`, `iono_engine` and `plugin` fail by timing now and then. Three tests that failed on the release candidate (`credits`, `gui_rules`, `hfdl_proto_rs`) and `icom_r8600` (it crashed in the USB library on the CI runners) were fixed before release; the fix commits are test-only, and they were not run on GitHub again before this release.
