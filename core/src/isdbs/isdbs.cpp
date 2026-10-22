// ISDB-S: modulation table, TMCC bits, the fixed words of the frame and the 8PSK trellis. See isdbs.h for what is and is not verified.
#include "dect2/isdbs/isdbs.h"
#include "dect2/common/mathconst.h"
#include <cmath>

namespace dect2 {
namespace isdbs {

const char* modName(int mod) {
    switch (mod) {
    case kBpsk12: return "BPSK 1/2";
    case kQpsk12: return "QPSK 1/2";
    case kQpsk23: return "QPSK 2/3";
    case kQpsk34: return "QPSK 3/4";
    case kQpsk56: return "QPSK 5/6";
    case kQpsk78: return "QPSK 7/8";
    case kTc8psk23: return "TC8PSK 2/3";
    default: return "unused";
    }
}

int bitsPerSymbol(int mod) { return mod == kBpsk12 ? 1 : mod == kUnused ? 0 : 2; }

int convRate(int mod) {
    switch (mod) {
    case kBpsk12: case kQpsk12: return 0;
    case kQpsk23: return 1;
    case kQpsk34: return 2;
    case kQpsk56: return 3;
    case kQpsk78: return 4;
    default: return -1;
    }
}

bool isConv(int mod) { return mod >= kBpsk12 && mod <= kQpsk78; }

double slotNetBits(int mod) {
    static const double kRate[5] = {1.0 / 2, 2.0 / 3, 3.0 / 4, 5.0 / 6, 7.0 / 8};
    if (mod == kTc8psk23) return 2.0 * kSlotSyms;
    if (!isConv(mod)) return 0;
    return slotCodedBits(mod) * kRate[convRate(mod)];
}

double slotTsBitrate(int mod) { return slotNetBits(mod) * (188.0 / 204.0) / kFrameSeconds; }

bool Tmcc::operator==(const Tmcc& o) const {
    if (valid != o.valid || change != o.change || slotTs != o.slotTs || tsId != o.tsId) return false;
    for (int i = 0; i < 4; i++) if (mode[i] != o.mode[i] || count[i] != o.count[i]) return false;
    return true;
}

int Tmcc::slotMod(int slot) const {
    int first = 0;
    for (int g = 0; g < 4; g++) {
        if (slot >= first && slot < first + count[g]) return mode[g];
        first += count[g];
    }
    return kUnused;
}

int Tmcc::slotsOf(int rel) const {
    int n = 0;
    for (int s = 0; s < kSlots; s++) if (slotTs[s] == rel && slotMod(s) != kUnused) n++;
    return n;
}

int Tmcc::modOf(int rel) const {
    for (int s = 0; s < kSlots; s++) if (slotTs[s] == rel && slotMod(s) != kUnused) return slotMod(s);
    return -1;
}

namespace {
void putBits(std::vector<uint8_t>& v, unsigned val, int n) { for (int i = n - 1; i >= 0; i--) v.push_back((val >> i) & 1); }
unsigned getBits(const uint8_t* b, size_t& pos, int n) { unsigned v = 0; for (int i = 0; i < n; i++) v = (v << 1) | b[pos++]; return v; }
uint16_t crc16(const uint8_t* bits, int n) {
    uint16_t c = 0xFFFF;
    for (int i = 0; i < n; i++) {
        const bool fb = ((c >> 15) & 1) ^ (bits[i] & 1);
        c <<= 1;
        if (fb) c ^= 0x1021;
    }
    return c;
}
}  // namespace

std::array<uint8_t, kFramesPerSuper * kTmccSliceBits> tmccPack(const Tmcc& t) {
    std::vector<uint8_t> pay;
    for (int g = 0; g < 4; g++) putBits(pay, (unsigned)t.mode[g], 4);
    for (int g = 0; g < 4; g++) putBits(pay, (unsigned)t.count[g], 6);
    for (int s = 0; s < kSlots; s++) putBits(pay, t.slotTs[s], 3);
    for (int r = 0; r < kMaxStreams; r++) putBits(pay, t.tsId[r], 16);
    pay.resize(kTmccPayloadBits, 0);
    std::array<uint8_t, kFramesPerSuper * kTmccSliceBits> out{};
    for (int k = 0; k < kFramesPerSuper; k++) {
        uint8_t* o = &out[(size_t)k * kTmccSliceBits];
        for (int i = 0; i < 5; i++) o[i] = (t.change >> (4 - i)) & 1;
        for (int i = 0; i < 43; i++) o[5 + i] = pay[(size_t)k * 43 + i];
    }
    return out;
}

bool tmccUnpack(const uint8_t* bits, Tmcc& t) {
    std::vector<uint8_t> pay(kTmccPayloadBits);
    int change = -1;
    for (int k = 0; k < kFramesPerSuper; k++) {
        const uint8_t* s = bits + (size_t)k * kTmccSliceBits;
        int c = 0;
        for (int i = 0; i < 5; i++) c = (c << 1) | s[i];
        if (change >= 0 && c != change) return false;
        change = c;
        for (int i = 0; i < 43; i++) pay[(size_t)k * 43 + i] = s[5 + i];
    }
    Tmcc r;
    r.change = change;
    size_t pos = 0;
    int total = 0;
    for (int g = 0; g < 4; g++) { r.mode[g] = (int)getBits(pay.data(), pos, 4); if (r.mode[g] > kTc8psk23) return false; }
    for (int g = 0; g < 4; g++) { r.count[g] = (int)getBits(pay.data(), pos, 6); total += r.count[g]; }
    if (total > kSlots) return false;
    for (int s = 0; s < kSlots; s++) r.slotTs[s] = (uint8_t)getBits(pay.data(), pos, 3);
    for (int q = 0; q < kMaxStreams; q++) r.tsId[q] = (uint16_t)getBits(pay.data(), pos, 16);
    r.valid = true;
    t = r;
    return true;
}

void tmccSliceEncode(const uint8_t slice[48], uint8_t out[64]) {
    for (int i = 0; i < 48; i++) out[i] = slice[i] & 1;
    const uint16_t c = crc16(out, 48);
    for (int i = 0; i < 16; i++) out[48 + i] = (c >> (15 - i)) & 1;
}

bool tmccSliceCheck(const uint8_t bits[64]) {
    const uint16_t c = crc16(bits, 48);
    uint16_t r = 0;
    for (int i = 0; i < 16; i++) r = (uint16_t)((r << 1) | (bits[48 + i] & 1));
    return c == r;
}

const std::array<float, kSyncSyms>& syncWord() {
    static const std::array<float, kSyncSyms> w = [] {
        std::array<float, kSyncSyms> a{};
        const uint32_t v = 0x072D957Bu;   // picked for the lowest side lobes of the aperiodic autocorrelation (4 of 32)
        for (int i = 0; i < kSyncSyms; i++) a[i] = ((v >> i) & 1) ? -1.f : 1.f;
        return a;
    }();
    return w;
}

const std::array<float, kFrameIdSyms>& frameIdWord(int frame) {
    static const std::array<std::array<float, kFrameIdSyms>, kFramesPerSuper> w = [] {
        std::array<std::array<float, kFrameIdSyms>, kFramesPerSuper> a{};
        for (int f = 0; f < kFramesPerSuper; f++)
            for (int j = 0; j < kFrameIdSyms; j++) a[f][j] = (__builtin_popcount((unsigned)(f + 1) & (unsigned)j) & 1) ? -1.f : 1.f;   // Walsh rows 1..8
        return a;
    }();
    return w[(size_t)(frame & 7)];
}

namespace {
std::vector<float> lfsr15(unsigned seed, size_t n) {
    std::vector<float> v(n);
    unsigned reg = seed & 0x7FFF;
    for (size_t i = 0; i < n; i++) {
        const unsigned fb = ((reg >> 14) ^ (reg >> 13)) & 1;
        reg = ((reg << 1) | fb) & 0x7FFF;
        v[i] = fb ? -1.f : 1.f;
    }
    return v;
}
}  // namespace

const std::vector<float>& burstSequence() {
    static const std::vector<float> b = lfsr15(0x4A80, (size_t)kBlocks * kBurstSyms);
    return b;
}

std::vector<float> fillerSequence(int slot, size_t n) { return lfsr15(0x1234u + (unsigned)slot * 37u, n); }

int tc8Step(Tc8State& s, int u, int v) {
    // parity check h0 = 11, h1 = 02, h2 = 04 (octal) of the 8-state Ungerboeck code for 8PSK: z0(n) = z0(n-3) + z1(n-1) + z2(n-2), here as a
    // three bit register (a = z0 now, b = z0(n-2) + z2(n-1), c = z0(n-1)); the information bits are z1 = u and z2 = v
    const int label = (v << 2) | (u << 1) | s.a;
    const uint8_t na = (uint8_t)(s.b ^ u), nb = (uint8_t)(s.c ^ v), nc = s.a;
    s.a = na; s.b = nb; s.c = nc;
    return label;
}

cf32 psk8Point(int label) {
    const double a = dect2::math::kPi * 0.25 * label;
    return cf32((float)std::cos(a), (float)std::sin(a));
}

}  // namespace isdbs
}  // namespace dect2
