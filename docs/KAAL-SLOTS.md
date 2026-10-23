# KAAL v4 voice slots: interface and bit budgets for the codec

Status: frame version 4, branch kaal-phy-030 (2026-10-22). Written for the codec agent (kaal-codec-030). The PHY side is in
core/include/dect2/kaal/kaal_slots.h (constants and the slot payload struct, header only) and core/src/kaal/kaal_v4.cpp. Measured
sensitivity numbers are in docs/KAAL-NEXT.md; this file fixes the bit budgets, which will not change inside version 4.

## 1. What the codec gets

- One channel = one slot. Four slots per superframe, numbered 0..3. Each slot is independent: its own codec instance, its own talker.
- Superframe period: **40 ms** (640 symbols at 16 ksym/s). Every 40 ms each active voice slot carries **one payload of exactly N bits**
  (table below), i.e. two 20 ms codec frames, or one 40 ms frame, whichever the codec prefers. The PHY does not look inside.
- Delivery to the codec: per slot and per 40 ms either the N bits **bit-exact** (the slot's LDPC word passed all its parity checks), or an
  **erasure** (nullptr / ok = false). There are no residual bit errors to handle; plan for whole-payload loss (40 ms, i.e. both 20 ms
  frames of a pair at once) and conceal it. Typical design point: 1 % payload loss at the sensitivity limit.
- Audio: 16 kHz mono int16 at both ends (as KaalCodec3). 40 ms = 640 samples per payload.
- No codec header is needed for the profile: the PHY tells the codec which profile the slot carried (the receiver finds it from the code
  length, see 3). If the codec wants its own frame type / mode bits it puts them inside the N bits.

## 2. Payload budgets (per slot, per 40 ms)

| profile id | name | modulation | payload bits N / 40 ms | codec bit rate | LDPC info k | code rate | coded bits |
|---|---|---|---|---|---|---|---|
| 1 | V80 | 16-QAM | 320 | **8.0 kbit/s** | 320 | 0.588 | 544 |
| 2 | V60 | 16-QAM | 240 | **6.0 kbit/s** | 240 | 0.441 | 544 |
| 3 | V48 | 16-QAM | 192 | **4.8 kbit/s** | 192 | 0.353 | 544 |
| 4 | V32R | QPSK | 128 | **3.2 kbit/s** (robust) | 128 | 0.471 | 272 |

Per 20 ms codec frame: V80 160 bits, V60 120 bits, V48 96 bits, V32R 64 bits.

Suggested targets for the codec agent: a 6.0 kbit/s main profile (default V60), an 8.0 kbit/s "best" one, 4.8 kbit/s for weak links, and
3.2 kbit/s for the robust (QPSK) slot if a low-rate mode is planned. Any subset is fine; a profile the codec does not offer is simply not
selectable in the TX panel.

Channel need, measured (KAAL-NEXT.md section 6): V32R needs the lowest Es/N0, V48 1.7 dB more, V60 1.3 dB more than V48, and V80 2 dB
more than V60.

## 3. Slot types the PHY also carries (not codec business, for completeness)

| profile id | name | modulation | k | content per 40 ms |
|---|---|---|---|---|
| 0 | IDLE | 16-QAM | 304 | fill (same format as D68 with type 0), keeps the signal continuous |
| 5 | D68 | 16-QAM | 304 | data: 16-bit header + 34 bytes + CRC-16 = 6.8 kbit/s |
| 6 | D26R | QPSK | 136 | robust data: 16-bit header + 13 bytes + CRC-16 = 2.6 kbit/s |

Every k is different, so the receiver identifies a slot's profile by which k makes the LDPC word decode (it tries the last good one first,
and the slot map in the control channel says which one it is once that has decoded).

## 4. What 1, 2, 3, 4 active voice slots means

A slot that has no voice carries data (D68) when the TX has data queued (text, beacon, files), otherwise IDLE fill. The air signal is the same
with 1 or 4 talkers (same power, same PAPR, same sync), so a receiver can always lock.

| active voice slots (V60) | voice | data capacity left (D68 slots + control channel) |
|---|---|---|
| 0 | - | 4 x 6.8 + 0.66 = 27.9 kbit/s |
| 1 | 6.0 kbit/s | 3 x 6.8 + 0.66 = 21.1 kbit/s |
| 2 | 2 x 6.0 | 14.3 kbit/s |
| 3 | 3 x 6.0 | 7.5 kbit/s |
| 4 | 4 x 6.0 | 0.66 kbit/s (control channel only: beacon, IDs, text) |

Measured 1 % payload-loss points (AWGN, docs/KAAL-NEXT.md 6.1): V32R 5.0 dB, V48 6.7 dB, V60 8.0 dB, V80 10.0 dB Es/N0.
The codec registers itself with `registerSlotCodec()` (kaal_slots.h: SlotVoiceEncoder / SlotVoiceDecoder, 640 samples per payload);
the TX generator and the live receiver use it as soon as it is registered.

## 5. Timing and latency

Slot s occupies symbols 64 + 144 s .. 64 + 144 s + 143 of the superframe (s = 0..3), i.e. 4.0 + 9.0 s ms .. 13.0 + 9.0 s ms. The TX needs
the slot's N bits at the start of that slot; the RX delivers them at the end of the slot plus one LDPC decode. No interleaving across
superframes. One-way PHY delay = 40 ms (fill a payload) + at most 40 ms (wait for the slot) + 9 ms (slot air time) + decode, about 50 to
90 ms depending on the slot, plus the codec's own look-ahead.

## 6. C++ interface (core/include/dect2/kaal/kaal_slots.h)

```cpp
namespace dect2::kaal::v4 {
constexpr int kSlots = 4;
constexpr double kSuperframeS = 0.040;
constexpr int kPcmPerPayload = 640;                 // 16 kHz samples per 40 ms
enum Profile : uint8_t { kIdle = 0, kV80 = 1, kV60 = 2, kV48 = 3, kV32R = 4, kD68 = 5, kD26R = 6 };
int payloadBits(Profile p);                         // N of the table (voice) or data bytes * 8 (data)
struct SlotPayload {                                // one slot of one superframe
    Profile profile = kIdle;
    bool ok = false;                                // RX: the LDPC word decoded (bits valid); TX: ignored
    uint16_t nbits = 0;                             // == payloadBits(profile) for voice
    uint8_t bits[40] = {};                          // payload, MSB first, packed 8 per byte
};
}
```

The transmitter takes, per superframe, four `SlotPayload` (or a callback per slot); the receiver hands out four per superframe. The codec
wrapper converts 640 PCM samples into one payload and back; it owns the concealment.
