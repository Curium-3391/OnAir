// Project Horus protocol layer (horus_proto.h): CRC, Golay (23,12), layer 2 frames, binary packets, UKHAS sentences.
// Written for OnAir from the horusdemodlib wiki; the layer 2 constants follow horusdemodlib's horus_l2.c / golay23.c
// (LGPL-2.1, David Rowe and Tomas Haerdin), see THIRD_PARTY.md. No code was copied.
#include "dect2/horus/horus_proto.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace dect2::horus {

uint16_t crc16(const uint8_t* p, size_t n) {
    uint16_t c = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
        c ^= uint16_t(p[i]) << 8;
        for (int k = 0; k < 8; k++) c = (c & 0x8000) ? uint16_t((c << 1) ^ 0x1021) : uint16_t(c << 1);
    }
    return c;
}

// ---------------------------------------------------------------- Golay (23,12)
namespace {
constexpr uint32_t kGolayPoly = 0xC75;   // x^11 + x^10 + x^6 + x^5 + x^4 + x^2 + 1

uint32_t syndrome(uint32_t w) {
    for (int x = 11; x >= 0; x--) if (w & (1u << (11 + x))) w ^= kGolayPoly << x;
    return w & 0x7FF;
}

struct GolayTable {
    uint32_t err[2048];
    GolayTable() {
        for (auto& e : err) e = 0xFFFFFFFF;
        err[0] = 0;
        for (int a = 0; a < 23; a++) {
            err[syndrome(1u << a)] = 1u << a;
            for (int b = a + 1; b < 23; b++) {
                err[syndrome((1u << a) | (1u << b))] = (1u << a) | (1u << b);
                for (int c = b + 1; c < 23; c++) err[syndrome((1u << a) | (1u << b) | (1u << c))] = (1u << a) | (1u << b) | (1u << c);
            }
        }
    }
};
const GolayTable& golayTable() { static const GolayTable t; return t; }

int popcount(uint32_t v) { int n = 0; while (v) { v &= v - 1; n++; } return n; }
inline int getBitLsb(const uint8_t* b, int i) { return (b[i >> 3] >> (i & 7)) & 1; }
inline void setBitLsb(uint8_t* b, int i, int v) { if (v) b[i >> 3] |= uint8_t(1u << (i & 7)); else b[i >> 3] &= uint8_t(~(1u << (i & 7))); }
inline int getBitMsb(const uint8_t* b, int i) { return (b[i >> 3] >> (7 - (i & 7))) & 1; }

bool isPrime(int n) { if (n < 2) return false; for (int d = 2; d * d <= n; d++) if (n % d == 0) return false; return true; }

// bit i of the region goes to bit (b i) mod nbits (the bits counted from the least significant of each byte)
void interleave(uint8_t* d, int nbytes, bool inverse) {
    const int nbits = nbytes * 8, b = interleaverStep(nbytes);
    std::vector<uint8_t> out((size_t)nbytes, 0);
    for (int i = 0; i < nbits; i++) {
        const int j = int((int64_t)b * i % nbits);
        if (!inverse) setBitLsb(out.data(), j, getBitLsb(d, i));
        else setBitLsb(out.data(), i, getBitLsb(d, j));
    }
    std::memcpy(d, out.data(), (size_t)nbytes);
}

// additive scrambler: 15-bit state from 0x4a80, output = bit 1 xor bit 0, shifted right with the output fed into bit 14
void scramble(uint8_t* d, int nbytes) {
    uint32_t s = 0x4a80;
    for (int i = 0; i < nbytes * 8; i++) {
        const uint32_t o = ((s >> 1) ^ s) & 1;
        if (o) d[i >> 3] ^= uint8_t(1u << (i & 7));
        s = (s >> 1) | (o << 14);
    }
}
} // namespace

uint32_t golayEncode(uint32_t data12) {
    const uint32_t w = (data12 & 0xFFF) << 11;
    return w | syndrome(w);
}

uint32_t golayDecode(uint32_t w, int* corrected) {
    w &= 0x7FFFFF;
    const uint32_t e = golayTable().err[syndrome(w)];
    if (corrected) *corrected = e == 0xFFFFFFFF ? 0 : popcount(e);
    if (e != 0xFFFFFFFF) w ^= e;
    return w >> 11;
}

int frameBytes(int n) {
    const int cw = (8 * n + 11) / 12;
    const int bits = 16 + 8 * n + 11 * cw;
    return (bits + 7) / 8;
}

int interleaverStep(int nbytes) {
    if (nbytes == 43) return 337;   // Horus v1
    if (nbytes == 63) return 389;   // Horus v2
    int b = (int)std::floor(nbytes * 8 / 1.62) + 1;
    while (!isPrime(b)) b++;
    return b;
}

std::vector<uint8_t> encodeFrame(const uint8_t* payload, int n) {
    const int total = frameBytes(n), cw = (8 * n + 11) / 12;
    std::vector<uint8_t> f((size_t)total, 0);
    f[0] = kUw[0]; f[1] = kUw[1];
    std::memcpy(f.data() + 2, payload, (size_t)n);
    int pbit = (2 + n) * 8;   // parity bits packed MSB first after the data
    for (int c = 0; c < cw; c++) {
        uint32_t d = 0;
        for (int k = 0; k < 12; k++) { const int bi = c * 12 + k; d = (d << 1) | uint32_t(bi < 8 * n ? getBitMsb(payload, bi) : 0); }
        const uint32_t par = golayEncode(d) & 0x7FF;
        for (int k = 10; k >= 0; k--, pbit++) if ((par >> k) & 1) f[(size_t)(pbit >> 3)] |= uint8_t(0x80 >> (pbit & 7));
    }
    interleave(f.data() + 2, total - 2, false);
    scramble(f.data() + 2, total - 2);
    return f;
}

void decodeFrame(const uint8_t* body, int n, uint8_t* payload, int* corrected) {
    const int nb = frameBytes(n) - 2, cw = (8 * n + 11) / 12;
    std::vector<uint8_t> d(body, body + nb);
    scramble(d.data(), nb);   // the scrambler is its own inverse
    interleave(d.data(), nb, true);
    std::memset(payload, 0, (size_t)n);
    int fixes = 0, pbit = n * 8;
    for (int c = 0; c < cw; c++) {
        uint32_t w = 0;
        for (int k = 0; k < 12; k++) { const int bi = c * 12 + k; w = (w << 1) | uint32_t(bi < 8 * n ? getBitMsb(d.data(), bi) : 0); }
        for (int k = 0; k < 11; k++, pbit++) w = (w << 1) | uint32_t(getBitMsb(d.data(), pbit));
        int fx = 0;
        const uint32_t dat = golayDecode(w, &fx);
        fixes += fx;
        for (int k = 0; k < 12; k++) { const int bi = c * 12 + k; if (bi < 8 * n && ((dat >> (11 - k)) & 1)) payload[bi >> 3] |= uint8_t(0x80 >> (bi & 7)); }
    }
    if (corrected) *corrected = fixes;
}

// ---------------------------------------------------------------- packets
namespace {
uint16_t le16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
float lef(const uint8_t* p) { uint32_t u = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; float f; std::memcpy(&f, &u, 4); return f; }
void put16(uint8_t* p, unsigned v) { p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); }
void putf(uint8_t* p, float f) { uint32_t u; std::memcpy(&u, &f, 4); for (int i = 0; i < 4; i++) p[i] = uint8_t(u >> (8 * i)); }
std::string callsignFor(const Tables& t, int id) {
    auto it = t.ids.find(id);
    return it != t.ids.end() ? it->second : "ID " + std::to_string(id);
}
} // namespace

bool parseBinary(const uint8_t* p, int n, const Tables& t, Packet& out) {
    if (n != kV1Bytes && n != kV2Bytes) return false;
    if (crc16(p, (size_t)n - 2) != le16(p + n - 2)) return false;
    Packet k;
    const bool v2 = n == kV2Bytes;
    const int o = v2 ? 1 : 0;   // v2 has a 16-bit payload ID, so every field after it sits one byte later
    k.kind = v2 ? 2 : 1;
    k.payloadId = v2 ? le16(p) : p[0];
    k.seq = le16(p + 1 + o);
    k.hour = p[3 + o]; k.minute = p[4 + o]; k.second = p[5 + o];
    k.lat = lef(p + 6 + o); k.lon = lef(p + 10 + o);
    k.altM = le16(p + 14 + o);
    k.speedKmh = p[16 + o]; k.sats = p[17 + o]; k.tempC = int8_t(p[18 + o]);
    k.battV = p[19 + o] * 5.0 / 255.0;
    k.callsign = callsignFor(t, k.payloadId);
    k.raw.assign(p, p + n);
    if (v2) {
        auto it = t.formats.find(k.callsign);
        if (it != t.formats.end()) k.custom = applyFormat(it->second, p + 21, 9);
        if (k.custom.empty()) {   // no format for this payload: the bytes as hex
            char b[4]; std::string h;
            for (int i = 21; i < 30; i++) { snprintf(b, sizeof b, "%02X", p[i]); h += b; }
            k.custom.push_back({"custom", h});
        }
    }
    if (!std::isfinite(k.lat) || !std::isfinite(k.lon)) { k.lat = 0; k.lon = 0; }
    k.sentence = buildSentence(k);
    out = std::move(k);
    return true;
}

std::vector<uint8_t> buildBinary(const Packet& p, int version, const uint8_t custom[9]) {
    const bool v2 = version == 2;
    const int n = v2 ? kV2Bytes : kV1Bytes, o = v2 ? 1 : 0;
    std::vector<uint8_t> b((size_t)n, 0);
    if (v2) put16(b.data(), (unsigned)p.payloadId); else b[0] = uint8_t(p.payloadId);
    put16(b.data() + 1 + o, p.seq);
    b[(size_t)3 + o] = uint8_t(p.hour); b[(size_t)4 + o] = uint8_t(p.minute); b[(size_t)5 + o] = uint8_t(p.second);
    putf(b.data() + 6 + o, (float)p.lat); putf(b.data() + 10 + o, (float)p.lon);
    put16(b.data() + 14 + o, (unsigned)std::lround(std::fmax(0, std::fmin(65535, p.altM))));
    b[(size_t)16 + o] = uint8_t(std::max(0, std::min(255, p.speedKmh)));
    b[(size_t)17 + o] = uint8_t(std::max(0, std::min(255, p.sats)));
    b[(size_t)18 + o] = uint8_t(int8_t(std::max(-128, std::min(127, p.tempC))));
    b[(size_t)19 + o] = uint8_t(std::lround(std::fmax(0, std::fmin(255, p.battV * 255.0 / 5.0))));
    if (v2 && custom) std::memcpy(b.data() + 21, custom, 9);
    put16(b.data() + n - 2, crc16(b.data(), (size_t)n - 2));
    return b;
}

std::string buildSentence(const Packet& p) {
    char b[160];
    std::string s = p.callsign + "," + std::to_string(p.seq);
    snprintf(b, sizeof b, ",%02d:%02d:%02d,%.5f,%.5f,%d", p.hour, p.minute, p.second, p.lat, p.lon, (int)std::lround(p.altM));
    s += b;
    if (p.kind != 0) {
        snprintf(b, sizeof b, ",%d,%d,%d,%.2f", p.speedKmh, p.sats, p.tempC, p.battV);
        s += b;
    }
    for (const auto& c : p.custom) s += "," + c.second;
    snprintf(b, sizeof b, "*%04X", crc16(s));
    return "$$" + s + b;
}

bool parseSentence(const std::string& lineIn, const Tables& t, Packet& out) {
    std::string line = lineIn;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) line.pop_back();
    size_t a = 0;
    while (a < line.size() && line[a] == '$') a++;
    const size_t star = line.rfind('*');
    if (star == std::string::npos || star < a + 1 || line.size() < star + 5) return false;
    const std::string body = line.substr(a, star - a), crcTxt = line.substr(star + 1, 4);
    for (char c : crcTxt) if (!std::isxdigit((unsigned char)c)) return false;
    if ((unsigned)std::strtoul(crcTxt.c_str(), nullptr, 16) != crc16(body)) return false;
    std::vector<std::string> f;
    size_t s = 0;
    for (size_t i = 0; i <= body.size(); i++) if (i == body.size() || body[i] == ',') { f.push_back(body.substr(s, i - s)); s = i + 1; }
    if (f.size() < 6 || f[0].empty()) return false;
    Packet k;
    k.kind = 0;
    k.callsign = f[0];
    k.seq = (uint32_t)std::strtoul(f[1].c_str(), nullptr, 10);
    int h = 0, m = 0, sec = 0;
    if (std::sscanf(f[2].c_str(), "%d:%d:%d", &h, &m, &sec) == 3 || (f[2].size() == 6 && std::sscanf(f[2].c_str(), "%2d%2d%2d", &h, &m, &sec) == 3)) { k.hour = h; k.minute = m; k.second = sec; }
    k.lat = std::strtod(f[3].c_str(), nullptr);
    k.lon = std::strtod(f[4].c_str(), nullptr);
    k.altM = std::strtod(f[5].c_str(), nullptr);
    const CustomFormat* fmt = nullptr;
    if (auto it = t.formats.find(k.callsign); it != t.formats.end()) fmt = &it->second;
    for (size_t i = 6; i < f.size(); i++) {
        const size_t j = i - 6;
        std::string name = fmt && j < fmt->sentence.size() ? fmt->sentence[j] : "field " + std::to_string(i + 1);
        k.custom.push_back({name, f[i]});
    }
    k.raw.assign(line.begin(), line.end());
    k.sentence = "$$" + body + "*" + crcTxt;
    out = std::move(k);
    return true;
}

} // namespace dect2::horus
