// SBAS L1 message content (RTCA DO-229, public description). See sbas_msg.h for the bit numbering.
#include "dect2/sbas/sbas_msg.h"
#include "dect2/gnss/gnss_msg.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace dect2 {
namespace sbas {

uint32_t getBits(const uint8_t* b, int first, int len) {
    uint32_t v = 0;
    for (int i = 0; i < len; i++) v = (v << 1) | (b[first + i] & 1u);
    return v;
}
int32_t getSigned(const uint8_t* b, int first, int len) {
    uint32_t v = getBits(b, first, len);
    if (len < 32 && (v & (1u << (len - 1)))) return (int32_t)(v | (~0u << len));
    return (int32_t)v;
}
void putBits(uint8_t* b, int first, int len, uint32_t v) {
    for (int i = 0; i < len; i++) b[first + i] = (uint8_t)((v >> (len - 1 - i)) & 1u);
}

bool crcOk(const uint8_t* bits) { return crc24q(bits, 226) == getBits(bits, 226, 24); }
int preambleIndex(const uint8_t* bits) {
    const uint32_t p = getBits(bits, 0, 8);
    for (int i = 0; i < 3; i++) if (p == kSbasPreamble[i]) return i;
    return -1;
}
int messageType(const uint8_t* bits) { return (int)getBits(bits, 8, 6); }

const char* typeName(int t) {
    switch (t) {
    case 0: return "Don't use (test)";
    case 1: return "PRN mask";
    case 2: case 3: case 4: case 5: return "Fast corrections";
    case 6: return "Integrity information";
    case 7: return "Fast correction degradation";
    case 9: return "GEO navigation message";
    case 10: return "Degradation parameters";
    case 12: return "SBAS network time / UTC offset";
    case 17: return "GEO almanacs";
    case 18: return "Ionospheric grid point masks";
    case 24: return "Mixed fast / long-term corrections";
    case 25: return "Long-term satellite corrections";
    case 26: return "Ionospheric delay corrections";
    case 27: return "SBAS service message";
    case 28: return "Clock-ephemeris covariance matrix";
    case 62: return "Internal test message";
    case 63: return "Null message";
    default: return "Other / reserved on L1";
    }
}

std::string maskBitName(int bit, int* prn) {
    char b[40];
    int p = 0;
    if (bit >= 1 && bit <= 37) { p = bit; snprintf(b, sizeof b, "GPS %d", bit); }
    else if (bit >= 38 && bit <= 61) { p = bit - 37; snprintf(b, sizeof b, "GLONASS %d", bit - 37); }
    else if (bit >= 120 && bit <= 138) { p = bit; snprintf(b, sizeof b, "SBAS %d", bit); }
    else if (bit >= 193 && bit <= 202) { p = bit; snprintf(b, sizeof b, "QZSS %d", bit); }
    else snprintf(b, sizeof b, "reserved %d", bit);
    if (prn) *prn = p;
    return b;
}

// DO-229 UDRE and GIVE variances (m^2)
static const double kUdreVar[14] = {0.052, 0.0924, 0.1444, 0.283, 0.4678, 0.8315, 1.2992, 1.8709, 2.5465, 3.326, 5.1968, 20.7870, 230.9661, 2078.695};
static const double kGiveVar[15] = {0.0084, 0.0333, 0.0749, 0.1331, 0.2079, 0.2994, 0.4075, 0.5322, 0.6735, 0.8315, 1.1974, 1.8709, 3.326, 20.787, 187.0826};
static const double kAi[16] = {0, 0.00005, 0.00009, 0.00012, 0.00015, 0.00020, 0.00030, 0.00045, 0.00060, 0.00090, 0.00150, 0.00210, 0.00270, 0.00330, 0.00460, 0.00580};

double udreVariance(int i) { return i >= 0 && i < 14 ? kUdreVar[i] : 0; }
double giveVariance(int i) { return i >= 0 && i < 15 ? kGiveVar[i] : 0; }
double degradationA(int i) { return i >= 0 && i < 16 ? kAi[i] : 0; }
const char* udreText(int i) {
    static char buf[4][24];
    static int k = 0;
    if (i == 14) return "not monitored";
    if (i == 15) return "do not use";
    char* b = buf[k++ & 3];
    snprintf(b, 24, "sigma %.2f m", std::sqrt(udreVariance(i)));
    return b;
}

// ------------------------------------------------------------------ the decoder
int Network::slotPrn(const GeoState& g, int slot) {
    if (!g.maskValid || slot < 0 || slot >= (int)g.slotBit.size()) return 0;
    int prn = 0;
    maskBitName(g.slotBit[(size_t)slot], &prn);
    return prn;
}
std::string Network::slotName(const GeoState& g, int slot) {
    if (!g.maskValid || slot < 0 || slot >= (int)g.slotBit.size()) { char b[24]; snprintf(b, sizeof b, "slot %d", slot + 1); return b; }
    return maskBitName(g.slotBit[(size_t)slot]);
}

Summary Network::summarise(const GeoState& g) {
    Summary s;
    s.prn = g.prn; s.messages = g.messages;
    for (int i = 0; i < kMaxSlots; i++) {
        const FastCorr& f = g.fast[(size_t)i];
        if (!f.valid) continue;
        s.nFast++;
        if (f.udrei <= 13) s.nUsable++;
        else if (f.udrei == 14) s.nNotMonitored++;
        else s.nDoNotUse++;
    }
    return s;
}

IonoSummary Network::ionoSummary(const GeoState& g, int band) {
    IonoSummary s;
    s.band = band;
    if (band < 0 || band >= kIonoBands) return s;
    const IonoBand& b = g.iono[(size_t)band];
    s.nIgp = b.nIgp;
    s.blocksSeen = b.blocksSeen;
    s.blocksNeeded = (b.nIgp + 14) / 15;
    double sum = 0;
    bool first = true;
    for (size_t i = 0; i < b.valid.size(); i++) {
        if (!b.valid[i]) continue;
        if (b.givei[i] == 15) { s.nNotMonitored++; continue; }
        s.nValue++;
        const float d = b.delayM[i];
        if (first) { s.minM = s.maxM = d; first = false; }
        s.minM = std::min(s.minM, d); s.maxM = std::max(s.maxM, d);
        sum += d;
        s.worstGivei = std::max(s.worstGivei, (int)b.givei[i]);
    }
    if (s.nValue) s.meanM = (float)(sum / s.nValue);
    return s;
}

void Network::clear() { geos_.clear(); log_.clear(); total_ = bad_ = 0; std::memset(perType_, 0, sizeof perType_); }
void Network::countBad(int prn) { geos_[prn].prn = prn; geos_[prn].badCrc++; bad_++; }

static void setFast(GeoState& g, int slot, double prc, int udrei, int iodf, int iodp, double t) {
    if (slot < 0 || slot >= kMaxSlots) return;
    FastCorr& f = g.fast[(size_t)slot];
    f.valid = true; f.prcM = prc; f.udrei = udrei; f.iodf = iodf; f.iodp = iodp; f.timeS = t;
}

// one long-term half message (106 bits from `half`, 0 based offset into the message)
static int decodeLongTerm(GeoState& g, const uint8_t* b, int half, double t, std::string& txt) {
    int added = 0;
    const int vel = (int)getBits(b, half, 1);
    auto sub0 = [&](int p, int iodp) {
        LongTerm l;
        l.slot = (int)getBits(b, p, 6);
        if (l.slot == 0) return;
        l.iode = (int)getBits(b, p + 6, 8);
        for (int i = 0; i < 3; i++) l.dpos[i] = getSigned(b, p + 14 + 9 * i, 9) * 0.125;
        l.daf0 = getSigned(b, p + 41, 10) * std::ldexp(1.0, -31);
        l.iodp = iodp; l.timeS = t;
        bool done = false;
        for (auto& e : g.longTerm) if (e.slot == l.slot) { e = l; done = true; }
        if (!done) g.longTerm.push_back(l);
        added++;
    };
    if (vel == 0) {
        const int iodp = (int)getBits(b, half + 103, 2);
        sub0(half + 1, iodp);
        sub0(half + 52, iodp);
    } else {
        const int p = half + 1;
        LongTerm l;
        l.slot = (int)getBits(b, p, 6);
        if (l.slot) {
            l.velocity = true;
            l.iode = (int)getBits(b, p + 6, 8);
            for (int i = 0; i < 3; i++) l.dpos[i] = getSigned(b, p + 14 + 11 * i, 11) * 0.125;
            l.daf0 = getSigned(b, p + 47, 11) * std::ldexp(1.0, -31);
            for (int i = 0; i < 3; i++) l.dvel[i] = getSigned(b, p + 58 + 8 * i, 8) * std::ldexp(1.0, -11);
            l.daf1 = getSigned(b, p + 82, 8) * std::ldexp(1.0, -39);
            l.t0S = getBits(b, p + 90, 13) * 16.0;
            l.iodp = (int)getBits(b, half + 104, 2);
            l.timeS = t;
            bool done = false;
            for (auto& e : g.longTerm) if (e.slot == l.slot) { e = l; done = true; }
            if (!done) g.longTerm.push_back(l);
            added++;
        }
    }
    g.longTermMessages += added ? 1 : 0;
    char s[48];
    snprintf(s, sizeof s, " long-term: %d satellite(s)", added);
    txt += s;
    return added;
}

std::string Network::add(int prn, const uint8_t* b, double t) {
    GeoState& g = geos_[prn];
    g.prn = prn;
    const int type = messageType(b);
    g.messages++;
    g.lastType = type;
    if (type < 64) g.typesSeen |= 1ull << type;
    if (g.firstS < 0) g.firstS = t;
    g.lastS = t;
    total_++;
    perType_[type & 63]++;
    char s[200];
    std::string txt;
    switch (type) {
    case 0: case 2: case 3: case 4: case 5: {
        const int iodf = (int)getBits(b, 14, 2), iodp = (int)getBits(b, 16, 2);
        const int blk = (type == 0 ? 0 : type - 2);
        int n = 0;
        for (int i = 0; i < 13; i++) {
            const int slot = 13 * blk + i;
            if (slot >= kMaxSlots) break;
            setFast(g, slot, getSigned(b, 18 + 12 * i, 12) * 0.125, (int)getBits(b, 174 + 4 * i, 4), iodf, iodp, t);
            n++;
        }
        g.fastMessages++;
        snprintf(s, sizeof s, "%s slots %d-%d, IODF %d, IODP %d", type == 0 ? "MT0 (test)" : "fast corrections", 13 * blk + 1, 13 * blk + n, iodf, iodp);
        txt = s;
        break;
    }
    case 1: {
        g.nMasked = 0;
        g.slotBit.clear();
        g.mask.fill(0);
        for (int i = 1; i <= 210; i++) {
            const bool on = getBits(b, 13 + i, 1) != 0;
            g.mask[(size_t)i] = on;
            if (on) { g.slotBit.push_back(i); g.nMasked++; }
        }
        g.iodp = (int)getBits(b, 224, 2);
        g.maskValid = true;
        snprintf(s, sizeof s, "PRN mask: %d satellites, IODP %d", g.nMasked, g.iodp);
        txt = s;
        break;
    }
    case 6: {
        for (int k = 0; k < 4; k++) g.iodf[k] = (int)getBits(b, 14 + 2 * k, 2);
        int n = 0;
        for (int i = 0; i < kMaxSlots; i++) {
            const int u = (int)getBits(b, 22 + 4 * i, 4);
            FastCorr& f = g.fast[(size_t)i];
            if (f.valid && f.iodf == g.iodf[i / 13]) { f.udrei = u; n++; }
            else if (!f.valid) { f.udrei = u; }
        }
        snprintf(s, sizeof s, "integrity: IODF %d %d %d %d, %d UDRE updated", g.iodf[0], g.iodf[1], g.iodf[2], g.iodf[3], n);
        txt = s;
        break;
    }
    case 7: {
        g.tLat = (int)getBits(b, 14, 4);
        g.iodpDeg = (int)getBits(b, 18, 2);
        for (int i = 0; i < kMaxSlots; i++) g.aiDeg[(size_t)i] = (uint8_t)getBits(b, 22 + 4 * i, 4);
        g.degValid = true;
        snprintf(s, sizeof s, "fast correction degradation: system latency %d s, IODP %d", g.tLat, g.iodpDeg);
        txt = s;
        break;
    }
    case 9: {
        GeoNav& n = g.nav;
        n.valid = true;
        n.iodn = (int)getBits(b, 14, 8);
        n.t0S = getBits(b, 22, 13) * 16.0;
        n.ura = (int)getBits(b, 35, 4);
        n.pos[0] = getSigned(b, 39, 30) * 0.08;
        n.pos[1] = getSigned(b, 69, 30) * 0.08;
        n.pos[2] = getSigned(b, 99, 25) * 0.4;
        n.vel[0] = getSigned(b, 124, 17) * 0.000625;
        n.vel[1] = getSigned(b, 141, 17) * 0.000625;
        n.vel[2] = getSigned(b, 158, 18) * 0.004;
        n.acc[0] = getSigned(b, 176, 10) * 0.0000125;
        n.acc[1] = getSigned(b, 186, 10) * 0.0000125;
        n.acc[2] = getSigned(b, 196, 10) * 0.0000625;
        n.af0 = getSigned(b, 206, 12) * std::ldexp(1.0, -31);
        n.af1 = getSigned(b, 218, 8) * std::ldexp(1.0, -40);
        const double r = std::sqrt(n.pos[0] * n.pos[0] + n.pos[1] * n.pos[1] + n.pos[2] * n.pos[2]);
        snprintf(s, sizeof s, "GEO navigation: radius %.0f km, URA index %d, t0 %.0f s", r / 1000.0, n.ura, n.t0S);
        txt = s;
        break;
    }
    case 10: {
        Degradation& d = g.deg;
        d.valid = true;
        d.brrc = (int)getBits(b, 14, 10); d.cltcLsb = (int)getBits(b, 24, 10); d.cltcV1 = (int)getBits(b, 34, 10); d.iltcV1 = (int)getBits(b, 44, 9);
        d.cltcV0 = (int)getBits(b, 53, 10); d.iltcV0 = (int)getBits(b, 63, 9); d.cgeoLsb = (int)getBits(b, 72, 10); d.cgeoV = (int)getBits(b, 82, 10);
        d.igeo = (int)getBits(b, 92, 9); d.cer = (int)getBits(b, 101, 6); d.cionoStep = (int)getBits(b, 107, 10); d.iiono = (int)getBits(b, 117, 9);
        d.cionoRamp = (int)getBits(b, 126, 10); d.rssUdre = (int)getBits(b, 136, 1); d.rssIono = (int)getBits(b, 137, 1);
        txt = "degradation parameters";
        break;
    }
    case 12: {
        TimeOffset& u = g.utc;
        u.valid = true;
        u.a1 = getSigned(b, 14, 24) * std::ldexp(1.0, -50);
        u.a0 = getSigned(b, 38, 32) * std::ldexp(1.0, -30);
        u.tUtc = (int)getBits(b, 70, 8) * 4096;
        u.wnT = (int)getBits(b, 78, 8);
        u.dtLS = getSigned(b, 86, 8);
        u.wnLSF = (int)getBits(b, 94, 8);
        u.dn = (int)getBits(b, 102, 8);
        u.dtLSF = getSigned(b, 110, 8);
        u.utcId = (int)getBits(b, 118, 3);
        u.tow = (int)getBits(b, 121, 20);
        u.wn = (int)getBits(b, 141, 10);
        u.glonass = getBits(b, 151, 1) != 0;
        snprintf(s, sizeof s, "time offset: GPS week %d, TOW %d s, leap seconds %d, UTC id %d", u.wn, u.tow, u.dtLS, u.utcId);
        txt = s;
        break;
    }
    case 17: {
        g.almanac.clear();
        for (int k = 0; k < 3; k++) {
            const int p = 14 + 67 * k;
            AlmEntry a;
            a.dataId = (int)getBits(b, p, 2);
            a.prn = (int)getBits(b, p + 2, 8);
            a.health = (int)getBits(b, p + 10, 8);
            a.pos[0] = getSigned(b, p + 18, 15) * 2600.0;
            a.pos[1] = getSigned(b, p + 33, 15) * 2600.0;
            a.pos[2] = getSigned(b, p + 48, 9) * 26000.0;
            a.vel[0] = getSigned(b, p + 57, 3) * 10.0;
            a.vel[1] = getSigned(b, p + 60, 3) * 10.0;
            a.vel[2] = getSigned(b, p + 63, 4) * 60.0;
            if (a.prn) g.almanac.push_back(a);
        }
        g.almT0 = getBits(b, 215, 11) * 64.0;
        snprintf(s, sizeof s, "GEO almanac: %zu satellite(s)", g.almanac.size());
        txt = s;
        break;
    }
    case 18: {
        const int band = (int)getBits(b, 18, 4);
        const int nBands = (int)getBits(b, 14, 4);
        const int iodi = (int)getBits(b, 22, 2);
        if (band >= kIonoBands) { snprintf(s, sizeof s, "IGP mask: band %d is not defined", band); txt = s; break; }
        IonoBand& ib = g.iono[(size_t)band];
        ib.mask.fill(0);
        ib.nIgp = 0;
        for (int i = 1; i <= kIgpPerBand; i++) { ib.mask[(size_t)i] = (uint8_t)getBits(b, 23 + i, 1); ib.nIgp += ib.mask[(size_t)i]; }
        ib.maskValid = true;
        if (ib.iodi != iodi || ib.valid.size() != (size_t)ib.nIgp) {
            ib.delayM.assign((size_t)ib.nIgp, 0.f); ib.valid.assign((size_t)ib.nIgp, 0); ib.givei.assign((size_t)ib.nIgp, 15); ib.blocksSeen = 0;
        }
        ib.iodi = iodi;
        g.ionoIodi = iodi;
        snprintf(s, sizeof s, "IGP mask: band %d of %d, %d grid points, IODI %d", band, nBands, ib.nIgp, iodi);
        txt = s;
        break;
    }
    case 24: {
        const int iodp = (int)getBits(b, 110, 2), blk = (int)getBits(b, 112, 2), iodf = (int)getBits(b, 114, 2);
        for (int i = 0; i < 6; i++) setFast(g, 13 * blk + i, getSigned(b, 14 + 12 * i, 12) * 0.125, (int)getBits(b, 86 + 4 * i, 4), iodf, iodp, t);
        g.fastMessages++;
        snprintf(s, sizeof s, "mixed: fast corrections slots %d-%d, IODF %d, IODP %d,", 13 * blk + 1, 13 * blk + 6, iodf, iodp);
        txt = s;
        decodeLongTerm(g, b, 120, t, txt);
        break;
    }
    case 25: {
        txt = "long-term corrections:";
        int n = decodeLongTerm(g, b, 14, t, txt);
        n += decodeLongTerm(g, b, 120, t, txt);
        (void)n;
        break;
    }
    case 26: {
        const int band = (int)getBits(b, 14, 4), blk = (int)getBits(b, 18, 4), iodi = (int)getBits(b, 217, 2);
        if (band >= kIonoBands || blk > 13) { snprintf(s, sizeof s, "ionospheric delays: band %d block %d out of range", band, blk); txt = s; break; }
        IonoBand& ib = g.iono[(size_t)band];
        int got = 0;
        if (ib.maskValid && ib.iodi == iodi) {
            for (int i = 0; i < 15; i++) {
                const size_t idx = (size_t)(blk * 15 + i);
                if (idx >= ib.valid.size()) break;
                const uint32_t d = getBits(b, 22 + 13 * i, 9);
                const int e = (int)getBits(b, 22 + 13 * i + 9, 4);
                ib.givei[idx] = (uint8_t)e;
                ib.delayM[idx] = d == 0x1FF ? 0.f : (float)(d * 0.125);
                ib.valid[idx] = 1;
                if (d == 0x1FF) ib.givei[idx] = 15;          // all ones: not monitored
                got++;
            }
            ib.blocksSeen |= 1 << blk;
        }
        snprintf(s, sizeof s, "ionospheric delays: band %d block %d, IODI %d%s", band, blk, iodi, ib.maskValid && ib.iodi == iodi ? "" : " (no matching mask yet)");
        txt = s;
        (void)got;
        break;
    }
    case 27: txt = "service message (text of the provider's service area)"; break;
    case 28: txt = "clock-ephemeris covariance matrix"; break;
    case 62: txt = "internal test message"; break;
    case 63: txt = "null message"; break;
    default: snprintf(s, sizeof s, "type %d (not decoded)", type); txt = s; break;
    }
    LogEntry e;
    e.timeS = t; e.prn = prn; e.type = type; e.text = txt;
    log_.push_back(e);
    if (log_.size() > 200) log_.erase(log_.begin(), log_.begin() + (long)(log_.size() - 200));
    return txt;
}

// ------------------------------------------------------------------ test signal content
namespace {
struct R {
    uint64_t s;
    explicit R(uint64_t seed) : s(seed * 6364136223846793005ull + 1442695040888963407ull) {}
    uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return (uint32_t)(s >> 33); }
};
// the data field starts at message bit 14: d[0] is message bit 14
void put(uint8_t* d, int msgBit, int len, uint32_t v) { putBits(d, msgBit - 14, len, v); }
void putS(uint8_t* d, int msgBit, int len, long v) { putBits(d, msgBit - 14, len, (uint32_t)(v & ((1ll << len) - 1))); }
}  // namespace

bool synthData(int prn, int64_t m, int type, uint8_t* d) {
    std::memset(d, 0, 212);
    R rng((uint64_t)prn * 7919u + (uint64_t)m);
    const int64_t period = m / 16;                    // the contents move once a rotation of the schedule
    switch (type) {
    case 1:
        for (int i = 1; i <= 14; i++) put(d, 13 + i, 1, 1);       // GPS 1..14
        put(d, 13 + 120 + (prn - 120), 1, 1);                       // the satellite itself as an SBAS PRN (mask bit 120..138)
        put(d, 224, 2, 1);
        return true;
    case 0: case 2: case 3: case 4: case 5: {
        const int blk = type - 2;
        put(d, 14, 2, 1); put(d, 16, 2, 1);
        for (int i = 0; i < 13; i++) {
            const int slot = 13 * blk + i;
            const long prc = (long)((slot * 23 + period * 3) % 120) - 60;       // +-7.5 m
            putS(d, 18 + 12 * i, 12, prc);
            put(d, 174 + 4 * i, 4, (uint32_t)(slot % 12));
        }
        return true;
    }
    case 6:
        for (int k = 0; k < 4; k++) put(d, 14 + 2 * k, 2, 1);
        for (int i = 0; i < kMaxSlots; i++) put(d, 22 + 4 * i, 4, (uint32_t)(i % 12));
        return true;
    case 7:
        put(d, 14, 4, 3); put(d, 18, 2, 1);
        for (int i = 0; i < kMaxSlots; i++) put(d, 22 + 4 * i, 4, (uint32_t)(i % 16));
        return true;
    case 9: {
        const double lon = (prn - 120) * 0.22;                                    // radians: three different positions on the arc
        const double R0 = 42164169.0;
        put(d, 14, 8, 17);
        put(d, 22, 13, (uint32_t)(m % 5400));
        put(d, 35, 4, 2);
        putS(d, 39, 30, std::lround(R0 * std::cos(lon) / 0.08));
        putS(d, 69, 30, std::lround(R0 * std::sin(lon) / 0.08));
        putS(d, 99, 25, 120);                                                      // 48 m above the equatorial plane
        putS(d, 206, 12, 40 - (long)(rng.next() % 5));
        putS(d, 218, 8, 3);
        return true;
    }
    case 10:
        put(d, 14, 10, 80); put(d, 24, 10, 20); put(d, 34, 10, 60); put(d, 44, 9, 90); put(d, 53, 10, 30); put(d, 63, 9, 60);
        put(d, 72, 10, 40); put(d, 82, 10, 20); put(d, 92, 9, 100); put(d, 101, 6, 10); put(d, 107, 10, 300); put(d, 117, 9, 200); put(d, 126, 10, 50);
        return true;
    case 12:
        putS(d, 14, 24, 7); putS(d, 38, 32, -1100);
        put(d, 70, 8, 5); put(d, 78, 8, 100); putS(d, 86, 8, 18); put(d, 94, 8, 100); put(d, 102, 8, 7); putS(d, 110, 8, 18);
        put(d, 118, 3, 1); put(d, 121, 20, (uint32_t)((m % 604800)));
        put(d, 141, 10, 400); put(d, 151, 1, 0);
        return true;
    case 17:
        for (int k = 0; k < 3; k++) {
            const int p = 14 + 67 * k;
            put(d, p, 2, 0); put(d, p + 2, 8, (uint32_t)(120 + k)); put(d, p + 10, 8, 0);
            putS(d, p + 18, 15, 16000 - 5000 * k); putS(d, p + 33, 15, (long)(3000 + 2000 * k)); putS(d, p + 48, 9, 1);
        }
        put(d, 215, 11, 900);
        return true;
    case 18:
        put(d, 14, 4, 2); put(d, 18, 4, 0); put(d, 22, 2, 0);
        for (int i = 1; i <= 75; i++) put(d, 23 + i, 1, (uint32_t)(i % 5 != 0));          // 60 grid points in band 0
        return true;
    case 24:
        put(d, 110, 2, 1); put(d, 112, 2, 0); put(d, 114, 2, 1);
        for (int i = 0; i < 6; i++) { putS(d, 14 + 12 * i, 12, (long)(i * 9 - 20 + period % 5)); put(d, 86 + 4 * i, 4, (uint32_t)(i % 8)); }
        return true;
    case 25:
        put(d, 14, 1, 0);
        put(d, 15, 6, 1); put(d, 21, 8, 33); putS(d, 29, 9, 12); putS(d, 38, 9, -7); putS(d, 47, 9, 3); putS(d, 56, 10, 20);
        put(d, 66, 6, 2); put(d, 72, 8, 33); putS(d, 80, 9, -4); putS(d, 89, 9, 9); putS(d, 98, 9, -2); putS(d, 107, 10, -15);
        put(d, 117, 2, 1);
        return true;
    case 26: {
        const int blk = (int)(period % 4);
        put(d, 14, 4, 0); put(d, 18, 4, (uint32_t)blk);
        for (int i = 0; i < 15; i++) { put(d, 22 + 13 * i, 9, (uint32_t)(24 + blk * 3 + i)); put(d, 22 + 13 * i + 9, 4, (uint32_t)(i % 9)); }
        put(d, 217, 2, 0);
        return true;
    }
    default: return false;
    }
}

}  // namespace sbas
}  // namespace dect2
