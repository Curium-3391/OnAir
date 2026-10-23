# OnAir Web: share your radio with many listeners

OnAir Web turns `onair-server` (the headless server, docs/headless-server.md) into a WebSDR / KiwiSDR-style receiver for many people at once. Your SDR captures a wide piece of spectrum; everyone who opens your page gets their **own virtual receiver** inside that capture: their own frequency, mode (AM, NFM, WFM, USB, LSB, CW), passband, squelch, AGC and audio, their own zoomable waterfall, and the results of decoders they subscribe to. One radio, many listeners, nobody tunes anybody else away.

Status: the server core is built and tested with synthetic signals and fake clients (loopback sockets, many simulated sessions). **Not tried with a real radio, real browsers over the internet, a Raspberry Pi, or a real certificate.** The browser interface is in `web/onair-web/` (vanilla JS, no build step, no internet needed; not yet tried in real browsers against a live server); without it the server shows a placeholder page, and any client that speaks docs/ONAIR-WEB-PROTOCOL.md works.

## How it works

```
radio --(raw samples, the radio's rate)--> onair-server
            |-- the main receiver (the mode chosen in the control page, as before)
            '-- OnAir Web hub
                  shared once:  overlap-save FFT of the capture, waterfall
                  per listener: the FFT bins of its channel -> small inverse FFT -> exact resampler -> passband filter -> demodulator -> squelch / AGC -> ADPCM or PCM
                  per decoder:  the same slice at the decoder's rate; listeners who ask for the same decoder on the same frequency share one instance
```

Because every listener only cuts its own few bins out of the shared transform, the cost of a listener does not grow with the radio's sample rate (see the sizing table). The control page and the remote-control API (port 8765) are unchanged and stay private: web listeners never control the radio itself (its centre frequency, rate, gain or mode), only their own virtual receiver.

## Quick start (your own network)

```
onair-server --web --web-lan --device test      # the built-in test signal; drop --device test for your radio
```

or in the settings file (`~/.config/onair/server.conf`, see docs/headless-server.md):

```
web=1
web_lan=1
autostart=1
```

Then open `http://<address of the server>:8073/` on any computer or phone of your network. Without TLS only private addresses (10.x, 172.16-31.x, 192.168.x, 169.254.x, 100.64-127.x, this computer) are let in; anybody else gets "this server needs HTTPS".

The radio's centre frequency and rate are what the listeners can tune inside: set them in the control page (port 8765) or with `freq_mhz=` / `rate_msps=`. A wider rate gives listeners more spectrum but costs the shared FFT more processor time.

## On the internet

1. **TLS.** Listeners' passwords and invites must not travel in clear text, and browsers only allow some audio features on HTTPS. Either:
   * a certificate in OnAir itself (needs a build with `-DDECT2_WITH_TLS=ON`):
     ```
     web_tls_cert=/etc/letsencrypt/live/radio.example.org/fullchain.pem
     web_tls_key=/etc/letsencrypt/live/radio.example.org/privkey.pem
     ```
     The files are read once at start: restart the server after a renewal (for certbot: `--deploy-hook "systemctl restart onair-server"`). The key file must be readable by the server's user (for example a copy with mode 0640 owned by `onair`);
   * or a reverse proxy (Caddy, nginx) that does the TLS and forwards to `127.0.0.1:8073`. Set `web_trust_proxy=1` so the listener's address comes from `X-Forwarded-For` (only believed from a proxy on this computer) and the per-address limits keep working. Caddy example:
     ```
     radio.example.org {
         reverse_proxy 127.0.0.1:8073
     }
     ```
     Caddy gets and renews the Let's Encrypt certificate itself and passes WebSockets through. For nginx add `proxy_http_version 1.1; proxy_set_header Upgrade $http_upgrade; proxy_set_header Connection "upgrade"; proxy_set_header Host $host; proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for; proxy_read_timeout 120s;`.
   * Let's Encrypt needs a domain name that points at your public address (a dynamic DNS name works) and port 80 or 443 reachable for the challenge. A self-signed certificate works for testing, but every listener gets the browser's warning.
2. **Router.** Forward one TCP port (443 with a proxy, or 8073) from the router to the server. Forward nothing else: the control page (8765) and the rtl_tcp share must stay inside your network.
3. **Firewall.** Allow that port in. On Linux with ufw: `ufw allow 443/tcp` (proxy) or `ufw allow 8073/tcp`. Under CGNAT (many mobile and some fibre connections) you cannot forward a port: use a tunnel (Cloudflare Tunnel, Tailscale Funnel, a small VPS with a reverse SSH tunnel) to the proxy.
4. **Access.** Decide who may listen (`web_auth`, below) and how many (`web_max_users`). Start with a password or invites; open access to the whole internet means anyone, and they share your upload bandwidth.

`web_plain_internet=1` lets plain HTTP in from every address. Do not use it except behind something else that does the encryption.

## Access and limits

| Key | Default | Meaning |
| --- | --- | --- |
| `web` | 0 | 1: OnAir Web on (also `--web`) |
| `web_port` | 8073 | its port (also `--web-port`) |
| `web_bind` / `web_lan` | 127.0.0.1 | the address to listen on; `web_lan=1` is `0.0.0.0` (also `--web-lan`) |
| `web_tls_cert`, `web_tls_key` | `tls_cert`, `tls_key` | PEM files for HTTPS / WSS |
| `web_trust_proxy` | 0 | a reverse proxy on this computer: take the address from `X-Forwarded-For` |
| `web_plain_internet` | 0 | allow plain HTTP from non-private addresses (not recommended) |
| `web_origin` | | an extra page origin allowed to open the WebSocket (repeatable), e.g. `https://radio.example.org` behind a proxy on another name |
| `web_auth` | open | `open` (anyone), `password` (`web_password`, 8 characters or more), `invite` (invite tokens only) |
| `web_invite` | | `token,label,max_minutes` (repeatable). Valid in every mode; the admin API makes and revokes them |
| `web_admin_token` | made at first start | the admin API's token (Bearer); 32 hex characters |
| `web_admin_lan` | 0 | the admin API from other computers too (otherwise from this computer only) |
| `web_max_users` | 8 | listeners at once |
| `web_max_per_ip` | 2 | listeners from one address |
| `web_cpu_cap_pct` | 75 | all web processing together, % of the whole machine: above it new listeners get "busy"; well above it for 10 s the newest open-access listener is disconnected |
| `web_session_cpu_pct` | 25 | one listener, % of one core: over it the listener gets fewer and smaller spectrum rows, still over after 10 s it is disconnected (`cpu_budget`) |
| `web_max_kbps` | 8000 | all listeners together, kbit/s (new listeners get "busy" above 90 %) |
| `web_user_kbps` | 512 | one listener, kbit/s (audio settings over 80 % of it are refused; spectrum is slowed down above it) |
| `web_max_decoders` | 8 | shared decoder instances |
| `web_idle_min` | 30 | no tuning or other action for this long: disconnected (0: never) |
| `web_public_max_min`, `web_member_max_min` | 0 | session time limit for open access / for password and invite users (0: none; an invite's own limit wins) |
| `web_max_fps`, `web_max_bins` | 15, 2048 | spectrum rows a second and bins a row, per listener |
| `web_threads` | 0 | worker threads (0: half the cores, 1 to 4) |
| `web_log_ip` | 0 | 1: listeners' addresses appear in the log and the admin user list. Off: the log says only "session 7 joined" and the admin list shows a short tag that cannot be turned back into the address |
| `web_name`, `web_location`, `web_antenna` | | shown to listeners |
| `web_root` | | the browser UI's folder; empty: `web/onair-web` beside the program or `../share/onair/web/onair-web`; none found: the placeholder page |
| `web_demods` | all six | the demodulators listeners may use, e.g. `am,usb,lsb,cw` for an HF-only server |
| `web_decoders_off` | | data decoders turned off for listeners |
| `web_decoders_on`, `web_legal_ack` | | voice / pager decoders turned on, and the version of the legal notice the host accepted (set by the admin API; see Safety) |
| `web_directory`, `web_directory_url`, `web_public_url` | 0 | the server directory heartbeat (extension point, not in this build) |

Failed logins (wrong password or invite, wrong admin token) from one address: after 5 within a minute that address is refused for a minute. Every wrong answer comes after 300 ms.

## Admin API

Same port, `Authorization: Bearer <web_admin_token>`, from this computer unless `web_admin_lan=1`. POST bodies are JSON (`Content-Type: application/json`, at most 4 KB).

```
T=$(grep ^web_admin_token ~/.config/onair/server.conf | cut -d= -f2)
curl -s -H "Authorization: Bearer $T" http://127.0.0.1:8073/web/v1/admin/users
curl -s -H "Authorization: Bearer $T" http://127.0.0.1:8073/web/v1/admin/stats
curl -s -H "Authorization: Bearer $T" -H 'Content-Type: application/json' -d '{"id":7,"reason":"please share"}' http://127.0.0.1:8073/web/v1/admin/kick
curl -s -H "Authorization: Bearer $T" -H 'Content-Type: application/json' -d '{"label":"Ana","max_minutes":120}' http://127.0.0.1:8073/web/v1/admin/invite
curl -s -H "Authorization: Bearer $T" -H 'Content-Type: application/json' -d '{"prefix":"3fa9c1"}' http://127.0.0.1:8073/web/v1/admin/invite/revoke
curl -s -H "Authorization: Bearer $T" -H 'Content-Type: application/json' -d '{"demod":"wfm","on":false}' http://127.0.0.1:8073/web/v1/admin/policy
```

| Call | |
| --- | --- |
| `GET users` | the sessions: id, name, class, address (or its tag), seconds, idle seconds, frequency, mode, audio / spectrum on, kbit/s, CPU %, degrade level, dropped frames, decoders |
| `GET stats` | users, CPU % (of the machine) and its cap, kbit/s and its cap, admitted / rejected / kicked counts, samples dropped because the DSP fell behind, the capture, the shared decoders and their listener counts |
| `POST kick` | `{"id":n,"reason":"..."}` |
| `GET policy`, `POST policy` | the demodulators and decoders listeners may use; `{"demod":"am","on":true}` or `{"decoder":"adsb","on":false}` |
| `GET legal` | the legal notice text and its version |
| `GET invites`, `POST invite`, `POST invite/revoke` | invites (the list shows only a 6-character prefix of each token; the token is shown once, when it is made); revoke by `token` or `prefix` |

Changes made through the admin API are written to the settings file.

## Safety and legal defaults

These are fixed rules, not settings:

* **Web listeners can never transmit.** The protocol has no transmit, arm, PTT or carrier message (they are answered "forbidden"), and the web code is compiled with `ONAIR_WEB_SESSION`, which turns the transmit headers into a compile error, so no transmit function can be linked into it. `tests/server/test_web_safety.cpp` (no transmit, key or recording member on the web classes, a scan of `server/web` for transmit, engine, key and file-writing code) and the ctest `web_tx_guard` (the compile must fail) prove it.
* **No keys, no decryption.** Web listeners cannot import, see or use keys; encrypted traffic is never decoded; the voice key store is not part of the server.
* **Digital voice (DMR, P25, NXDN, TETRA, dPMR, D-STAR, Yaesu System Fusion, M17 and trunking) and pagers (POCSAG, FLEX) are off for web listeners.** The host can enable each one, and only after accepting the legal notice (`GET /web/v1/admin/legal`; `POST policy` with `"accept_legal_notice":"onair-web-legal-1"`). The acceptance is stored as `web_legal_ack`; a later version of the notice has to be accepted again. While the server's own receiver runs such a mode, its results are not passed to web listeners unless that kind is enabled.
* Voice codecs that are patented (AMBE / IMBE) are only used when the host installed the plug-in (docs/CODECS.md); web audio itself is IMA ADPCM or plain PCM.
* **Nothing is written to the host's disk for a listener.** There is no recording, upload or file message; the web code has no file-writing call (the scan above checks it). The only file it changes is the settings file, through the admin API, on the host's own request.
* **No addresses are logged** unless `web_log_ip=1`. Addresses are kept in memory while a listener is connected, for the per-address limits and the failed-login throttle (bounded table, entries expire after a minute).
* The browser page gets a strict Content-Security-Policy (no outside scripts, no framing), no CORS headers are ever sent, and listeners never see each other's names or frequencies.

### Host responsibility

By running OnAir Web you make what your receiver hears available to other people. You are responsible for that:

* Check the law where your receiver is (and, for an internet server, where your listeners are). Receiving may be allowed while passing on what you received is not: many countries forbid disclosing the content of messages that are not meant for the general public (for example the UK Wireless Telegraphy Act 2006 s.48, the US 47 U.S.C. 605 and ECPA, the EU member states' telecommunications laws). Some bands (police, military, mobile phones, some air traffic in some countries) may not be received at all.
* Listeners act on your connection: their traffic is your upload, their abuse reaches your address. Use a password or invites for anything beyond friends, set `web_max_users` and the bandwidth caps to what your line can carry, and keep the control page private.
* Keep the software and your certificates up to date, and run the server as an unprivileged user (packaging/linux/onair-server.service).

## Hardware sizing

Measured with `onair-web-load` (tools/onair_web_load.cpp: the hub on one thread, synthetic capture with noise and 12 FM carriers, each listener tuned to its own channel with audio on and a 1024-bin waterfall at 10 rows a second) on an Apple M2 Pro, one core = 100 %:

| Capture | Listeners | Mode, audio | Total CPU | Per listener | Data per listener |
| --- | --- | --- | --- | --- | --- |
| 2.4 Msps | shared part only | waterfall | 1.0 % | | |
| 2.4 Msps | 1 / 4 / 16 / 32 | NFM, ADPCM 12 kHz | 1.3 / 2.7 / 8.0 / 15.7 % | 0.2 to 0.46 % | 126 kbit/s |
| 10 Msps | shared part only | waterfall | 4.3 % | | |
| 10 Msps | 8 / 32 | NFM, ADPCM 12 kHz | 8.2 / 24.6 % | 0.5 to 0.6 % | 140 kbit/s |
| 2.4 Msps | 1 / 8 | WFM, ADPCM 24 kHz | 1.9 / 9.3 % | 0.9 to 1.0 % | 172 kbit/s |
| 2.4 Msps | 8 / 32 | USB, ADPCM 12 kHz, no waterfall | 9.4 / 36.6 % | 1.1 % | 54 kbit/s |

What that means:

* The listener's cost hardly depends on the capture's rate (10 Msps costs about the same per listener as 2.4 Msps); only the shared part grows with the rate.
* Data per listener: ADPCM audio is 4 bits a sample (48 kbit/s at 12 kHz, 96 at 24 kHz), PCM four times that; a 1024-bin waterfall at 10 rows a second is about 82 kbit/s. Your upload is the usual limit: 30 listeners at 130 kbit/s need about 4 Mbit/s up.
* A Raspberry Pi 4 core is roughly 4 to 6 times slower than the core measured above (estimate, not measured): about 2 to 3 % per NFM listener and 5 to 6 % per SSB listener, so a Pi 4 can serve 10 to 20 listeners next to a light main mode (FM, ADS-B), fewer beside DVB-T2 or DAB. Keep `web_cpu_cap_pct` so that the main receiver keeps enough.
* The radio itself: an RTL-SDR (2.4 Msps) covers 2 MHz (for example the 2 m band, the air band in parts, the FM band in parts); an Airspy, SDRplay or HackRF at 10 Msps covers 8 to 10 MHz; HF direct-sampling radios (RX888, Red Pitaya, SDRplay) cover all of HF in one capture.
* Memory: about 1 MB per listener (filters and queues) plus the shared FFT (a few MB at 10 Msps).

Run it on your own machine:

```
onair-web-load --rate 2.4e6 --users 0,1,8,32 --seconds 5 --mode nfm
onair-web-load --rate 10e6 --users 0,8 --mode usb --no-spectrum
```

## Decoders

Listeners subscribe to decoders by kind (`welcome.decoders` lists what the server offers and the policy allows). In this build there is one: **`host`**, the results of the server's own receiver (the mode chosen in the control page, as its telemetry JSON). The machinery for more is in place (`DecoderFactory` / `WebDecoder` in `server/web/web_hub.h`): a decoder gets its own slice of the capture at the rate it asks for, identical subscriptions (same kind, same kHz) share one instance, and its results go to every subscriber as `event` messages. Wiring the existing receivers (ADS-B, AIS, APRS, FT8 / FT4 / WSPR, SSTV, WEFAX, weather satellites) as web decoders is the next step; their policy classes are already in `web_policy.cpp`.

## The browser UI and the directory (extension points)

* **Browser UI.** Static files in `web/onair-web/` (or `web_root`) are served at `/`: `index.html` and whatever it loads from the same origin (scripts, styles, images, WebAssembly). Only plain file names are served (letters, digits, `.`, `-`, `_`), never dot files or anything outside the folder, at most 16 MB a file. The page talks to `/web/v1/ws` (docs/ONAIR-WEB-PROTOCOL.md) and may read `/web/v1/info` before connecting.
* **Directory.** `server/web/web_directory.h` is the place for an optional heartbeat that lists the server in a public directory of OnAir Web servers (`web_directory=1`, `web_directory_url`, `web_public_url`). It is off by default and not built in yet; when it is, it may send only the public `/web/v1/info` JSON and the public URL.

## Not done yet / known limits

* Not tried on a real radio, a real browser over the internet, a phone, a Raspberry Pi, or with a real certificate.
* IPv4 only.
* No browser UI in this build (the placeholder page); no Opus (no codec library in OnAir: ADPCM and PCM only).
* Only the `host` decoder; per-listener ADS-B / AIS / APRS / FT8 / SSTV / WEFAX / satellite decoders are not wired yet.
* No S-meter while a listener has audio off (the receiver of a listener without audio is not run at all, which keeps waterfall-only listeners nearly free).
* WFM is mono (no stereo decoder, no RDS for web listeners).
* The admin user list and stats are JSON only (no admin page).

## Browser client (web/onair-web/)

Files: `index.html`, `style.css`, `app.js` (UI and WebSocket), `onair-lib.js` (pure helpers: ADPCM decode, frame parsing, messages, frequency and bin mapping, zoom and pan, palettes, S-meter, result tables) and `onair-worklet.js` (audio jitter buffer, run in an AudioWorklet). Everything is served from the same origin (the server's content security policy allows nothing else). Tune by clicking a digit half, scrolling on the digits, arrow keys (Shift or Page Up/Down for 10 steps), or by clicking the spectrum; drag to pan, wheel or pinch to zoom (the zoomed slice is requested from the server as a finer spectrum span). Keys: 1 to 6 modes, M mute, + and - zoom. An invite link is `http://host:8073/#invite=TOKEN` (the token never reaches the server's logs). The notice banner shows the server's `notice` text from `/web/v1/info` when it sends one, else a generic one. The S-meter scale is relative to full scale (S9 = -66 dBFS), not calibrated.

Tests for the pure code (node built-ins only, not part of CMake): `node tests/web/run.js`.
