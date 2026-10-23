// Argos message layers (see argos_proto.h).
#include "dect2/argos/argos_proto.h"
#include "dect2/common/crc.h"
#include <cstdio>
#include <cstdlib>

namespace dect2::argos {

int nearestA2Length(int bits, int tol) {
    int best = 0, bd = tol + 1;
    for (int k = 0; k <= 7; k++) {
        const int L = kMinBits + 32 * k;
        const int d = std::abs(bits - L);
        if (d < bd) { bd = d; best = L; }
    }
    return best;
}

std::string hexOfBits(const std::vector<uint8_t>& b, size_t from, size_t n) {
    std::string s;
    char t[4];
    for (size_t i = 0; i < n; i += 4) {
        int v = 0;
        for (size_t k = 0; k < 4; k++) v = (v << 1) | ((from + i + k < b.size() && i + k < n) ? (b[from + i + k] & 1) : 0);
        snprintf(t, sizeof t, "%X", v);
        s += t;
    }
    return s;
}

std::string hexOf(const std::vector<uint8_t>& bytes) {
    std::string s;
    char t[4];
    for (uint8_t v : bytes) { snprintf(t, sizeof t, "%02X", v); s += t; }
    return s;
}

bool parseA2(const std::vector<uint8_t>& b, A2Message& o) {
    const int n = (int)b.size();
    if (n < kMinBits || n > kMaxBits || (n - kMinBits) % 32 != 0) return false;
    auto get = [&](int from, int cnt) { uint32_t v = 0; for (int i = 0; i < cnt; i++) v = (v << 1) | (b[(size_t)(from + i)] & 1); return v; };
    A2Message m;
    m.totalBits = n;
    m.syncHex = hexOfBits(b, 0, kSyncBits);
    m.lenField = (int)get(kSyncBits, kLenBits);
    m.id = get(kSyncBits + kLenBits, kIdBits);
    m.id20 = m.id >> 8;
    char t[16];
    snprintf(t, sizeof t, "%07X", m.id);
    m.idHex = t;
    const int d0 = kSyncBits + kLenBits + kIdBits;
    m.dataBits = n - d0;
    for (int i = 0; i + 8 <= m.dataBits; i += 8) m.data.push_back((uint8_t)get(d0 + i, 8));
    m.rawHex = hexOfBits(b, 0, (size_t)n);
    o = m;
    return true;
}

std::vector<uint8_t> buildA2(uint32_t sync24, int lenField, uint32_t id28, const std::vector<uint8_t>& data, int dataBits) {
    std::vector<uint8_t> b;
    auto put = [&](uint64_t v, int cnt) { for (int i = cnt - 1; i >= 0; i--) b.push_back((uint8_t)((v >> i) & 1)); };
    put(sync24, kSyncBits);
    put((uint32_t)lenField & 0xF, kLenBits);
    put(id28 & 0x0FFFFFFF, kIdBits);
    for (int i = 0; i < dataBits; i++) {
        const size_t byte = (size_t)i / 8;
        b.push_back(byte < data.size() ? (uint8_t)((data[byte] >> (7 - i % 8)) & 1) : 0);
    }
    return b;
}

uint16_t fcs16(const uint8_t* p, size_t n) { return crc::crc16X25(p, n); }

bool HdlcDeframer::push(int bit) {
    bit &= 1;
    sr_ = ((sr_ << 1) | (uint32_t)bit) & 0xFF;
    if (sr_ == 0x7E) {   // a flag: 0111 1110 (the same in either bit order)
        flags++;
        bool done = false;
        if (inFrame_ && bits_.size() >= 7) {
            bits_.resize(bits_.size() - 7);   // the flag's first seven bits went in as data
            if (bits_.size() % 8 == 0 && bits_.size() >= 32) {
                std::vector<uint8_t> by(bits_.size() / 8, 0);
                for (size_t i = 0; i < bits_.size(); i++) by[i / 8] = (uint8_t)(by[i / 8] | (bits_[i] << (i % 8)));   // LSB first
                HdlcFrame f;
                const size_t n = by.size();
                f.fcsOk = fcs16(by.data(), n - 2) == (uint16_t)(by[n - 2] | by[n - 1] << 8);
                f.bytes = f.fcsOk ? std::vector<uint8_t>(by.begin(), by.end() - 2) : by;
                last_ = f;
                done = true;
            }
        }
        inFrame_ = true;
        bits_.clear();
        ones_ = 0;
        return done;
    }
    if (bit) {
        if (++ones_ >= 7) { if (inFrame_ && !bits_.empty()) aborts++; inFrame_ = false; bits_.clear(); return false; }
    } else {
        if (ones_ == 5) { ones_ = 0; return false; }   // a stuffed zero
        ones_ = 0;
    }
    if (inFrame_) {
        bits_.push_back((uint8_t)bit);
        if (bits_.size() > 8 * 1100) { inFrame_ = false; bits_.clear(); }
    }
    return false;
}

std::vector<uint8_t> hdlcFrameBits(const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> bytes = payload;
    const uint16_t f = fcs16(payload.data(), payload.size());
    bytes.push_back((uint8_t)(f & 0xFF));
    bytes.push_back((uint8_t)(f >> 8));
    std::vector<uint8_t> b;
    auto flag = [&] { for (int i = 0; i < 8; i++) b.push_back((uint8_t)((0x7E >> i) & 1)); };
    flag();
    int ones = 0;
    for (uint8_t v : bytes)
        for (int i = 0; i < 8; i++) {
            const int bit = (v >> i) & 1;
            b.push_back((uint8_t)bit);
            if (bit) { if (++ones == 5) { b.push_back(0); ones = 0; } } else ones = 0;
        }
    flag();
    return b;
}

}  // namespace dect2::argos
