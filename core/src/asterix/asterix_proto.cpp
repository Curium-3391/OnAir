// ASTERIX categories 021 (edition 2.x UAP), 048 and 062: framing, data item lengths and the fields a track table needs. See asterix_proto.h.
#include "dect2/asterix/asterix_proto.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace dect2 {
namespace asterix {

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Rd {
    const uint8_t* p;
    const uint8_t* e;
    size_t left() const { return (size_t)(e - p); }
};

uint32_t be(const uint8_t* d, int n) { uint32_t v = 0; for (int i = 0; i < n; i++) v = (v << 8) | d[i]; return v; }
int32_t sbe(const uint8_t* d, int n) {
    uint32_t v = be(d, n);
    const int bits = 8 * n;
    if (bits < 32 && (v & (1u << (bits - 1)))) v |= ~0u << bits;
    return (int32_t)v;
}

// a compound subfield: > 0 fixed length, 0 extendable (first octet and extents, FX in bit 1), < 0 repetitive: a count octet and that many units of -len octets
struct Sub { int len; };
enum Kind { kNone, kFixed, kVar, kVar3, kRep, kCompound, kExplicit };
struct Item {
    const char* id;
    Kind kind;
    int len;                      // fixed length / repetition unit
    const Sub* subs;
    int nSubs;
};

// ---- compound items
const Sub kSub048_130[] = {{1}, {1}, {1}, {1}, {1}, {1}, {1}};
// 062/380 aircraft derived data
const Sub kSub062_380[] = {{3}, {6}, {2}, {2}, {2}, {2}, {2}, {0}, {-15}, {2}, {2}, {7}, {2}, {2}, {2}, {2}, {2}, {2}, {1}, {8}, {1}, {6}, {2}, {1}, {-8}, {2}, {2}, {2}};
// 062/290 system track update ages
const Sub kSub062_290[] = {{1}, {1}, {1}, {1}, {2}, {1}, {1}, {1}, {1}, {1}};
// 062/295 track data ages: 31 one octet subfields
const Sub kSub062_295[] = {{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1}};
// 062/390 flight plan related data
const Sub kSub062_390[] = {{2}, {7}, {4}, {1}, {4}, {1}, {4}, {4}, {3}, {2}, {2}, {-4}, {6}, {1}, {7}, {7}, {2}, {7}};
// 062/110 mode 5 data reports and extended mode 1 code
const Sub kSub062_110[] = {{1}, {4}, {6}, {2}, {2}, {1}, {1}};
// 062/500 estimated accuracies
const Sub kSub062_500[] = {{4}, {2}, {4}, {1}, {1}, {2}, {2}, {1}};
// 062/340 measured information
const Sub kSub062_340[] = {{2}, {4}, {2}, {4}, {2}, {1}};
// 021/110 trajectory intent: TIS (extendable) and TID (repetitive, 15 octets)
const Sub kSub021_110[] = {{0}, {-15}};
// 021/220 met information: wind speed, wind direction, temperature, turbulence
const Sub kSub021_220[] = {{2}, {2}, {2}, {1}};
// 021/295 data ages: 1 octet each (the 21 subfields of edition 2.x are all one octet)
const Sub kSub021_295[] = {{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1},{1}};

#define S(a) a, (int)(sizeof(a) / sizeof(a[0]))
#define NO nullptr, 0

// FRN -> item, index 0 is FRN 1
const Item kU048[] = {
    {"010", kFixed, 2, NO}, {"140", kFixed, 3, NO}, {"020", kVar, 1, NO}, {"040", kFixed, 4, NO}, {"070", kFixed, 2, NO}, {"090", kFixed, 2, NO}, {"130", kCompound, 0, S(kSub048_130)},
    {"220", kFixed, 3, NO}, {"240", kFixed, 6, NO}, {"250", kRep, 8, NO}, {"161", kFixed, 2, NO}, {"042", kFixed, 4, NO}, {"200", kFixed, 4, NO}, {"170", kVar, 1, NO},
    {"210", kFixed, 4, NO}, {"030", kVar, 1, NO}, {"080", kFixed, 2, NO}, {"100", kFixed, 4, NO}, {"110", kFixed, 2, NO}, {"120", kNone, 0, NO}, {"230", kFixed, 2, NO},
    {"260", kFixed, 7, NO}, {"055", kFixed, 1, NO}, {"050", kFixed, 2, NO}, {"065", kFixed, 1, NO}, {"060", kFixed, 2, NO}, {"SP", kExplicit, 0, NO}, {"RE", kExplicit, 0, NO},
};
const Item kU021[] = {
    {"010", kFixed, 2, NO}, {"040", kVar, 1, NO}, {"161", kFixed, 2, NO}, {"015", kFixed, 1, NO}, {"071", kFixed, 3, NO}, {"130", kFixed, 6, NO}, {"131", kFixed, 8, NO},
    {"072", kFixed, 3, NO}, {"150", kFixed, 2, NO}, {"151", kFixed, 2, NO}, {"080", kFixed, 3, NO}, {"073", kFixed, 3, NO}, {"074", kFixed, 4, NO}, {"075", kFixed, 3, NO},
    {"076", kFixed, 4, NO}, {"140", kFixed, 2, NO}, {"090", kVar, 1, NO}, {"210", kFixed, 1, NO}, {"070", kFixed, 2, NO}, {"230", kFixed, 2, NO}, {"145", kFixed, 2, NO},
    {"152", kFixed, 2, NO}, {"200", kFixed, 1, NO}, {"155", kFixed, 2, NO}, {"157", kFixed, 2, NO}, {"160", kFixed, 4, NO}, {"165", kFixed, 2, NO}, {"077", kFixed, 3, NO},
    {"170", kFixed, 6, NO}, {"020", kFixed, 1, NO}, {"220", kCompound, 0, S(kSub021_220)}, {"146", kFixed, 2, NO}, {"148", kFixed, 2, NO}, {"110", kCompound, 0, S(kSub021_110)},
    {"016", kFixed, 1, NO}, {"008", kFixed, 1, NO}, {"271", kVar, 1, NO}, {"132", kFixed, 1, NO}, {"250", kRep, 8, NO}, {"260", kFixed, 7, NO}, {"400", kFixed, 1, NO},
    {"295", kCompound, 0, S(kSub021_295)}, {"", kNone, 0, NO}, {"", kNone, 0, NO}, {"", kNone, 0, NO}, {"", kNone, 0, NO}, {"", kNone, 0, NO}, {"RE", kExplicit, 0, NO}, {"SP", kExplicit, 0, NO},
};
const Item kU062[] = {
    {"010", kFixed, 2, NO}, {"", kNone, 0, NO}, {"015", kFixed, 1, NO}, {"070", kFixed, 3, NO}, {"105", kFixed, 8, NO}, {"100", kFixed, 6, NO}, {"185", kFixed, 4, NO},
    {"210", kFixed, 2, NO}, {"060", kFixed, 2, NO}, {"245", kFixed, 7, NO}, {"380", kCompound, 0, S(kSub062_380)}, {"040", kFixed, 2, NO}, {"080", kVar, 1, NO},
    {"290", kCompound, 0, S(kSub062_290)}, {"200", kFixed, 1, NO}, {"295", kCompound, 0, S(kSub062_295)}, {"136", kFixed, 2, NO}, {"130", kFixed, 2, NO}, {"135", kFixed, 2, NO},
    {"220", kFixed, 2, NO}, {"390", kCompound, 0, S(kSub062_390)}, {"270", kVar, 1, NO}, {"300", kFixed, 1, NO}, {"110", kCompound, 0, S(kSub062_110)}, {"120", kFixed, 2, NO},
    {"510", kVar3, 3, NO}, {"500", kCompound, 0, S(kSub062_500)}, {"340", kCompound, 0, S(kSub062_340)}, {"", kNone, 0, NO}, {"", kNone, 0, NO}, {"", kNone, 0, NO},
    {"", kNone, 0, NO}, {"", kNone, 0, NO}, {"RE", kExplicit, 0, NO}, {"SP", kExplicit, 0, NO},
};
#undef S
#undef NO

// One compound item: the primary subfield (FX chained), then the present subfields in order. got[i] = pointer to subfield i (or null).
bool readCompound(Rd& r, const Item& it, const uint8_t** got, int* gotLen, const uint8_t** start, size_t* total) {
    const uint8_t* s = r.p;
    // primary
    std::vector<uint8_t> prim;
    for (;;) {
        if (r.left() < 1) return false;
        const uint8_t b = *r.p++;
        prim.push_back(b);
        if (!(b & 1)) break;
        if (prim.size() > 8) return false;
    }
    for (int i = 0; i < it.nSubs; i++) { got[i] = nullptr; gotLen[i] = 0; }
    for (size_t j = 0; j < prim.size(); j++) {
        for (int k = 0; k < 7; k++) {
            if (!(prim[j] & (0x80 >> k))) continue;
            const int idx = (int)j * 7 + k;
            if (idx >= it.nSubs) return false;          // a subfield this decoder does not know the length of
            const int code = it.subs[idx].len;
            const uint8_t* b = r.p;
            size_t n = 0;
            if (code > 0) n = (size_t)code;
            else if (code == 0) {
                size_t m = 0;
                for (;;) { if (r.left() < m + 1) return false; if (!(b[m] & 1)) break; m++; }
                n = m + 1;
            } else {
                if (r.left() < 1) return false;
                n = 1 + (size_t)b[0] * (size_t)(-code);
            }
            if (r.left() < n) return false;
            got[idx] = b; gotLen[idx] = (int)n;
            r.p += n;
        }
    }
    *start = s;
    *total = (size_t)(r.p - s);
    return true;
}

bool readItem(Rd& r, const Item& it, const uint8_t** start, size_t* len) {
    *start = r.p;
    switch (it.kind) {
    case kFixed:
        if (r.left() < (size_t)it.len) return false;
        r.p += it.len; *len = (size_t)it.len; return true;
    case kVar: {
        size_t n = 0;
        for (;;) { if (r.left() < n + 1) return false; if (!(r.p[n] & 1)) break; n++; if (n > 16) return false; }
        n++; r.p += n; *len = n; return true;
    }
    case kVar3: {
        size_t n = 0;
        for (;;) { if (r.left() < n + 3) return false; if (!(r.p[n + 2] & 1)) break; n += 3; if (n > 48) return false; }
        n += 3; r.p += n; *len = n; return true;
    }
    case kRep: {
        if (r.left() < 1) return false;
        const size_t n = 1 + (size_t)r.p[0] * (size_t)it.len;
        if (r.left() < n) return false;
        r.p += n; *len = n; return true;
    }
    case kExplicit: {
        if (r.left() < 1 || r.p[0] < 1 || r.left() < r.p[0]) return false;
        const size_t n = r.p[0];
        r.p += n; *len = n; return true;
    }
    case kCompound: {
        const uint8_t* got[40]; int gl[40];
        const uint8_t* st; size_t tot;
        if (it.nSubs > 40) return false;
        if (!readCompound(r, it, got, gl, &st, &tot)) return false;
        *len = tot; return true;
    }
    default: return false;
    }
}

std::string name6(const uint8_t* d) { return icao6(d, 8); }
double wrap360(double x) { x = std::fmod(x, 360.0); return x < 0 ? x + 360.0 : x; }
int octalDigits(uint32_t v12) { return (int)(((v12 >> 9) & 7) * 1000 + ((v12 >> 6) & 7) * 100 + ((v12 >> 3) & 7) * 10 + (v12 & 7)); }

void sourceId(Target& t, const uint8_t* d) { t.sac = d[0]; t.sic = d[1]; }

// ---- per item handlers ---------------------------------------------------------------------------------------------------------------
void item048(Target& t, const std::string& id, const uint8_t* d, size_t n) {
    if (id == "010") sourceId(t, d);
    else if (id == "140") { t.hasTime = true; t.todS = be(d, 3) / 128.0; }
    else if (id == "020") {
        static const char* typ[8] = {"no detection", "PSR", "SSR", "SSR + PSR", "Mode S all-call", "Mode S roll-call", "Mode S all-call + PSR", "Mode S roll-call + PSR"};
        t.kind = typ[(d[0] >> 5) & 7];
        if (d[0] & 0x10) t.kind += " (simulated)";
    }
    else if (id == "040") { t.hasPolar = true; t.rhoNm = be(d, 2) / 256.0; t.thetaDeg = be(d + 2, 2) * 360.0 / 65536.0; }
    else if (id == "070") { t.squawkInvalid = (d[0] & 0xC0) != 0; t.squawk = octalDigits(be(d, 2) & 0xFFF); }
    else if (id == "090") { t.hasLevel = true; t.flightLevel = sbe(d, 2) & 0x3FFF; if (t.flightLevel >= 8192) t.flightLevel -= 16384; t.flightLevel *= 0.25; }
    else if (id == "220") t.address = be(d, 3);
    else if (id == "240") t.callsign = name6(d);
    else if (id == "161") t.trackNo = (int)(be(d, 2) & 0xFFF);
    else if (id == "042") { t.hasXy = true; t.xM = sbe(d, 2) / 128.0 * 1852.0; t.yM = sbe(d + 2, 2) / 128.0 * 1852.0; }
    else if (id == "200") { t.hasGs = true; t.gsKt = be(d, 2) * 3600.0 / 16384.0; t.hasTrack = true; t.trackDeg = be(d + 2, 2) * 360.0 / 65536.0; }
    else if (id == "110" && !t.hasAltFt) { int32_t v = sbe(d, 2) & 0x3FFF; if (v >= 8192) v -= 16384; t.hasAltFt = true; t.altFt = v * 25.0; }
    (void)n;
}

void item021(Target& t, const std::string& id, const uint8_t* d, size_t n) {
    if (id == "010") sourceId(t, d);
    else if (id == "040") {
        static const char* atp[8] = {"ADS-B (24 bit address)", "ADS-B (duplicate address)", "ADS-B (surface vehicle)", "ADS-B (anonymous)", "ADS-B", "ADS-B", "ADS-B", "ADS-B"};
        t.kind = atp[(d[0] >> 5) & 7];
        if (n >= 2 && (d[1] & 0x80)) t.kind += ", differential correction";
        if (n >= 2 && (d[1] & 0x40)) t.onGround = true;
    }
    else if (id == "161") t.trackNo = (int)(be(d, 2) & 0xFFF);
    else if (id == "130") { t.hasLatLon = true; t.lat = sbe(d, 3) * 180.0 / 8388608.0; t.lon = sbe(d + 3, 3) * 180.0 / 8388608.0; }
    else if (id == "131") { t.hasLatLon = true; t.lat = sbe(d, 4) * 180.0 / 1073741824.0; t.lon = sbe(d + 4, 4) * 180.0 / 1073741824.0; }
    else if (id == "080") t.address = be(d, 3);
    else if (id == "073" || id == "071") { if (!t.hasTime) { t.hasTime = true; t.todS = be(d, 3) / 128.0; } }
    else if (id == "140") { t.hasAltFt = true; t.altFt = sbe(d, 2) * 6.25; }
    else if (id == "145") { t.hasLevel = true; t.flightLevel = sbe(d, 2) * 0.25; }
    else if (id == "151") { t.hasTas = true; t.tasKt = be(d, 2) & 0x7FFF; }
    else if (id == "155") { t.hasVrate = true; int32_t v = (int32_t)(be(d, 2) & 0x7FFF); if (v >= 0x4000) v -= 0x8000; t.vrateFpm = v * 6.25; }
    else if (id == "157" && !t.hasVrate) { t.hasVrate = true; int32_t v = (int32_t)(be(d, 2) & 0x7FFF); if (v >= 0x4000) v -= 0x8000; t.vrateFpm = v * 6.25; }
    else if (id == "160") { const uint32_t gs = (be(d, 2) & 0x7FFF); t.hasGs = true; t.gsKt = gs * 3600.0 / 16384.0; t.hasTrack = true; t.trackDeg = be(d + 2, 2) * 360.0 / 65536.0; }
    else if (id == "170") t.callsign = name6(d);
    else if (id == "070") t.squawk = octalDigits(be(d, 2) & 0xFFF);
    else if (id == "152" && !t.hasTrack) { /* magnetic heading is not the track: left out */ }
}

void item062(Target& t, const std::string& id, const uint8_t* d, size_t n, const Item& it) {
    if (id == "010") sourceId(t, d);
    else if (id == "070") { t.hasTime = true; t.todS = be(d, 3) / 128.0; }
    else if (id == "105") { t.hasLatLon = true; t.lat = sbe(d, 4) * 180.0 / 33554432.0; t.lon = sbe(d + 4, 4) * 180.0 / 33554432.0; }
    else if (id == "100") { t.hasXy = true; t.xM = sbe(d, 3) * 0.5; t.yM = sbe(d + 3, 3) * 0.5; }
    else if (id == "185") {
        const double vx = sbe(d, 2) * 0.25, vy = sbe(d + 2, 2) * 0.25;       // m/s
        t.hasGs = true; t.gsKt = std::hypot(vx, vy) / 0.514444;
        t.hasTrack = true; t.trackDeg = wrap360(std::atan2(vx, vy) * 180.0 / kPi);
    }
    else if (id == "060") { t.squawkInvalid = (d[0] & 0xC0) != 0; t.squawk = octalDigits(be(d, 2) & 0xFFF); }
    else if (id == "245") t.callsign = icao6(d + 1, 8);
    else if (id == "040") t.trackNo = (int)be(d, 2);
    else if (id == "136") { t.hasLevel = true; t.flightLevel = sbe(d, 2) * 25.0 / 100.0; }
    else if (id == "130") { t.hasAltFt = true; t.altFt = sbe(d, 2) * 6.25; }
    else if (id == "135" && !t.hasAltFt) { int32_t v = (int32_t)(be(d, 2) & 0x7FFF); if (v >= 0x4000) v -= 0x8000; t.hasAltFt = true; t.altFt = v * 25.0; }
    else if (id == "220") { t.hasVrate = true; t.vrateFpm = sbe(d, 2) * 6.25; }
    else if (id == "080") { t.kind = (d[0] & 0x80) ? "track (monosensor)" : "track"; if (d[0] & 0x40) t.kind += ", SPI"; }
    else if (id == "380") {
        const uint8_t* got[40]; int gl[40];
        Rd r{d, d + n};
        const uint8_t* st; size_t tot;
        if (!readCompound(r, it, got, gl, &st, &tot)) return;
        if (got[0]) t.address = be(got[0], 3);
        if (got[1]) t.callsign = icao6(got[1], 8);
        if (got[4]) { t.hasTas = true; t.tasKt = be(got[4], 2); }
        if (got[17]) { /* ground speed in 2^-14 NM/s: the track data already gave it */ if (!t.hasGs) { t.hasGs = true; t.gsKt = sbe(got[17], 2) * 3600.0 / 16384.0; } }
    }
    else if (id == "390") {
        const uint8_t* got[40]; int gl[40];
        Rd r{d, d + n};
        const uint8_t* st; size_t tot;
        if (!readCompound(r, it, got, gl, &st, &tot)) return;
        auto ascii = [](const uint8_t* p, int k) { std::string s; for (int i = 0; i < k; i++) if (p[i] > 32 && p[i] < 127) s += (char)p[i]; return s; };
        if (got[1] && t.callsign.empty()) t.callsign = ascii(got[1], 7);
        if (got[6]) t.dep = ascii(got[6], 4);
        if (got[7]) t.dst = ascii(got[7], 4);
    }
}

// one record of a category: FSPEC, then the items in the order of the UAP
bool decodeRecord(int cat, Rd& r, Target& t) {
    const Item* tab; int nTab;
    if (cat == 48) { tab = kU048; nTab = (int)(sizeof kU048 / sizeof *kU048); }
    else if (cat == 21) { tab = kU021; nTab = (int)(sizeof kU021 / sizeof *kU021); }
    else { tab = kU062; nTab = (int)(sizeof kU062 / sizeof *kU062); }
    const uint8_t* rec = r.p;
    // FSPEC
    std::vector<int> frns;
    int frnBase = 0;
    for (;;) {
        if (r.left() < 1) return false;
        const uint8_t b = *r.p++;
        for (int k = 0; k < 7; k++) if (b & (0x80 >> k)) frns.push_back(frnBase + k + 1);
        frnBase += 7;
        if (!(b & 1)) break;
        if (frnBase > 7 * 8) return false;
    }
    t.cat = cat;
    for (int frn : frns) {
        if (frn > nTab) { t.partial = true; break; }
        const Item& it = tab[frn - 1];
        if (it.kind == kNone) { t.partial = true; break; }               // unknown length: leave the rest of the record
        const uint8_t* d; size_t n;
        if (!readItem(r, it, &d, &n)) { t.partial = true; break; }
        t.items += t.items.empty() ? "" : " ";
        t.items += it.id;
        if (cat == 48) item048(t, it.id, d, n);
        else if (cat == 21) item021(t, it.id, d, n);
        else item062(t, it.id, d, n, it);
    }
    // a record that stopped early: its end is the end of the data block, so the caller must skip to it
    t.raw.assign(rec, r.p);
    return true;
}

}  // namespace

std::string icao6(const uint8_t* d, size_t nChars) {
    std::string s;
    for (size_t i = 0; i < nChars; i++) {
        const size_t bit = i * 6;
        const uint32_t v = ((uint32_t)d[bit / 8] << 8 | (bit / 8 + 1 < (nChars * 6 + 7) / 8 ? d[bit / 8 + 1] : 0)) >> (16 - 6 - (bit % 8)) & 0x3F;
        char c;
        if (v >= 1 && v <= 26) c = (char)('A' + v - 1);
        else if (v >= 48 && v <= 57) c = (char)('0' + v - 48);
        else if (v == 32) c = ' ';
        else c = '?';
        s += c;
    }
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

const char* categoryName(int cat) {
    switch (cat) {
    case 21: return "CAT 021 ADS-B target reports";
    case 48: return "CAT 048 monoradar target reports";
    case 62: return "CAT 062 system track data";
    default: return "other category";
    }
}
std::string squawkText(int s) { char b[8]; if (s < 0) return "----"; snprintf(b, sizeof b, "%04d", s); return b; }
std::string addressText(int64_t a) { char b[12]; if (a < 0) return ""; snprintf(b, sizeof b, "%06X", (unsigned)a); return b; }

size_t decodeBlocks(const uint8_t* d, size_t n, std::vector<Target>& out, BlockStats* st) {
    size_t pos = 0;
    while (n - pos >= 3) {
        const int cat = d[pos];
        const size_t len = ((size_t)d[pos + 1] << 8) | d[pos + 2];
        if (len < 3 || len > n - pos) { if (st) st->badBlocks++; return pos; }
        if (st) { st->blocks++; st->perCat[cat]++; }
        if (cat == 21 || cat == 48 || cat == 62) {
            Rd r{d + pos + 3, d + pos + len};
            while (r.left() > 0) {
                Target t;
                const uint8_t* start = r.p;
                if (!decodeRecord(cat, r, t)) { if (st) st->badBlocks++; break; }
                if (t.partial) {
                    // the record's own end is not known: it is the rest of the block when it is the only one
                    t.raw.assign(start, d + pos + len);
                    if (st) st->partialRecords++;
                    r.p = r.e;
                }
                if (st) st->records++;
                out.push_back(std::move(t));
            }
        } else if (st) st->otherCategory++;
        pos += len;
    }
    return pos;
}

// ------------------------------------------------------------------ encoding
namespace {
void put(std::vector<uint8_t>& v, uint32_t x, int n) { for (int i = n - 1; i >= 0; i--) v.push_back((uint8_t)(x >> (8 * i))); }
void put6(std::vector<uint8_t>& v, const std::string& s, int nChars) {
    uint64_t acc = 0;
    for (int i = 0; i < nChars; i++) {
        uint32_t c = 32;
        if (i < (int)s.size()) {
            const char ch = s[(size_t)i];
            if (ch >= 'A' && ch <= 'Z') c = (uint32_t)(ch - 'A' + 1);
            else if (ch >= '0' && ch <= '9') c = (uint32_t)(ch - '0' + 48);
            else c = 32;
        }
        acc = (acc << 6) | c;
    }
    const int bytes = (nChars * 6 + 7) / 8;
    for (int i = bytes - 1; i >= 0; i--) v.push_back((uint8_t)(acc >> (8 * i)));
}
uint32_t octalToField(int sq) { return (uint32_t)(((sq / 1000) % 8) << 9 | ((sq / 100) % 10 % 8) << 6 | ((sq / 10) % 10 % 8) << 3 | (sq % 10 % 8)); }
// FSPEC from a set of FRNs (1 based)
std::vector<uint8_t> fspec(const std::vector<int>& frns) {
    int top = 0;
    for (int f : frns) top = std::max(top, f);
    const int bytes = (top + 6) / 7;
    std::vector<uint8_t> v((size_t)std::max(bytes, 1), 0);
    for (int f : frns) v[(size_t)((f - 1) / 7)] |= (uint8_t)(0x80 >> ((f - 1) % 7));
    for (size_t i = 0; i + 1 < v.size(); i++) v[i] |= 1;
    return v;
}
}  // namespace

std::vector<uint8_t> encodeRecord(const Target& t) {
    std::vector<int> frn;
    std::vector<uint8_t> body;
    auto add = [&](int f) { frn.push_back(f); };
    if (t.cat == 48) {
        add(1); put(body, (uint32_t)(t.sac & 255) << 8 | (uint32_t)(t.sic & 255), 2);
        if (t.hasTime) { add(2); put(body, (uint32_t)std::lround(t.todS * 128.0), 3); }
        { add(3); uint8_t typ = 3; if (t.kind.rfind("Mode S roll-call + PSR", 0) == 0) typ = 7; else if (t.kind == "SSR") typ = 2; else if (t.kind == "PSR") typ = 1; else if (t.kind.rfind("Mode S roll-call", 0) == 0) typ = 5; body.push_back((uint8_t)(typ << 5)); }
        if (t.hasPolar) { add(4); put(body, (uint32_t)std::lround(t.rhoNm * 256.0) & 0xFFFF, 2); put(body, (uint32_t)std::lround(t.thetaDeg * 65536.0 / 360.0) & 0xFFFF, 2); }
        if (t.squawk >= 0) { add(5); put(body, octalToField(t.squawk), 2); }
        if (t.hasLevel) { add(6); put(body, (uint32_t)((long)std::lround(t.flightLevel * 4.0) & 0x3FFF), 2); }
        if (t.address >= 0) { add(8); put(body, (uint32_t)t.address, 3); }
        if (!t.callsign.empty()) { add(9); put6(body, t.callsign, 8); }
        if (t.trackNo >= 0) { add(11); put(body, (uint32_t)t.trackNo & 0xFFF, 2); }
        if (t.hasXy) { add(12); put(body, (uint32_t)(long)std::lround(t.xM / 1852.0 * 128.0) & 0xFFFF, 2); put(body, (uint32_t)(long)std::lround(t.yM / 1852.0 * 128.0) & 0xFFFF, 2); }
        if (t.hasGs && t.hasTrack) { add(13); put(body, (uint32_t)std::lround(t.gsKt / 3600.0 * 16384.0) & 0xFFFF, 2); put(body, (uint32_t)std::lround(t.trackDeg * 65536.0 / 360.0) & 0xFFFF, 2); }
    } else if (t.cat == 21) {
        add(1); put(body, (uint32_t)(t.sac & 255) << 8 | (uint32_t)(t.sic & 255), 2);
        add(2); body.push_back(0);
        if (t.trackNo >= 0) { add(3); put(body, (uint32_t)t.trackNo & 0xFFF, 2); }
        if (t.hasLatLon) {
            add(7);
            put(body, (uint32_t)(long)std::lround(t.lat * 1073741824.0 / 180.0), 4);
            put(body, (uint32_t)(long)std::lround(t.lon * 1073741824.0 / 180.0), 4);
        }
        if (t.address >= 0) { add(11); put(body, (uint32_t)t.address, 3); }
        if (t.hasTime) { add(12); put(body, (uint32_t)std::lround(t.todS * 128.0), 3); }
        if (t.hasAltFt) { add(16); put(body, (uint32_t)(long)std::lround(t.altFt / 6.25) & 0xFFFF, 2); }
        if (t.squawk >= 0) { add(19); put(body, octalToField(t.squawk), 2); }
        if (t.hasLevel) { add(21); put(body, (uint32_t)(long)std::lround(t.flightLevel * 4.0) & 0xFFFF, 2); }
        if (t.hasVrate) { add(24); put(body, (uint32_t)(long)std::lround(t.vrateFpm / 6.25) & 0x7FFF, 2); }
        if (t.hasGs && t.hasTrack) { add(26); put(body, (uint32_t)std::lround(t.gsKt / 3600.0 * 16384.0) & 0x7FFF, 2); put(body, (uint32_t)std::lround(t.trackDeg * 65536.0 / 360.0) & 0xFFFF, 2); }
        if (!t.callsign.empty()) { add(29); put6(body, t.callsign, 8); }
    } else {
        // 062: the items are written in FRN order; build them in a table first
        struct F { int frn; std::vector<uint8_t> b; };
        std::vector<F> fs;
        { F f{1, {}}; put(f.b, (uint32_t)(t.sac & 255) << 8 | (uint32_t)(t.sic & 255), 2); fs.push_back(f); }
        if (t.hasTime) { F f{4, {}}; put(f.b, (uint32_t)std::lround(t.todS * 128.0), 3); fs.push_back(f); }
        if (t.hasLatLon) { F f{5, {}}; put(f.b, (uint32_t)(long)std::lround(t.lat * 33554432.0 / 180.0), 4); put(f.b, (uint32_t)(long)std::lround(t.lon * 33554432.0 / 180.0), 4); fs.push_back(f); }
        if (t.hasXy) { F f{6, {}}; put(f.b, (uint32_t)(long)std::lround(t.xM / 0.5) & 0xFFFFFF, 3); put(f.b, (uint32_t)(long)std::lround(t.yM / 0.5) & 0xFFFFFF, 3); fs.push_back(f); }
        if (t.hasGs && t.hasTrack) {
            F f{7, {}};
            const double v = t.gsKt * 0.514444, a = t.trackDeg * kPi / 180.0;
            put(f.b, (uint32_t)(long)std::lround(v * std::sin(a) / 0.25) & 0xFFFF, 2); put(f.b, (uint32_t)(long)std::lround(v * std::cos(a) / 0.25) & 0xFFFF, 2);
            fs.push_back(f);
        }
        if (t.squawk >= 0) { F f{9, {}}; put(f.b, octalToField(t.squawk), 2); fs.push_back(f); }
        if (!t.callsign.empty()) { F f{10, {}}; f.b.push_back(0); put6(f.b, t.callsign, 8); fs.push_back(f); }
        if (t.address >= 0) {
            // 380 with ADR (subfield 1) only
            F f{11, {}}; f.b.push_back(0x80); put(f.b, (uint32_t)t.address, 3); fs.push_back(f);
        }
        if (t.trackNo >= 0) { F f{12, {}}; put(f.b, (uint32_t)t.trackNo & 0xFFFF, 2); fs.push_back(f); }
        { F f{13, {}}; f.b.push_back(0); fs.push_back(f); }
        if (t.hasLevel) { F f{17, {}}; put(f.b, (uint32_t)(long)std::lround(t.flightLevel * 4.0) & 0xFFFF, 2); fs.push_back(f); }
        if (t.hasAltFt) { F f{18, {}}; put(f.b, (uint32_t)(long)std::lround(t.altFt / 6.25) & 0xFFFF, 2); fs.push_back(f); }
        if (t.hasVrate) { F f{20, {}}; put(f.b, (uint32_t)(long)std::lround(t.vrateFpm / 6.25) & 0xFFFF, 2); fs.push_back(f); }
        for (const F& f : fs) { add(f.frn); body.insert(body.end(), f.b.begin(), f.b.end()); }
    }
    std::vector<uint8_t> rec = fspec(frn);
    rec.insert(rec.end(), body.begin(), body.end());
    return rec;
}

std::vector<uint8_t> wrapBlock(int cat, const std::vector<std::vector<uint8_t>>& records) {
    size_t n = 3;
    for (auto& r : records) n += r.size();
    std::vector<uint8_t> b;
    b.push_back((uint8_t)cat);
    b.push_back((uint8_t)(n >> 8)); b.push_back((uint8_t)n);
    for (auto& r : records) b.insert(b.end(), r.begin(), r.end());
    return b;
}

}  // namespace asterix
}  // namespace dect2
