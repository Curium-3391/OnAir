# OnAir Web protocol, version 1

This is the contract between an OnAir Web server (`onair-server` with `web=1`) and a browser page or any other client. The server side is `server/web/web_proto.cpp` (message validation), `web_hub.cpp` (what the messages do) and `web_server.cpp` (HTTP and WebSocket). `tests/server/test_web_proto.cpp` checks the rules below.

Version rule: the version is the integer `proto` (now `1`) and the WebSocket subprotocol `onair-web.v1`. Within version 1 the server may add new **server** message types, new fields in server messages and new decoder kinds; clients must ignore what they do not know. The server never accepts unknown client fields (a message with one is refused), so a new client field means a new protocol version.

## Endpoints

All on one port (default 8073), HTTP/1.1, `Connection: close` on every plain HTTP answer.

| Method, path | What |
| --- | --- |
| `GET /`, `GET /<file>` | the browser UI (static files below `web_root`; a placeholder page when none is installed) |
| `GET /web/v1/info` | public server information, JSON (below) |
| `GET /web/v1/ws` | the listener's WebSocket |
| `GET`/`POST /web/v1/admin/...` | the admin API (see docs/ONAIR-WEB.md); never needed by a listener |

`/web/v1/info`:

```json
{"proto":1,"name":"OnAir Web","location":"Vilnius","antenna":"discone","users":3,"max_users":8,
 "capture":{"centre_hz":145000000,"rate_hz":2400000},"auth":"open","tls":true}
```

`auth` is `open`, `password` or `invite`. No listener addresses, names or frequencies are ever in it.

## WebSocket

* RFC 6455, version 13. The upgrade request may offer `Sec-WebSocket-Protocol: onair-web.v1` (echoed); another subprotocol is refused with 400.
* A request with an `Origin` header must come from the server's own origin (`http(s)://<Host header>`) or one listed in `web_origin`; otherwise 403. Programs that send no `Origin` are allowed (they still have to authenticate).
* Client to server: **text frames only**, masked, not fragmented, at most **4096 bytes**. A binary frame closes with 1003, a fragmented message with 1003, a bigger one with 1009, an unmasked one or bad reserved bits with 1002.
* Server to client: text frames (JSON) and binary frames (spectrum and audio, below). Never masked.
* Keepalive: the server sends a ping every 15 s; a connection from which nothing (a frame, a pong) has arrived for 45 s is closed with 1001. Browsers answer pings by themselves.
* Rate limit: a token bucket of 20 messages a second with a burst of 40. A message over it gets `{"type":"error","code":"rate_limited"}` and is dropped; the third time within 10 s the connection is closed with 4008.

### Close codes

| Code | Meaning |
| --- | --- |
| 1000 | normal (after `bye` or the client's close) |
| 1001 | keepalive timeout, server stopping |
| 1002 / 1003 / 1009 | protocol error / unsupported frame / too big |
| 4000 | invalid message before `hello` |
| 4001 | authentication failed, no `hello` within 5 s, or another message first |
| 4008 | rate limit |
| 4029 | server busy (full, processor or bandwidth cap), or too many failed logins from the address (wait 60 s) |
| 4040 | kicked (by the admin, the processor or bandwidth budget, a slow connection) |
| 4041 | idle or session time limit reached |

Before a close for a reason the server sends a text message (`error` or `kicked`) that says why.

## Session start

1. The client opens the WebSocket and sends, within 5 s:

   ```json
   {"type":"hello","proto":1,"name":"Kris","password":"...","invite":"..."}
   ```

   `proto` is required and must be 1. `name` (optional, up to 32 characters; markup characters removed) is shown to the host only. `password` for `auth: password`; `invite` (an invite token) works in every mode and makes the session an `invited` one. Both at most 128 characters. A wrong password or invite is answered after 300 ms with `error` (`auth`) and close 4001; five failures from one address within 60 s block that address for 60 s.

2. The server answers `welcome`, or `error` with `code: "busy"` and close 4029:

   ```json
   {"type":"welcome","proto":1,"session":17,"class":"public",
    "server":{"name":"OnAir Web","location":"","antenna":""},
    "capture":{"centre_hz":145000000,"rate_hz":2400000},
    "limits":{"max_bins":2048,"max_fps":15,"max_kbps":512,"idle_min":30,"time_limit_min":0,"msg_per_sec":20,"max_message":4096},
    "audio":{"codecs":["adpcm","pcm16"],"rates":[8000,12000,16000,24000,48000]},
    "demods":["am","nfm","wfm","usb","lsb","cw"],
    "decoders":[{"kind":"host","name":"This server's receiver (its mode's results)","needs_freq":false}]}
   ```

   When the host limits frequencies, the welcome also has a `ranges` object (absent otherwise: older servers and open servers never send it; clients that do not know it work as before, the server still refuses what is not allowed):

   ```json
   "ranges":{"restricted":true,"action":"refuse","guard":{"min_hz":200,"max_hz":5000,"fraction":0.05},
             "allow":[{"lo":144000000,"hi":146000000,"modes":["nfm"],"note":"2 m"}],
             "block":[{"lo":145800000,"hi":146000000}],"notes":["ham_hf: Amateur radio HF bands (typical ...)"]}
   ```

   `allow` is the listener's own class's list (not merged; `modes` missing: all modes; `note` optional); `restricted` false with `block` only means "everywhere except". Edges are inclusive Hz (10 significant digits). The passband plus the guard margin (`fraction` of the bandwidth, between `min_hz` and `max_hz`) must lie wholly inside one merged allowed range and must not touch a block. `action` is what a refused `tune` gets: `refuse` (an `error`) or `clamp` (the server moves the frequency, see `state`).

   `class` is `public`, `member` (password) or `invited`. `demods` and `decoders` list only what the host allows. The session starts tuned to the capture's centre in NFM with audio and spectrum off.

## Client messages

Every message is one JSON object with `type`. Unknown fields, wrong types and values out of range are refused with an `error` (`bad_message`, `out_of_range`) and change nothing. Numbers must be finite; integers must be whole.

| type | fields | notes |
| --- | --- | --- |
| `hello` | `proto` (1, required), `name`, `password`, `invite` | first message only |
| `tune` | `freq_hz` (0 .. 1e11), `mode` (`am` `nfm` `wfm` `usb` `lsb` `cw`), `low_hz` + `high_hz` (together, -150000 .. 150000, at least 50 Hz apart) | at least one of them. `freq_hz` must be inside the capture (centre +- 48 % of the rate). The passband is relative to `freq_hz`; a new `mode` without a passband takes the mode's default |
| `rx` | `squelch_db` (-200 .. 0; <= -150 is open), `agc` (`off` `slow` `fast`), `gain_db` (-20 .. 60, used with AGC off and for FM), `deemph_us` (0, 50, 75; WFM) | |
| `audio` | `on` (bool, required), `codec` (`adpcm` default, `pcm16`), `rate` (8000, 12000 default, 16000, 24000, 48000) | refused when the codec and rate would use more than 80 % of `max_kbps` |
| `spectrum` | `on` (bool, required), `bins` (64 .. 4096, default 1024, at most `max_bins`), `fps` (1 .. 30, default 10, at most `max_fps`), `start_hz` + `stop_hz` (together, absolute, at least 1 kHz) | without a span: the whole capture |
| `subscribe` | `decoder` (an id from `welcome.decoders`), `freq_hz` (required when `needs_freq`) | at most 4 a session |
| `unsubscribe` | `decoder`, `freq_hz` | |
| `ping` | `t` (any number, echoed) | |
| `bye` | | the server closes with 1000 |

Passband limits per mode (Hz relative to the tuned frequency): AM and NFM -10000 .. 10000, WFM -110000 .. 110000, USB -1000 .. 10000, LSB -10000 .. 1000, CW -1500 .. 1500. Defaults: AM +-4500, NFM +-6250, WFM +-100000, USB 100 .. 2800, LSB -2800 .. -100, CW +-250 (heard at a 700 Hz tone).

**Forbidden types.** `tx`, `transmit`, `ptt`, `arm`, `tx_arm`, `carrier`, `key`, `keys`, `import_key`, `key_import`, `voicekeys`, `decrypt`, `record`, `recording`, `save`, `upload`, `exec`, `shell`, `config`, `admin` are answered with `{"type":"error","code":"forbidden"}`. There is no way to transmit, arm a transmitter, load or use keys, decrypt, or write to the server's disk from a web session; the server code cannot even reach the transmit path (see docs/ONAIR-WEB.md, Safety).

## Server text messages

| type | fields |
| --- | --- |
| `welcome` | see above |
| `state` | the session's settings after every change: `freq_hz`, `mode`, `low_hz`, `high_hz`, `squelch_db`, `agc`, `gain_db`, `deemph_us`, `audio{on,codec,rate}`, `spectrum{on,bins,fps,start_hz?,stop_hz?}`, `subscriptions` (keys), `degrade` (0 .. 3: the server is sending fewer or smaller spectrum rows to keep within the session's budget) |
| `ranges` | `ranges`: the same object as `welcome.ranges`; sent when the host changes the frequency ranges while you listen. A tuning that is no longer allowed is silenced and answered with one `forbidden_range` error; the client should tune again. Not sent by servers without ranges |
| `capture` | `centre_hz`, `rate_hz`: the host retuned or changed the rate. Tuned frequencies are kept; a session now outside the capture is silent until it tunes again |
| `event` | `decoder` (the subscription key: `kind` or `kind@<kHz>k`), `data` (the decoder's JSON value, at most 64 KB) |
| `pong` | `t` |
| `error` | `code`, `message` (English text) |
| `kicked` | `reason`: `kicked`, `kicked: <admin's text>`, `idle`, `time_limit`, `cpu_budget`, `bandwidth_budget`, `server_busy`, `slow_client`, `bye` |

Error codes: `bad_json`, `bad_message`, `unknown_type`, `forbidden`, `out_of_range`, `not_allowed` (the host has turned it off), `forbidden_range` (the host limits the frequency ranges and this tuning, passband or decoder is outside them or blocked; the text says why and may name the nearest allowed frequency; the session keeps its previous tuning), `rate_limited`, `busy`, `auth`.

**Frequency ranges and the spectrum.** When the host limits ranges, spectrum rows are masked on the server: bins the listener may not see carry the noise floor of the visible part (so the row is still a normal `0x10` frame; the client needs no change to stay correct, only to look good). A `tune` is checked as a whole (frequency, mode, passband together) and, with any range set, the passband must also lie inside the usable 96 % of the capture. A `subscribe` with `freq_hz` is refused with `forbidden_range` when the decoder's slice is not visible; the `host` decoder is silent unless the whole capture is visible. This is an additive change: `proto` stays 1.

The `host` decoder's `data` is `{"mode":"<the server's mode id>","telemetry":{...}}` with the receiver's telemetry (the same JSON as `/api/v1/telemetry` of the remote-control API). It is not sent while the server runs a digital voice or pager mode the host has not enabled for web listeners.

## Binary frames

Little endian. The first byte says what it is.

**Spectrum row** (`0x10`), 32-byte header:

| Offset | Type | |
| --- | --- | --- |
| 0 | u8 | `0x10` |
| 1 | u8 | format version, 1 |
| 2 | u16 | bins (n) |
| 4 | u32 | row sequence number (per session) |
| 8 | f64 | start Hz (absolute, left edge of bin 0) |
| 16 | f64 | stop Hz (right edge of the last bin) |
| 24 | f32 | floor dBFS (value 0) |
| 28 | f32 | top dBFS (value 255) |
| 32 | u8 x n | levels, linear in dB between floor and top |

Rows are paced by the samples (the requested `fps` of signal time), each bin keeps the peak of the finer bins it covers, and the floor follows the noise slowly (about 10 dB under the quietest fifth). A listener whose connection cannot keep up gets fewer rows, never old ones: at most two rows wait in its queue.

**Audio** (`0x20`), 24-byte header, mono:

| Offset | Type | |
| --- | --- | --- |
| 0 | u8 | `0x20` |
| 1 | u8 | codec: 0 PCM 16-bit, 1 IMA ADPCM |
| 2 | u16 | samples in this frame (n) |
| 4 | u32 | frame sequence number (per session) |
| 8 | u32 | sample rate, Hz |
| 12 | f32 | channel level, dBFS (S-meter) |
| 16 | u8 | flags: bit 0 = squelched (no payload, n samples of silence) |
| 17 | u8 | 0 |
| 18 | i16 | ADPCM predictor at the start of the frame |
| 20 | u8 | ADPCM step index at the start of the frame (0 .. 88) |
| 21 | u8 | 0 |
| 22 | u16 | 0 |
| 24 | ... | payload: PCM, n x i16; ADPCM, ceil(n / 2) bytes, low nibble first |

Frames are 40 ms. Every ADPCM frame carries its decoder state, so a lost or dropped frame never breaks the next one. The ADPCM is the standard IMA step table (the same as WAV `0x11` / DVI). A listener that falls behind loses the oldest audio first (after its spectrum rows).

No patented codec is used. Opus is not in OnAir's dependencies; if it is added later it gets a new codec number and is offered only when the server has it.

## Limits

| | |
| --- | --- |
| client message | 4096 bytes, 20 a second (burst 40) |
| JSON | depth 4, 96 values, strings 1024 bytes, valid UTF-8, no NUL, no repeated keys |
| HTTP request head | 8 KB, 48 header fields, 5 s to arrive |
| HTTP body (admin) | 4 KB, `Content-Type: application/json` |
| connections | `max(16, 3 x web_max_users + 8)` in total, 12 per address |
| sessions | `web_max_users` (8), `web_max_per_ip` (2) |
| decoders | `web_max_decoders` shared instances (8), 4 subscriptions a session |
| outgoing queue | 384 KB a session: spectrum rows dropped first, then the oldest audio; control messages and events are never dropped (a client that does not read at all is closed with `slow_client`) |

## Example (JavaScript)

```js
const ws = new WebSocket(`wss://${location.host}/web/v1/ws`, "onair-web.v1");
ws.binaryType = "arraybuffer";
ws.onopen = () => ws.send(JSON.stringify({type: "hello", proto: 1, name: "me"}));
ws.onmessage = (e) => {
  if (typeof e.data === "string") {
    const m = JSON.parse(e.data);
    if (m.type === "welcome") {
      ws.send(JSON.stringify({type: "tune", freq_hz: 145500000, mode: "nfm"}));
      ws.send(JSON.stringify({type: "audio", on: true, codec: "adpcm", rate: 12000}));
      ws.send(JSON.stringify({type: "spectrum", on: true, bins: 1024, fps: 10}));
    }
    return;
  }
  const v = new DataView(e.data);
  if (v.getUint8(0) === 0x10) { /* spectrum: v.getUint16(2, true) bins from offset 32 */ }
  if (v.getUint8(0) === 0x20) { /* audio: decode, play through an AudioWorklet */ }
};
```
