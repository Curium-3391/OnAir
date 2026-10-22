// Sigfox uplink frame layer (see sigfox_proto.h). Tables from the public Sigfox radio specification (EP-SPECS Rev. 1.3, 2019): Table 3-1
// (length indicator, payload, authentication and container sizes), Table 3-2 (convolution codes), Table 3-3 (frame types), section 2 (zones).
#include "dect2/sigfox/sigfox_proto.h"
#include <algorithm>
#include <cstdio>

namespace dect2::sigfox {

namespace {
const FrameType kFt[] = {
    {0x006B, 8, 1, false},  {0x06E0, 8, 2, false},  {0x0034, 8, 3, false},
    {0x008D, 9, 1, false},  {0x00D2, 9, 2, false},  {0x0302, 9, 3, false},
    {0x035F, 12, 1, false}, {0x0598, 12, 2, false}, {0x05A3, 12, 3, false},
    {0x0611, 16, 1, false}, {0x06BF, 16, 2, false}, {0x072C, 16, 3, false},
    {0x094C, 20, 1, false}, {0x0971, 20, 2, false}, {0x0997, 20, 3, false},
    {0x0F67, 16, 1, true},  {0x0FC9, 16, 2, true},  {0x11BE, 16, 3, true},
};
constexpr int kNFt = (int)(sizeof kFt / sizeof kFt[0]);

const Zone kZones[] = {
    {"RC1", 868.130, 869.525, 100, "Europe, Middle East, Africa"},
    {"RC2", 902.200, 905.200, 600, "USA, Mexico, Brazil"},
    {"RC3", 923.200, 922.200, 100, "Japan"},
    {"RC4", 920.800, 922.300, 600, "Latin America, Australia, New Zealand, Asia-Pacific"},
    {"RC5", 923.300, 922.300, 100, "South Korea"},
    {"RC6", 865.200, 866.300, 100, "India"},
};

int popcount16(unsigned v) { int n = 0; for (; v; v &= v - 1) n++; return n; }

void put(std::vector<uint8_t>& b, uint64_t v, int n) { for (int i = n - 1; i >= 0; i--) b.push_back((uint8_t)((v >> i) & 1)); }
}  // namespace

const FrameType* frameTypes(int& count) { count = kNFt; return kFt; }

const FrameType* frameTypeOf(uint16_t ft, int maxErrors) {
    const FrameType* best = nullptr;
    int bestD = 99, ties = 0;
    for (const FrameType& f : kFt) {
        const int d = popcount16((unsigned)(f.ft ^ ft) & 0x1FFF);
        if (d < bestD) { bestD = d; best = &f; ties = 0; } else if (d == bestD) ties++;
    }
    if (bestD > maxErrors || ties) return nullptr;
    return best;
}

uint16_t frameTypeFor(int container, int rank, bool control) {
    for (const FrameType& f : kFt) if (f.container == container && f.rank == rank && f.control == control) return f.ft;
    return 0;
}

int payloadBytes(int container, int li, bool& singleBit) {
    singleBit = false;
    switch (container) {
    case 8: if (li == 0) return 0; if (li >= 2) { singleBit = true; return 0; } return -1;
    case 9: return li == 0 ? 1 : -1;
    case 12: return li == 2 ? 2 : li == 1 ? 3 : li == 0 ? 4 : -1;
    case 16: return li == 3 ? 5 : li == 2 ? 6 : li == 1 ? 7 : 8;
    case 20: return li == 3 ? 9 : li == 2 ? 10 : li == 1 ? 11 : 12;
    default: return -1;
    }
}

int authBytes(int container, int li) {
    bool sb = false;
    const int p = payloadBytes(container, li, sb);
    return p < 0 ? -1 : container - 6 - p;
}

bool layoutFor(int n, bool singleBit, bool bitValue, int& container, int& li) {
    if (singleBit) { container = 8; li = bitValue ? 3 : 2; return true; }
    if (n < 0 || n > 12) return false;
    for (int c : {8, 9, 12, 16, 20})
        for (int l = 0; l < 4; l++) {
            bool sb = false;
            if (payloadBytes(c, l, sb) == n && !sb) { container = c; li = l; return true; }
        }
    return false;
}

uint16_t crc16(const std::vector<uint8_t>& bytes) {
    uint16_t r = 0;   // the start value is not given in the specification; 0 reproduces the public example frame (tests/sigfox)
    for (uint8_t v : bytes) {
        r ^= (uint16_t)(v << 8);
        for (int k = 0; k < 8; k++) r = (r & 0x8000) ? (uint16_t)((r << 1) ^ 0x1021) : (uint16_t)(r << 1);
    }
    return (uint16_t)(r ^ 0xFFFF);
}

std::vector<uint8_t> convEncode(const std::vector<uint8_t>& in, int rank) {
    if (rank <= 1) return in;
    std::vector<uint8_t> out(in.size());
    uint8_t s1 = 0, s2 = 0;   // the previous two input bits; the coder starts from 0 (assumed, not stated)
    for (size_t i = 0; i < in.size(); i++) {
        out[i] = (uint8_t)((in[i] ^ (rank == 2 ? s1 : 0) ^ s2) & 1);
        s2 = s1; s1 = in[i];
    }
    return out;
}

std::vector<uint8_t> convDecode(const std::vector<uint8_t>& in, int rank) {
    if (rank <= 1) return in;
    std::vector<uint8_t> out(in.size());
    uint8_t s1 = 0, s2 = 0;
    for (size_t i = 0; i < in.size(); i++) {
        out[i] = (uint8_t)((in[i] ^ (rank == 2 ? s1 : 0) ^ s2) & 1);
        s2 = s1; s1 = out[i];
    }
    return out;
}

std::vector<uint8_t> buildFrame(uint32_t id, uint32_t mc, const std::vector<uint8_t>& payloadIn, int rank, bool bf, bool singleBit, bool bitValue,
                                const std::vector<uint8_t>& authIn) {
    std::vector<uint8_t> payload(payloadIn.begin(), payloadIn.begin() + (ptrdiff_t)std::min<size_t>(payloadIn.size(), 12));
    if (singleBit) payload.clear();
    int container = 8, li = 0;
    layoutFor((int)payload.size(), singleBit, bitValue, container, li);
    const int na = container - 6 - (int)payload.size();
    std::vector<uint8_t> c;   // container bytes
    c.push_back((uint8_t)((li << 6) | ((bf ? 1 : 0) << 5) | ((mc >> 8) & 0x0F)));
    c.push_back((uint8_t)(mc & 0xFF));
    for (int k = 0; k < 4; k++) c.push_back((uint8_t)((id >> (8 * k)) & 0xFF));   // the ID bytes in reverse order
    c.insert(c.end(), payload.begin(), payload.end());
    uint32_t h = id * 2654435761u ^ (mc * 40503u + 12345u);   // made-up authentication bytes: the real ones need the device key
    for (int k = 0; k < na; k++) {
        h = h * 1664525u + 1013904223u;
        c.push_back(k < (int)authIn.size() ? authIn[(size_t)k] : (uint8_t)(h >> 16));
    }
    const uint16_t crc = crc16(c);
    std::vector<uint8_t> content;
    for (uint8_t v : c) put(content, v, 8);
    put(content, crc, 16);
    std::vector<uint8_t> b;
    for (int j = 0; j < kPreambleBits; j++) b.push_back((uint8_t)(j % 2 == 0));
    put(b, frameTypeFor(container, rank), kFtBits);
    const std::vector<uint8_t> coded = convEncode(content, rank);
    b.insert(b.end(), coded.begin(), coded.end());
    return b;
}

bool parseFrame(const std::vector<uint8_t>& bits, size_t pos, Uplink& out) {
    if (pos + kFtBits > bits.size()) return false;
    uint16_t ft = 0;
    for (int i = 0; i < kFtBits; i++) ft = (uint16_t)((ft << 1) | (bits[pos + (size_t)i] & 1));
    const FrameType* f = frameTypeOf(ft, 1);
    if (!f) return false;
    const size_t nContent = (size_t)(8 * f->container + 16);
    if (pos + kFtBits + nContent > bits.size()) return false;
    std::vector<uint8_t> coded(bits.begin() + (ptrdiff_t)(pos + kFtBits), bits.begin() + (ptrdiff_t)(pos + kFtBits + nContent));
    const std::vector<uint8_t> d = convDecode(coded, f->rank);
    std::vector<uint8_t> c((size_t)f->container);
    for (size_t i = 0; i < c.size(); i++) { uint8_t v = 0; for (int k = 0; k < 8; k++) v = (uint8_t)((v << 1) | d[i * 8 + (size_t)k]); c[i] = v; }
    uint16_t crc = 0;
    for (int k = 0; k < 16; k++) crc = (uint16_t)((crc << 1) | d[(size_t)f->container * 8 + (size_t)k]);
    if (crc16(c) != crc) return false;
    Uplink u;
    u.rank = f->rank; u.container = f->container; u.ft = f->ft; u.control = f->control;
    u.li = c[0] >> 6;
    u.downlinkRequest = (c[0] >> 5) & 1;
    u.rep = (c[0] >> 4) & 1;
    u.mc = (uint32_t)((c[0] & 0x0F) << 8 | c[1]);
    u.id = (uint32_t)c[2] | (uint32_t)c[3] << 8 | (uint32_t)c[4] << 16 | (uint32_t)c[5] << 24;
    const int p = payloadBytes(u.container, u.li, u.singleBit);
    if (p < 0) return false;
    u.bitValue = u.singleBit && (u.li & 1);
    u.payload.assign(c.begin() + 6, c.begin() + 6 + p);
    u.auth.assign(c.begin() + 6 + p, c.end());
    u.crc = crc;
    u.crcOk = true;
    out = u;
    return true;
}

std::string idHex(uint32_t id) {
    char b[12];
    snprintf(b, sizeof b, "%08X", id);
    return b;
}

std::string hexOf(const std::vector<uint8_t>& b) {
    std::string s;
    char t[4];
    for (uint8_t v : b) { snprintf(t, sizeof t, "%02X", v); s += t; }
    return s;
}

const Zone* zones(int& count) { count = (int)(sizeof kZones / sizeof kZones[0]); return kZones; }

}  // namespace dect2::sigfox
