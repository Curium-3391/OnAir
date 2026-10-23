# Which radios can transmit in OnAir

Transmit is only possible through the Arm + licence confirmation + band-plan check, whatever the radio. This table says which radios have a transmit path at all.

| Radio | Transmit in OnAir | How | Should it? |
|---|---|---|---|
| HackRF One / Pro | yes | OnAir's own USB driver (libhackrf fallback has none) | yes |
| PlutoSDR (native, libiio) | yes, also full duplex | AD9361 TX attenuation, 1 output (2 on 2T images) | yes |
| USRP B200/B210/B200mini/B205mini (native, UHD) | yes, since 3.0 | UHD tx_streamer, fc32/sc16, TX/RX connector, B210 outputs RF A and RF B; needs UHD 3.15 or newer with the C API TX functions. The several RX entries of one USRP appear as ONE radio in the TX list. Not full duplex: a USB USRP opens once, so the receiver must stop first | yes |
| LimeSDR (native) | no | native driver is receive-only; transmits through its SoapySDR module (SoapyLMS7) | later: native TX |
| bladeRF (native) | no | same: use SoapyBladeRF | later: native TX |
| SoapySDR radios with a TX channel | yes | Soapy TX stream | yes |
| SDRplay RSP* | no | the hardware is receive-only | no |
| Airspy R2/Mini, Airspy HF+ | no | the hardware is receive-only | no |
| RTL-SDR (also rtl_tcp) | no | "RTL-SDR dongles can only receive" | no |
| KiwiSDR, SpyServer, RX888, Fobos | no | receive-only | no |
| Hamlib rigs | CAT only | handled by the Hamlib sink | yes |

The TX window shows a radio that cannot transmit as a short line ("2 receive-only radios hidden", with a show toggle); the reason names the radio once, e.g. "USRP B210 000000001 cannot transmit in this build: the installed UHD has no transmit functions".

Not tested on real hardware: the USRP TX path was written against UHD's C API and tested with fakes only (open without UHD, classification, texts). Test with a dummy load and the Arm gate.
