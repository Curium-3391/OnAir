# LAN swarm

Several OnAir programs (desktop, Raspberry Pi server, phone) on one local network see each other and cooperate. Everything is opt-in and off at every start.

## Status

| Part | State |
|---|---|
| Announce packets, node table, merged results, track board, range splitting, hit merging, pairing handshake, tokens, rate limit, memory merge | `core/src/net/swarm.cpp`, unit tests in `tests/net/test_swarm.cpp` (passed once with the earlier 12-digit design; the 160-bit rewrite has NOT been built or run) |
| UDP multicast discovery | `core/src/net/swarm_net.cpp` written, passes `-fsyntax-only`, never run |
| Swarm view window | `app/panels/swarm_ui.cpp` (View > LAN swarm), never compiled |
| Pooled results over the network, coordinated scan over the network, remote control channel, memory sync over the network | NOT written (only the pure logic exists) |

## What is sent

Discovery packet (text, at most 512 bytes, every 5 s to 239.255.77.31:47831, TTL 1): random node id, name, radio name, tunable range, current mode, load, a short summary, control port. No IQ, no audio, no positions, no keys. The window shows the exact text before you switch it on. Packets go out only on private IPv4 interfaces (10/8, 172.16/12, 192.168/16, 169.254/16); a received packet is dropped unless its source is private and on the same subnet as one of our interfaces, is not our own, and comes at most once a second per source. A source address can hold at most 4 node ids, the table holds 64, and a node id cannot be taken over by another address while it is alive.

## Pairing and remote control

Remote control needs explicit one-time pairing per node. The node to be controlled shows a 160-bit pairing secret (32 base32 characters, QR code planned). The controlling node gets it by scan, paste or typing, and both run:

```
C: OAPAIR1 HELLO <nonceC>        S: OAPAIR1 CHAL <nonceS>
C: OAPAIR1 PROOF <HMAC(code, "C"|nonceC|nonceS)>
S: OAPAIR1 OK <HMAC(code, "S"|nonceC|nonceS|token)> <token xor HMAC(code, "T"|nonceC|nonceS)>
```

HMAC-SHA256 from `meshcrypto`. The code is never sent; a wrong proof counts and three lock the offer; the code works once and expires after 2 minutes; a second peer is refused while one is mid-handshake; the initiator verifies the server proof, so a fake node that does not know the code is detected.

### Threat model

- Passive listener on the LAN: sees nonces, proofs, the wrapped token. With a 160-bit secret it cannot test guesses offline. (The earlier 12-digit design could be brute-forced offline from one recording; that is why the secret is long.)
- Active attacker on the LAN: cannot pair (no code), cannot impersonate a paired node to the initiator, can only spend the 3 wrong-proof attempts (denial of the pairing attempt, not a break-in).
- Someone who sees the screen or QR code while it is displayed can pair. Show it only on the node to be controlled and only while pairing.
- After pairing, the token is a bearer secret. Plain TCP/UDP would expose it to a sniffer, so control traffic must use the repo TLS layer (`DECT2_WITH_TLS`, as the remote API does) or be HMAC-signed with a key derived from the token. That channel is not written. Until it exists no control is offered (`ctlPort = 0`).
- A short typed PIN is not safe without a PAKE (SPAKE2 / OPAQUE). The repository has no usable group primitive: the Ed25519 code has no Diffie-Hellman, is not constant-time, and X25519 is not exposed. A PAKE would need a new primitive or a reviewed library; none was invented here.
- Control commands are a whitelist (`status, start, stop, set_frequency, set_mode, set_gain, set_rate, scan_slice`). Transmit, PTT, the voice key store, settings and tokens are never reachable. Each peer is rate limited (token bucket, 5/s, burst 10).

## Pooled results

`MessageMerger`: same kind and same content (case and spacing ignored) within 2 s is one row tagged with every node that heard it (at most 16 tags, 5000 rows). `TrackBoard`: one combined ADS-B / AIS track per id with a validated position, newest report wins, 4000 tracks, 2 minute expiry.

## Coordinated scanning

`splitRange(lo, hi, nodes, step)`: walks the range in chunks of each node's instantaneous bandwidth, always giving the next chunk to the node with the least work for its capacity (bandwidth scaled by free load), only to nodes that can tune there; parts nobody can tune are reported. `mergeHits` joins hits within a tolerance (strongest wins, node tags kept).

## Memory sync

`mergeMemories`: last write wins per key, tombstones for deletes, every remote entry validated (key charset and length, text lengths, finite frequency), 10000 entries at most. Only between nodes you paired.

## NEEDS BUILD / TEST

1. Build `test_swarm` and run `ctest -R swarm` (new pairing code tests, `normalisePairingCode`, `offer` refusing short codes).
2. Compile `swarm_net.cpp` (POSIX; Windows returns "not available") and `swarm_ui.cpp` (uses `kModes`, `subIndexOf`, `receiverById`, `pal::warnAmber` as in `remote_api_app.cpp` and `feeds_ui.cpp`; check the includes).
3. Two machines or two network namespaces: announces seen both ways, own packet ignored, nothing from another subnet, nothing when only a public interface exists, TTL 1 confirmed with a packet capture, macOS multicast permission prompt, Linux firewall.
4. Persist the node id and a node name in preferences (it is random per run now); add the engine load figure to the announce.
5. Write the control channel (TLS or signed), wire pairing UI (show code and QR, enter code), then pooled results transport (results already pass through the decoders' message lists; a feed target like `feeds.cpp` is the natural hook), coordinated scan via the scanner, memory sync via `memory_json`.
6. Add the Windows interface enumeration (`GetAdaptersAddresses`) to `swarm_net.cpp`.
7. Fuzz `parseAnnounce` and the pairing lines with a real fuzzer (tests only run a small mutation loop).
