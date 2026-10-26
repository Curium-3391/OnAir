# OnAir Web directory

A public list of OnAir Web servers. Servers list themselves with a signed heartbeat; nobody has to be approved. The list is a JSON API plus a small page (search, band filter, map pins).

Parts:

* `web/onair-directory/` is the Cloudflare Worker, the D1 schema and the static page.
* `server/web/web_directory*.cpp` is the heartbeat in `onair-server` (off by default).
* Tests: `web/onair-directory/test/` (node, `npm test`) and `tests/server/test_web_directory.cpp` (C++).

## Why D1 and not KV

The list needs filters (band, text, distance), atomic rate-limit counters, a unique URL per server and a block list. D1 does all of that with plain SQL. KV is eventually consistent, so counters and "first registration wins" would be racy. The directory is small (a few hundred rows), so D1's free tier is plenty.

## What a server sends

Every 5 minutes (`web_directory_interval`, 60 to 3600 s) one HTTPS POST to `<directory>/api/v1/heartbeat`:

| field | content |
|---|---|
| `id` | `onair-` + 16 hex of SHA-256 of the public key |
| `pub` | Ed25519 public key (hex) |
| `ts` | unix seconds (must be within 5 minutes of the directory clock and newer than the last accepted one) |
| `name`, `desc`, `contact`, `radio`, `ver` | free text, size limited (48, 280, 80, 48, 24 bytes); contact and desc are optional |
| `url` | the HTTPS address listeners open (`web_public_url`) |
| `loc` | `lat,lon` rounded to 2 decimals (about 1 km) or a Maidenhead locator; the host may choose 0 to 5 decimals; empty is allowed |
| `freq` | coverage in Hz ranges: the capture window, or `web_directory_freq` |
| `cap`, `users` | listener capacity and the **number** of current listeners |
| `interval`, `reach` | seconds between beats; 1 = the directory may check the server (see below) |
| `sig` | Ed25519 signature |

Never sent: listener addresses, names, what anyone listens to, the free-text location and antenna of `/web/v1/info`, the server's own IP (the directory sees it only as the connection source and does not store it).

### Signing text

The signature covers this text (UTF-8), identical in `server/web/web_directory_proto.cpp` and `web/onair-directory/src/logic.js`:

```
OnAir-Directory-v1
action=heartbeat
id=...
pub=...
ts=...
name=...
url=...
loc=...
freq=88000000-108000000;1000000-30000000
radio=...
cap=...
users=...
interval=...
ver=...
contact=...
desc=...
reach=0
```

A `delete` request signs only `action`, `id`, `pub` and `ts`. A shared test vector (fixed seed, text and signature) is in both test suites, so the two languages cannot drift apart.

### Trust

The first heartbeat of an id fixes its public key. Later requests for that id must verify with that key, so nobody can take over a listing. `ts` must grow, which stops replays. Entries expire three missed heartbeats after the last one (`interval x 3`, 15 minutes by default) and are deleted a day later.

## Turning it on (server owner)

Settings file keys (all off or empty by default):

```
web_directory=1
web_directory_url=https://web.on-air.dev
web_public_url=https://radio.example.org
web_directory_loc=54.69,25.28        # or a locator such as KO24pq; leave out for no location
web_directory_loc_decimals=2         # 0 to 5
web_directory_radio=HackRF Pro
web_directory_contact=               # optional
web_directory_desc=Wideband receiver on the roof
web_directory_freq=                  # optional MHz ranges, e.g. 88-108,1-30; default is the capture window
web_directory_reach=0                # 1 lets the directory check that /web/v1/info answers
web_directory_interval=300
web_directory_key=                   # key file; default web_directory.key beside the settings file
```

1. Set the keys above and start `onair-server --web`.
2. Nothing is sent yet. The server prints the exact JSON body, where it goes and a confirmation code, and says `add web_directory_ack=<code>`.
3. Read the preview. Put the code in the settings file as `web_directory_ack=...` and restart. Changing name, address, location, radio, contact, description, interval, the directory URL or the reach switch changes the code, so you must look again. Time, user count, capture coverage and version change by themselves and do not.
4. HTTPS only: the directory URL and `web_public_url` must start with `https://`, the certificate and host name are checked. The build needs TLS (OpenSSL): the release packages from 3.2 on have it, and the certificate authorities come from the system (the Windows root store, `/etc/ssl/cert.pem` on macOS, the distribution bundle on Linux); `SSL_CERT_FILE` / `SSL_CERT_DIR` override that. Step by step for beginners: docs/HOST-A-RECEIVER.md.

The key is made on first start: 32 random bytes in `web_directory.key` (mode 0600). Keep it; it proves the listing is yours. Losing it means a new id (the old listing expires by itself, or ask the admin to remove it). The Ed25519 code in the repo is not constant-time; it signs only these public messages, nothing a listener can influence.

Remove yourself: `onair-server --directory-remove` (signed delete), or set `web_directory=0` and wait for the entry to expire.

## Abuse protection (Worker)

* Body at most 4 KB, JSON only, every field type- and size-checked, unknown fields refused, control characters refused.
* URL must be http(s), no user name, query or fragment, no odd ports (22, 25, 3306 ...), no IP literals in private, loopback, link-local, carrier-grade or documentation ranges (including IPv6 and the decimal/hex forms), no `.local`, `.internal`, `.lan` ... names. A host name is also resolved through a fixed DNS-over-HTTPS resolver when it is first registered or changes, and every answer must be a public address.
* Rate limits (D1 counters, keyed by a salted hash of the address, never the address): 60 requests per minute per address, 8 per 5 minutes per id, 5 new registrations per address per day, 5 reports per hour per address. At most `MAX_SERVERS` (2000) live entries. One server per URL.
* Block list by host (including sub-domains), id or key. A blocked host is removed at once and refused later.
* `POST /api/v1/report` with `{id, reason, text}`; reasons: abuse, illegal, offline, wrong-info, other. No address is stored with a report.

### Reachability check (opt-in, not a proxy)

Only when the server sent `reach=1`: at most once an hour (and when its URL changes) the Worker fetches `<registered url>web/v1/info` with redirects off, 4 s timeout, reads at most 8 KB and keeps one bit (looks like an OnAir Web server: yes/no). The URL comes only from the signed registration after the checks above; no visitor can make the directory fetch anything. The page shows a green or red dot, grey for "not checked".

## Public API

All GET answers allow any origin (CORS) and are cached for 20 s.

* `GET /api/v1/servers?band=hf&q=text&lat=..&lon=..&radius_km=..&freq_mhz=..&free=1&limit=..` returns `{ok, count, servers:[...]}`. Bands: lf mf hf vhf uhf shf mw fm air marine 2m 70cm 20m 40m 80m dab tv l. With `lat`/`lon` the list is sorted by distance and servers without a location are left out.
* `GET /api/v1/servers/<id>`
* `POST /api/v1/heartbeat`, `POST /api/v1/report`
* `GET /api/v1/health`

## Admin (Worker secret `ADMIN_TOKEN`)

Without the secret the admin API answers 404. With it, send `Authorization: Bearer <token>`:

* `DELETE /api/v1/admin/servers/<id>` removes a server.
* `POST /api/v1/admin/block` with `{kind: host|id|pub, value, note}` blocks and removes matching servers; `DELETE /api/v1/admin/block?kind=..&value=..` unblocks.
* `GET /api/v1/admin/reports`, `DELETE /api/v1/admin/reports/<n>`.

The token is never in the repository; set it only with `wrangler secret put`.

## Deploy steps (owner; nothing here has been run)

From `web/onair-directory/`, signed in to the Cloudflare account that should host it:

1. `npm install`
2. `npx wrangler d1 create onair-directory` and copy the printed id into `database_id` in `wrangler.jsonc`.
3. `npx wrangler d1 execute onair-directory --remote --file=schema.sql`
4. `npx wrangler secret put ADMIN_TOKEN` (a long random string) and `npx wrangler secret put IP_SALT` (another one).
5. Add the route or custom domain (for example `directory.on-air.dev`) to `wrangler.jsonc`.
6. `npm test`, then `npx wrangler deploy`.
7. Check `https://<host>/api/v1/health`, then start one server with the directory on and look at the page.

The hourly cron (`17 * * * *`) removes old entries and counters.

Page notes: it loads MapLibre from unpkg and tiles from tile.openstreetmap.org (the visitor's browser talks to both; the CSP in `public/_headers` allows only these). No analytics, no cookies. If that is not wanted, host the MapLibre files and tiles yourself like the live map does.
