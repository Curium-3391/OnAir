// ISDB-S: TMCC bits, the coding chain of a slot, and the whole receiver against the generator (clean, with noise, with a carrier offset and a
// symbol clock offset, hierarchical, a mid-stream change of the TMCC). Synthetic signals only: nothing here has met a real BS transponder.
#include "dect2/isdbs/isdbs.h"
#include "dect2/isdbs/isdbs_rx.h"
#include "../../core/src/isdbs/isdbs_slot.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <random>

using namespace dect2;
using namespace dect2::isdbs;

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { g_fail++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// packet n of stream `tag`: PID 0x100 + tag, counter in bytes 4..7, bytes after it depend on n
static void testPacket(int tag, uint64_t n, uint8_t* p) {
    uint32_t st = (uint32_t)(n * 2654435761u) ^ (0x9e3779b9u + (uint32_t)tag);
    p[0] = 0x47; p[1] = (uint8_t)(0x01 + (tag >> 8)); p[2] = (uint8_t)tag; p[3] = (uint8_t)(0x10 | (n & 15));
    p[4] = (uint8_t)(n >> 24); p[5] = (uint8_t)(n >> 16); p[6] = (uint8_t)(n >> 8); p[7] = (uint8_t)n;
    for (int i = 8; i < 188; i++) { st = st * 1664525u + 1013904223u; p[i] = (uint8_t)(st >> 24); }
}
static std::function<void(uint8_t*)> source(int tag) {
    auto n = std::make_shared<uint64_t>(0);
    return [tag, n](uint8_t* p) { testPacket(tag, (*n)++, p); };
}

static void testTmcc() {
    std::vector<StreamCfg> s(3);
    s[0].mod = kTc8psk23; s[0].slots = 16; s[0].tsId = 0x4010;
    s[1].mod = kQpsk34;   s[1].slots = 12; s[1].tsId = 0x4011;
    s[2].mod = kBpsk12;   s[2].slots = 4;  s[2].tsId = 0x4012;
    Tmcc t, u;
    std::string err;
    CHECK(tmccFromStreams(s, 5, t, &err), "tmccFromStreams: %s", err.c_str());
    CHECK(t.mode[3] == kUnused && t.count[3] == 16, "the filler is the fourth group");
    CHECK(t.slotMod(0) == kTc8psk23 && t.slotMod(15) == kTc8psk23 && t.slotMod(16) == kQpsk34 && t.slotMod(28) == kBpsk12 && t.slotMod(31) == kBpsk12 && t.slotMod(32) == kUnused, "slot modulation");
    const auto bits = tmccPack(t);
    CHECK(tmccUnpack(bits.data(), u) && u == t, "TMCC pack / unpack");
    uint8_t c64[64];
    tmccSliceEncode(&bits[48], c64);
    CHECK(tmccSliceCheck(c64), "slice check");
    c64[17] ^= 1;
    CHECK(!tmccSliceCheck(c64), "slice check catches a flipped bit");
    std::vector<StreamCfg> bad(2);
    bad[0].slots = 30; bad[1].slots = 30;
    CHECK(!tmccFromStreams(bad, 0, t), "more than 48 slots is refused");
    bad.assign(5, StreamCfg());
    bad[0].mod = kBpsk12; bad[1].mod = kQpsk12; bad[2].mod = kQpsk23; bad[3].mod = kQpsk34; bad[4].mod = kTc8psk23;
    for (auto& b : bad) b.slots = 2;
    CHECK(!tmccFromStreams(bad, 0, t), "five groups of modulation are refused");
}

static void testWords() {
    // sync word side lobes, Walsh frame numbers, burst balance
    const auto& w = syncWord();
    int worst = 0;
    for (int k = 1; k < kSyncSyms; k++) { int c = 0; for (int i = 0; i + k < kSyncSyms; i++) c += (int)(w[i] * w[i + k]); worst = std::max(worst, std::abs(c)); }
    CHECK(worst <= 4, "sync word side lobe %d", worst);
    for (int a = 0; a < 8; a++) for (int b = 0; b < 8; b++) {
        double c = 0;
        for (int i = 0; i < kFrameIdSyms; i++) c += frameIdWord(a)[i] * frameIdWord(b)[i];
        CHECK(a == b ? c == 32 : c == 0, "frame ids %d and %d: %g", a, b, c);
    }
    const auto& bs = burstSequence();
    CHECK(bs.size() == 768, "burst length");
    double sum = 0; for (float v : bs) sum += v;
    CHECK(std::fabs(sum) < 100, "burst balance %g", sum);
}

// the coding chain of one slot, no channel: encoder -> symbols -> decoder
static void testSlot(int mod, double snrDb, int frames, int dropFrames) {
    SlotEncoder enc(mod, source(7));
    SlotDecoder dec;
    dec.configure(mod);
    std::mt19937 rng(3);
    std::normal_distribution<float> nd(0.f, 1.f);
    const float sigma = snrDb < 100 ? (float)std::sqrt(std::pow(10.0, -snrDb / 10.0) / 2.0) : 0.f;
    std::vector<cf32> sym(kSlotSyms);
    std::vector<uint8_t> out;
    for (int f = 0; f < frames; f++) {
        enc.symbols(sym.data(), kSlotSyms);
        if (f < dropFrames) continue;   // the decoder joins the stream late
        for (auto& s : sym) s += cf32(nd(rng) * sigma, nd(rng) * sigma);
        dec.push(sym.data(), kSlotSyms);
        dec.take(out);
    }
    const size_t np = out.size() / 188;
    size_t good = 0, first = 0;
    bool haveFirst = false;
    uint64_t expect = 0;
    for (size_t i = 0; i < np; i++) {
        const uint8_t* p = &out[i * 188];
        uint8_t ref[188];
        const uint64_t n = ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) | ((uint64_t)p[6] << 8) | p[7];
        testPacket(7, n, ref);
        if (memcmp(p, ref, 188) == 0) {
            good++;
            if (!haveFirst) { haveFirst = true; first = i; expect = n; }
            else if (n != expect) { CHECK(false, "%s: packet %llu where %llu was expected", modName(mod), (unsigned long long)n, (unsigned long long)expect); expect = n; }
            expect++;
        }
    }
    printf("slot %-9s %5.1f dB: %zu packets, %zu good, first good at %zu, RS failed %llu\n", modName(mod), snrDb, np, good, first, (unsigned long long)dec.stats().rsFailed);
    CHECK(good > 0 && good + 8 >= np - first, "%s at %.1f dB: %zu of %zu packets good", modName(mod), snrDb, good, np);
}

struct RunResult {
    std::map<int, std::vector<uint8_t>> byTs;   // by stream tag (PID low byte)
    IsdbsTelemetry tel;
    double cpuSecs = 0;
    std::vector<std::string> log;
};

// a signal through the receiver
static RunResult run(const GenConfig& gc, int frames, int select, double rxRate = 0, bool quiet = false) {
    RunResult r;
    Generator gen(gc);
    CHECK(gen.ok(), "generator: %s", gen.error().c_str());
    IsdbsReceiver rx;
    rx.configure(rxRate > 0 ? rxRate : gc.sampleRate);
    CHECK(rx.ready(), "receiver not ready");
    rx.selectStream(select);
    std::vector<uint8_t> all;
    rx.setPacketCallback([&](const uint8_t* p, size_t n, double) { all.insert(all.end(), p, p + n * 188); });
    rx.setLogCallback([&](const std::string& s) { r.log.push_back(s); });
    const size_t total = (size_t)((double)frames * kFrameSyms / gc.symbolRate * gc.sampleRate);
    std::vector<cf32> buf(65536);
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t done = 0; done < total;) {
        const size_t n = std::min(buf.size(), total - done);
        gen.generate(buf.data(), n);
        rx.feed(buf.data(), n);
        done += n;
    }
    r.cpuSecs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    rx.telemetry(r.tel, 0);
    for (size_t i = 0; i + 188 <= all.size(); i += 188) r.byTs[all[i + 2]].insert(r.byTs[all[i + 2]].end(), all.begin() + (long)i, all.begin() + (long)i + 188);
    if (!quiet) printf("  %zu packets out, state %d, Es/N0 %.1f dB, carrier %.0f Hz, RS clean %llu corrected %llu failed %llu, %.1f s\n", all.size() / 188, r.tel.state, r.tel.snrDb, r.tel.cfoHz,
                       (unsigned long long)r.tel.rsClean, (unsigned long long)r.tel.rsCorrected, (unsigned long long)r.tel.rsFailed, r.cpuSecs);
    return r;
}

// counts the packets of stream `tag` that match the reference and checks they come in order without gaps after the first
static size_t countGood(const std::vector<uint8_t>& v, int tag, const char* what, size_t* bad = nullptr, size_t* gaps = nullptr) {
    size_t good = 0, badn = 0, gapn = 0, flagged = 0;
    bool have = false;
    uint64_t expect = 0;
    for (size_t i = 0; i + 188 <= v.size(); i += 188) {
        const uint8_t* p = &v[i];
        const uint64_t n = ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) | ((uint64_t)p[6] << 8) | p[7];
        uint8_t ref[188];
        testPacket(tag, n, ref);
        if (memcmp(p, ref, 188) != 0) { if (p[1] & 0x80) flagged++; else badn++; continue; }   // a packet Reed-Solomon gave up on says so (transport error indicator)
        if (have && n != expect) gapn++;
        have = true; expect = n + 1; good++;
    }
    if (gapn) {   // what the order looks like
        printf("    counters:");
        for (size_t i = 0; i + 188 <= v.size() && i < 24 * 188; i += 188) printf(" %u", ((unsigned)v[i + 6] << 8) | v[i + 7]);
        printf("\n");
    }
    if (bad) *bad = badn;
    if (gaps) *gaps = gapn;
    printf("  %s: %zu good, %zu flagged as errors, %zu bad, %zu gaps\n", what, good, flagged, badn, gapn);
    return good;
}

static GenConfig hierarchical(double snrDb, double cfo, double ppm) {
    GenConfig g;
    g.sampleRate = 2 * kSymbolRate;
    g.snrDb = snrDb; g.cfoHz = cfo; g.clockPpm = ppm;
    g.streams.resize(3);
    g.streams[0].mod = kTc8psk23; g.streams[0].slots = 8; g.streams[0].tsId = 0x4010; g.streams[0].src = source(0x10);
    g.streams[1].mod = kQpsk34;   g.streams[1].slots = 8; g.streams[1].tsId = 0x4011; g.streams[1].src = source(0x11);
    g.streams[2].mod = kBpsk12;   g.streams[2].slots = 8; g.streams[2].tsId = 0x4012; g.streams[2].src = source(0x12);
    return g;
}

static void sweep() {
    struct C { int mod; double snr, cfo, ppm; };
    const C cs[] = {{kTc8psk23, 12, 0, 0}, {kTc8psk23, 11, 0, 0}, {kTc8psk23, 10, 0, 0}, {kQpsk12, 4, 0, 0}, {kQpsk12, 2, 0, 0}, {kQpsk12, 1, 0, 0},
                    {kQpsk34, 7, 0, 0}, {kQpsk34, 6, 0, 0}, {kQpsk78, 10, 0, 0}, {kQpsk78, 9, 0, 0}, {kBpsk12, 3, 0, 0}, {kBpsk12, 1.5, 0, 0}, {kTc8psk23, 14, 1.5e6, 0}, {kTc8psk23, 14, -1.7e6, 0},
                    {kTc8psk23, 14, 0, 60}, {kTc8psk23, 14, 0, -60}, {kQpsk12, 3, 400e3, 20}, {kQpsk12, 3, 40e3, 0}, {kQpsk12, 1.5, 20e3, 0}};
    for (const C& c : cs) {
        GenConfig g;
        g.sampleRate = 2 * kSymbolRate; g.snrDb = c.snr; g.cfoHz = c.cfo; g.clockPpm = c.ppm;
        g.streams.resize(1);
        g.streams[0].mod = c.mod; g.streams[0].slots = 16; g.streams[0].src = source(0x40);
        RunResult r = run(g, 260, -1, 0, true);
        size_t bad = 0, gaps = 0;
        printf("sweep %s %.1f dB cfo %.0f ppm %.0f: ", modName(c.mod), c.snr, c.cfo, c.ppm);
        countGood(r.byTs[0x40], 0x40, "", &bad, &gaps);
        printf("   locked %d, RS clean %llu corrected %llu failed %llu, est %.1f dB, cfo %.0f\n", (int)r.tel.locked, (unsigned long long)r.tel.rsClean, (unsigned long long)r.tel.rsCorrected, (unsigned long long)r.tel.rsFailed, r.tel.snrDb, r.tel.cfoHz);
    }
}

int main(int argc, char** argv) {
    if (argc > 1 && !strcmp(argv[1], "sweep")) { sweep(); return 0; }
    testTmcc();
    testWords();
    printf("-- slot chains, no channel\n");
    for (int m = kBpsk12; m <= kTc8psk23; m++) testSlot(m, 200, m == kBpsk12 ? 120 : 60, 0);
    printf("-- slot chains, noise, joining late\n");
    testSlot(kTc8psk23, 14, 60, 5);
    testSlot(kQpsk12, 4.5, 80, 7);
    testSlot(kQpsk34, 8.5, 60, 3);
    testSlot(kBpsk12, 2.0, 150, 11);

    printf("-- receiver, hierarchical signal, clean\n");
    {
        GenConfig g = hierarchical(200, 0, 0);
        RunResult r = run(g, 160, 0);
        CHECK(r.tel.tmcc.valid && r.tel.streams.size() == 3, "TMCC: %zu streams", r.tel.streams.size());
        if (r.tel.tmcc.valid) {
            CHECK(r.tel.tmcc.mode[0] == kTc8psk23 && r.tel.tmcc.count[0] == 8 && r.tel.tmcc.tsId[2] == 0x4012, "TMCC contents");
        }
        size_t bad = 0, gaps = 0;
        const size_t good = countGood(r.byTs[0x10], 0x10, "TC8PSK stream", &bad, &gaps);
        CHECK(good > 100 && bad == 0 && gaps == 0, "TC8PSK stream: %zu good %zu bad %zu gaps", good, bad, gaps);
        RunResult q = run(g, 160, 1);
        const size_t g1 = countGood(q.byTs[0x11], 0x11, "QPSK 3/4 stream", &bad, &gaps);
        CHECK(g1 > 100 && bad == 0 && gaps == 0, "QPSK stream: %zu good %zu bad %zu gaps", g1, bad, gaps);
        RunResult b = run(g, 160, 2);
        const size_t g2 = countGood(b.byTs[0x12], 0x12, "BPSK stream", &bad, &gaps);
        CHECK(g2 > 40 && bad == 0 && gaps == 0, "BPSK stream: %zu good %zu bad %zu gaps", g2, bad, gaps);
        CHECK(r.tel.snrDb > 25, "Es/N0 of a clean signal: %.1f dB", r.tel.snrDb);
    }

    printf("-- receiver, noise, carrier offset 120 kHz, symbol clock +15 ppm\n");
    {
        GenConfig g = hierarchical(16, 120e3, 15);
        g.streams.pop_back();   // TC8PSK and QPSK 3/4
        RunResult r = run(g, 140, 0);
        size_t bad = 0, gaps = 0;
        const size_t good = countGood(r.byTs[0x10], 0x10, "TC8PSK, 16 dB", &bad, &gaps);
        CHECK(good > 80 && bad == 0 && gaps == 0, "noisy TC8PSK: %zu good %zu bad %zu gaps", good, bad, gaps);
        CHECK(std::fabs(r.tel.cfoHz - 120e3) < 3e3, "carrier estimate %.0f Hz", r.tel.cfoHz);
        CHECK(std::fabs(r.tel.snrDb - 16) < 3, "Es/N0 estimate %.1f dB for 16", r.tel.snrDb);
        RunResult q = run(g, 140, 1);
        const size_t g1 = countGood(q.byTs[0x11], 0x11, "QPSK 3/4, 16 dB", &bad, &gaps);
        CHECK(g1 > 80 && bad == 0 && gaps == 0, "noisy QPSK: %zu good %zu bad %zu gaps", g1, bad, gaps);
    }

    printf("-- receiver, 40 Msample/s input, 900 kHz offset, 9 dB, QPSK 2/3 only\n");
    {
        GenConfig g;
        g.sampleRate = 40e6; g.snrDb = 9; g.cfoHz = 900e3;
        g.streams.resize(1);
        g.streams[0].mod = kQpsk23; g.streams[0].slots = 24; g.streams[0].tsId = 0x4020; g.streams[0].src = source(0x20);
        RunResult r = run(g, 140, -1);
        size_t bad = 0, gaps = 0;
        const size_t good = countGood(r.byTs[0x20], 0x20, "QPSK 2/3, 9 dB, 40 Msps", &bad, &gaps);
        CHECK(good > 150 && bad == 0 && gaps == 0, "QPSK 2/3: %zu good %zu bad %zu gaps", good, bad, gaps);
    }

    printf("-- receiver, large carrier offsets (the LNB), low Es/N0\n");
    {
        struct C { int mod; double snr, cfo; const char* what; };
        const C cs[] = {{kTc8psk23, 12, -1.7e6, "TC8PSK 12 dB, -1.7 MHz"}, {kQpsk12, 5, 400e3, "QPSK 1/2 5 dB, +400 kHz (a multiple of 1/207 symbol rate away from zero)"}, {kQpsk56, 8.5, -950e3, "QPSK 5/6 8.5 dB, -950 kHz"}};
        for (const C& c : cs) {
            GenConfig g;
            g.sampleRate = 2 * kSymbolRate; g.snrDb = c.snr; g.cfoHz = c.cfo;
            g.streams.resize(1);
            g.streams[0].mod = c.mod; g.streams[0].slots = 16; g.streams[0].tsId = 0x4040; g.streams[0].src = source(0x40);
            RunResult r = run(g, 260, -1, 0, true);
            size_t bad = 0, gaps = 0;
            const size_t good = countGood(r.byTs[0x40], 0x40, c.what, &bad, &gaps);
            CHECK(good > 150 && bad == 0 && std::fabs(r.tel.cfoHz - c.cfo) < 3e3, "%s: %zu good %zu bad, carrier %.0f Hz", c.what, good, bad, r.tel.cfoHz);
        }
    }

    printf("-- receiver, TMCC change after super-frame 6\n");
    {
        GenConfig g;
        g.sampleRate = 2 * kSymbolRate;
        g.streams.resize(1);
        g.streams[0].mod = kQpsk12; g.streams[0].slots = 16; g.streams[0].tsId = 0x4030; g.streams[0].src = source(0x30);
        g.changeAfterSuper = 12;
        g.streams2.resize(1);
        g.streams2[0].mod = kTc8psk23; g.streams2[0].slots = 16; g.streams2[0].tsId = 0x4031; g.streams2[0].src = source(0x31);
        RunResult r = run(g, 240, -1);
        size_t bad = 0, gaps = 0;
        const size_t before = countGood(r.byTs[0x30], 0x30, "before the change (QPSK 1/2)", &bad, &gaps);
        const size_t after = countGood(r.byTs[0x31], 0x31, "after the change (TC8PSK)", &bad, &gaps);
        CHECK(before > 20 && after > 80, "TMCC change: %zu before, %zu after", before, after);
        CHECK(r.tel.tmcc.valid && r.tel.tmcc.mode[0] == kTc8psk23 && r.tel.tmcc.change == 1, "TMCC after the change");
    }

    printf("-- receiver, no signal and too low a rate\n");
    {
        IsdbsReceiver rx;
        rx.configure(20e6);
        CHECK(!rx.ready(), "20 Msample/s is too low");
        rx.configure(57.72e6);
        CHECK(rx.ready(), "57.72 Msample/s works");
        std::mt19937 rng(9);
        std::normal_distribution<float> nd(0.f, 1.f);
        std::vector<cf32> n(65536);
        uint64_t pk = 0;
        rx.setPacketCallback([&](const uint8_t*, size_t c, double) { pk += c; });
        for (int i = 0; i < 20; i++) { for (auto& v : n) v = cf32(nd(rng), nd(rng)); rx.feed(n.data(), n.size()); }
        IsdbsTelemetry t;
        rx.telemetry(t, 0);
        CHECK(!t.locked && pk == 0, "noise must not lock (locked %d, state %d, %llu packets)", (int)t.locked, t.state, (unsigned long long)pk);
    }

    printf("%s\n", g_fail ? "FAILED" : "all ok");
    return g_fail ? 1 : 0;
}
