# LAN swarm

Several OnAir programs (desktop, Raspberry Pi server, phone) on one local network see each other and cooperate. Everything is opt-in, off at every start, and stays on the local network.

## Status

| Part | Files | State |
|---|---|---|
| Announce packets, node table, merged results, track board, range splitting, hit merging, pairing, tokens, rate limit, memory merge | `core/src/net/swarm.cpp`, test `swarm` | unit tests |
| Control channel (encrypted + authenticated), commands, pooled results / tracks, coordinated scan, memory sync, pairing over TCP | `core/src/net/swarm_ctl.cpp`, test `swarm_ctl` | unit tests, fake nodes and real loopback sockets |
| UDP multicast discovery (POSIX and Windows interface enumeration) | `core/src/net/swarm_net.cpp` | compiles on POSIX; Windows branch never compiled; discovery never run on a real network |
| Swarm window: preview, join, nodes, pairing, control buttons, scan, pooled table | `app/panels/swarm_ui.cpp` (View > LAN swarm) | see NEEDS BUILD/TEST |

## What is sent

Discovery packet (text, at most 512 bytes, every 5 s to 239.255.77.31:47831, TTL 1): random node id (kept in `swarm.txt` so it is stable), name, radio name, tunable range, current mode, load, a short summary, control port. No IQ, no audio, no positions, no keys. The window shows the exact text before you switch it on. Packets go out only on private IPv4 interfaces (10/8, 172.16/12, 192.168/16, 169.254/16); a received packet is dropped unless its source is private and on the same subnet as one of our interfaces, is not our own, and comes at most once a second per source. A source address can hold at most 4 node ids, the table holds 64, and a node id cannot be taken over by another address while it is alive.

## Pairing

Remote control needs explicit one-time pairing per node. The node to be controlled shows a 160-bit pairing secret (32 base32 characters) for 2 minutes. The controlling node types or pastes it:

```
C: OAPAIR1 HELLO <nonceC>        S: OAPAIR1 CHAL <nonceS>
C: OAPAIR1 PROOF <HMAC(code, "C"|nonceC|nonceS)>
S: OAPAIR1 OK <HMAC(code, "S"|nonceC|nonceS|token)> <token xor HMAC(code, "T"|nonceC|nonceS)>
```

HMAC-SHA256 from `meshcrypto`. The code is never sent; a wrong proof counts and three lock the offer; the code works once; a second peer is refused while one is mid-handshake; the initiator verifies the server proof, so a node that does not know the code is detected. The resulting 96-bit token is stored in `swarm.txt` (mode 0600) on both sides and can be revoked from the window.

## Control channel

After pairing, every connection to the control port (TCP, default 47832) starts with `OACTL1 HELLO <keyId> <nonceC>`; the key id is the first 8 bytes of HMAC(token, "OACTL1 id"), so the token is never sent again. Keys per direction come from HMAC-SHA256(token, label | nonceC | nonceS). Frames are `u32 length | u64 sequence | AES-256-CTR ciphertext | HMAC-SHA256 tag`, encrypt-then-MAC, the tag is checked before anything is decrypted, the sequence must be exactly the next number (replay, reorder and dropped frames end the session), frames are limited to 64 KB, a revoked token ends open sessions, the server serves at most 8 sessions and 20 requests per second per session. Nothing new is invented: standard encrypt-then-MAC with the repository's AES-CTR and HMAC-SHA256.

Commands (a whitelist, everything else is "ERR not allowed"): `status, nodeinfo, start, stop, set_frequency, set_mode, set_gain, set_rate, scan_slice, scan_slices, results, tracks, mem_get, mem_put`. Transmit, PTT, the voice key store, settings, tokens and pairing are not reachable by any command. In the app, `set_gain` and `set_rate` answer "not supported on this node" and scanning answers "this node cannot scan yet" (see below).

### Threat model

- Passive listener on the LAN: sees nonces, proofs and ciphertext. Cannot guess the pairing secret offline (160 bits) and cannot read or alter control traffic (96-bit token key, AES-256 and HMAC). Traffic analysis still shows who talks to whom and how much.
- Active attacker on the LAN: cannot pair (no code), cannot impersonate a paired node (needs the token to produce a valid tag), cannot replay or reorder frames. It can spend the three wrong-proof attempts of an open pairing offer (the user starts again) and can open sockets up to the limit of 8 (denial of service on the control port, not a break-in). The announce packets are not authenticated: a LAN attacker can list fake nodes in the swarm view, and cannot make them controllable without the token.
- Someone who sees the pairing code on screen while it is shown can pair. Show it only for the moment of pairing; it expires after 2 minutes and works once.
- The token is a 96-bit bearer secret stored in plain text in the data folder (mode 0600). Anyone who can read that file can control the node it belongs to. The channel offers no forward secrecy: a stolen token also decrypts recorded sessions. There is no key exchange primitive in the repository (no X25519 / DH, the Ed25519 code is not constant-time), so none was added. TLS (the repo's `tls_conn`) would give forward secrecy but needs certificates; that was not done.
- A short typed PIN would need a PAKE (SPAKE2 / OPAQUE). Not available, hence the long secret.
- Clock: pooled messages carry the sender's Unix time. Nodes whose clocks differ by more than a day have their messages rejected; a difference of more than the merge window (5 s) defeats de-duplication of the same message heard by two nodes.

## Pooled results and the combined map

`MessageMerger`: same kind and same content (case and spacing ignored) within the window is one row tagged with every node that heard it (at most 16 tags, 5000 rows). Each node serves its own decoded messages from the Messages database (`msgdb`, which never holds voice or encrypted content) since a sequence number, 200 per request; a node never relays another node's messages. `TrackBoard`: one combined ADS-B / AIS track per id with a validated position, newest report wins, 4000 tracks, 2 minute expiry. The window shows the pooled table; drawing the combined tracks on the map widget is NOT done.

## Coordinated scanning

`splitRange` walks the range in chunks of each node's instantaneous bandwidth (the announce does not carry it yet: 2 MHz is assumed in the window), always giving the next chunk to the node with the least work for its capacity (bandwidth scaled by free load), only to nodes that can tune there; parts nobody can tune are reported. `coordinateScan` sends each node all its slices in one `scan_slices` request, drops hits outside the slice a node was given, reports nodes that fail with their slices as uncovered, and `mergeHits` joins hits within a tolerance (strongest wins, node tags kept). The protocol and merge are tested with fake nodes; **the app's own node cannot scan yet** (`scanSlice` is not connected to the scanner), so in the app a scan reports every node as unable.

## Memory sync

`mergeMemories`: last write wins per key, every remote entry validated, 10000 entries at most. In the app a memory's key is its frequency + mode, "modified" is the later of created / last used; new memories are added and name / notes are updated when the other side is newer. Deletions are not synced. Only between nodes you paired, only when you press "Sync memories".

## NEEDS BUILD / TEST

1. Build all new files in the real build: `core/src/net/swarm*.cpp`, `app/panels/swarm_ui.cpp` (uses `kModes`, `subIndexOf`, `receiverById`, `memoryBank`, `mdb::global`, `pal::okGreen`). Run `ctest -R swarm`.
2. Windows: build `swarm_net.cpp` (GetAdaptersAddresses, `IP_MULTICAST_TTL` as DWORD) and run the loopback socket test.
3. Two machines: announces both ways, own packet ignored, nothing from another subnet, nothing with only a public interface, TTL 1 confirmed with a packet capture, macOS local-network permission prompt, Linux firewall, Android (the UI file is desktop only).
4. The window: join / leave twice (thread joins), pairing between two instances, Start / Stop / Tune, memory sync, pooled table, forget / revoke; check that nothing blocks the frame when a node is unreachable (the buttons call the network on the UI thread with a 20 s timeout: move to a worker if it shows).
5. Not done: scanner hookup (`scanSlice`), `set_gain` / `set_rate` in the app host, announcing the instantaneous bandwidth, drawing pooled tracks on the map, a QR code for the pairing secret, TLS for forward secrecy, Android/phone nodes (needs a build with the same core), persisting the "Tune" MHz field.
6. A real fuzzer on `parseAnnounce`, `parseCommand`, and the frame reader (the tests run a small mutation loop and malformed socket cases only).
