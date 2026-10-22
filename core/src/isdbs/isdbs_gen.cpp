// ISDB-S test signal generator, see isdbs.h. Nothing is transmitted: the samples only exist in memory or in a file.
#include "dect2/isdbs/isdbs.h"
#include "dect2/dvbs/dvbs_gen.h"
#include "isdbs_slot.h"
#include "dect2/common/mathconst.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

namespace dect2 {
namespace isdbs {

bool tmccFromStreams(const std::vector<StreamCfg>& s, int change, Tmcc& t, std::string* err) {
    auto fail = [&](const char* m) { if (err) *err = m; return false; };
    if (s.empty() || s.size() > (size_t)kMaxStreams) return fail("1 to 8 streams");
    Tmcc r;
    r.change = change & 31;
    int slot = 0, groups = 0;
    for (size_t i = 0; i < s.size(); i++) {
        const StreamCfg& c = s[i];
        if (c.mod < kBpsk12 || c.mod > kTc8psk23) return fail("a stream needs a modulation");
        if (c.slots < 1) return fail("a stream needs at least one slot");
        if (slot + c.slots > kSlots) return fail("more than 48 slots");
        if (groups > 0 && r.mode[groups - 1] == c.mod && i > 0) r.count[groups - 1] += c.slots;
        else {
            if (groups >= 4) return fail("more than four groups of modulation");
            r.mode[groups] = c.mod; r.count[groups] = c.slots; groups++;
        }
        for (int k = 0; k < c.slots; k++) r.slotTs[(size_t)(slot + k)] = (uint8_t)i;
        r.tsId[i] = c.tsId;
        slot += c.slots;
    }
    if (slot < kSlots) {   // the rest: filler
        if (groups >= 4) return fail("more than four groups of modulation (the filler is one)");
        r.mode[groups] = kUnused; r.count[groups] = kSlots - slot;
    }
    r.valid = true;
    t = r;
    return true;
}

struct Generator::Impl {
    GenConfig cfg;
    std::string err;
    bool ok = false;
    Tmcc tmcc;
    std::vector<std::unique_ptr<SlotEncoder>> enc;      // per slot, null for filler
    std::vector<size_t> fillerPos;
    uint64_t frame = 0;
    uint64_t packets = 0;
    // pulse shaping
    static constexpr int kPhases = 128;
    int half = 10;
    std::vector<float> taps;                            // kPhases + 1 rows of 2 * half
    std::vector<cf32> hist;                             // symbols, hist[0] has index base
    long long base = 0;
    double t = 0, dt = 0;                               // position of the next sample in symbols; symbols per sample
    std::complex<double> rot{1.0, 0.0}, step{1.0, 0.0};
    std::mt19937 rng;
    std::normal_distribution<float> nd{0.f, 1.f};
    float noiseSigma = 0, scale = 1;
    std::vector<cf32> pending;                          // shaped samples not handed out yet
    size_t pendPos = 0;

    void buildEncoders(const std::vector<StreamCfg>& streams) {
        enc.clear();
        enc.resize(kSlots);
        fillerPos.assign(kSlots, 0);
        for (int s = 0; s < kSlots; s++) {
            const int mod = tmcc.slotMod(s);
            if (mod == kUnused) continue;
            const StreamCfg& sc = streams[tmcc.slotTs[(size_t)s]];
            auto src = sc.src;
            uint64_t* counter = &packets;
            enc[(size_t)s] = std::make_unique<SlotEncoder>(mod, [src, counter](uint8_t* p) {
                (*counter)++;
                if (src) src(p);
                else { memset(p, 0xFF, 188); p[0] = 0x47; p[1] = 0x1F; p[3] = 0x10; }
            });
        }
    }

    void frameSymbols(std::vector<cf32>& f) {
        const int fi = (int)(frame % kFramesPerSuper);
        const uint64_t super = frame / kFramesPerSuper;
        if (fi == 0 && cfg.changeAfterSuper >= 0 && (int64_t)super == cfg.changeAfterSuper) {
            Tmcc t2;
            tmccFromStreams(cfg.streams2, tmcc.change + 1, t2);
            tmcc = t2;
            buildEncoders(cfg.streams2);
        }
        f.assign(kFrameSyms, cf32(0, 0));
        // header
        const auto& sw = syncWord();
        for (int i = 0; i < kSyncSyms; i++) f[(size_t)i] = cf32(sw[(size_t)i], 0);
        const auto bits = tmccPack(tmcc);
        uint8_t coded[64];
        tmccSliceEncode(&bits[(size_t)fi * kTmccSliceBits], coded);
        for (int i = 0; i < 64; i++) {
            const float v = coded[i] ? -1.f : 1.f;
            f[(size_t)(kSyncSyms + i)] = cf32(v, 0);
            f[(size_t)(kSyncSyms + 64 + i)] = cf32(v, 0);
        }
        const auto& fw = frameIdWord(fi);
        for (int i = 0; i < kFrameIdSyms; i++) f[(size_t)(kSyncSyms + kTmccSyms + i)] = cf32(fw[(size_t)i], 0);
        // data: the slots in order (the order the packets are taken from the sources in), then laid on the blocks
        static thread_local std::vector<cf32> sl[kSlots];
        for (int s = 0; s < kSlots; s++) {
            sl[s].resize(kSlotSyms);
            if (enc[(size_t)s]) enc[(size_t)s]->symbols(sl[s].data(), kSlotSyms);
            else {
                const std::vector<float> fl = fillerSequence(s + (int)(frame & 1) * 48, kSlotSyms);
                for (int i = 0; i < kSlotSyms; i++) sl[s][(size_t)i] = cf32(fl[(size_t)i], 0);
            }
        }
        const auto& burst = burstSequence();
        for (int j = 0; j < kBlocks; j++) {
            cf32* b = &f[(size_t)(kHeaderSyms + j * kBlockSyms)];
            const cf32* d = &sl[j % kSlots][(size_t)(j / kSlots) * kBlockData];
            for (int i = 0; i < kBlockData; i++) b[i] = d[i];
            for (int i = 0; i < kBurstSyms; i++) b[kBlockData + i] = cf32(burst[(size_t)(j * kBurstSyms + i)], 0);
        }
        frame++;
    }

    void ensureSymbols(long long upto) {   // symbols with index < upto exist
        while (base + (long long)hist.size() < upto) {
            std::vector<cf32> f;
            frameSymbols(f);
            hist.insert(hist.end(), f.begin(), f.end());
        }
    }

    void produce(size_t n) {
        pending.erase(pending.begin(), pending.begin() + (long)pendPos);
        pendPos = 0;
        const size_t old = pending.size();
        pending.resize(old + n);
        for (size_t i = 0; i < n; i++) {
            const long long k0 = (long long)std::floor(t);
            ensureSymbols(k0 + half + 2);
            const double fr = t - (double)k0;
            const double ph = fr * kPhases;
            const int p0 = (int)ph;
            const float w = (float)(ph - p0);
            const float* r0 = &taps[(size_t)p0 * 2 * (size_t)half];
            const float* r1 = &taps[(size_t)(p0 + 1) * 2 * (size_t)half];
            float re = 0, im = 0;
            // output = sum over symbols k of s[k] h(t - k), k from k0 - half + 1 to k0 + half
            const cf32* sy = &hist[(size_t)(k0 - half + 1 - base)];
            for (int m = 0; m < 2 * half; m++) {
                const float h = r0[m] + w * (r1[m] - r0[m]);
                re += sy[m].real() * h; im += sy[m].imag() * h;
            }
            std::complex<double> o = std::complex<double>(re, im) * rot;
            rot *= step;
            if ((i & 1023) == 1023) rot /= std::abs(rot);
            float nr = 0, ni = 0;
            if (noiseSigma > 0) { nr = nd(rng) * noiseSigma; ni = nd(rng) * noiseSigma; }
            pending[old + i] = cf32(((float)o.real() + nr) * scale, ((float)o.imag() + ni) * scale);
            t += dt;
        }
        // drop symbols that can no longer be reached
        const long long keepFrom = (long long)std::floor(t) - half;
        if (keepFrom - base > 4 * kFrameSyms) {
            const long long cut = keepFrom - base;
            hist.erase(hist.begin(), hist.begin() + cut);
            base += cut;
        }
    }
};

Generator::Generator(const GenConfig& cfg) : p_(new Impl) {
    Impl& m = *p_;
    m.cfg = cfg;
    if (!tmccFromStreams(cfg.streams, 0, m.tmcc, &m.err)) return;
    if (cfg.changeAfterSuper >= 0) { Tmcc t2; if (!tmccFromStreams(cfg.streams2, 1, t2, &m.err)) return; }
    m.rng.seed(cfg.seed);
    m.buildEncoders(cfg.streams);
    m.half = std::max(6, 10);
    m.taps.assign((size_t)(Impl::kPhases + 1) * 2 * (size_t)m.half, 0.f);
    for (int p = 0; p <= Impl::kPhases; p++)
        for (int mm = 0; mm < 2 * m.half; mm++) {
            // sample at fractional position fr = p / kPhases after symbol k0; symbol k = k0 - half + 1 + mm sits (fr + half - 1 - mm) symbol periods away
            const double d = (double)p / Impl::kPhases + (double)(m.half - 1 - mm);
            m.taps[(size_t)p * 2 * (size_t)m.half + (size_t)mm] = (float)dvbs::rrcAt(d, kRollOff);
        }
    m.dt = cfg.symbolRate * (1.0 + cfg.clockPpm * 1e-6) / cfg.sampleRate;
    m.t = 0;
    m.base = -(long long)m.half;
    m.hist.assign((size_t)m.half, cf32(0, 0));
    const double w = 2.0 * dect2::math::kPi * cfg.cfoHz / cfg.sampleRate;
    m.step = std::complex<double>(std::cos(w), std::sin(w));
    if (cfg.snrDb < 150) {
        const double esn0 = std::pow(10.0, cfg.snrDb / 10.0);
        const double var = (cfg.sampleRate / cfg.symbolRate) / esn0;   // complex noise power per sample for unit signal power
        m.noiseSigma = (float)std::sqrt(var / 2.0);
        m.scale = (float)(cfg.level / std::sqrt(1.0 + var));
    } else {
        m.scale = (float)cfg.level;
    }
    m.ok = true;
}

Generator::~Generator() = default;
bool Generator::ok() const { return p_->ok; }
const std::string& Generator::error() const { return p_->err; }
const Tmcc& Generator::tmcc() const { return p_->tmcc; }
uint64_t Generator::packetsSent() const { return p_->packets; }
uint64_t Generator::framesSent() const { return p_->frame; }
double Generator::sampleRate() const { return p_->cfg.sampleRate; }

void Generator::generate(cf32* out, size_t n) {
    Impl& m = *p_;
    if (!m.ok) { std::fill(out, out + n, cf32(0, 0)); return; }
    size_t done = 0;
    while (done < n) {
        if (m.pendPos >= m.pending.size()) m.produce(std::min<size_t>(65536, n - done > 4096 ? n - done : 4096));
        const size_t take = std::min(n - done, m.pending.size() - m.pendPos);
        memcpy(out + done, m.pending.data() + m.pendPos, take * sizeof(cf32));
        m.pendPos += take;
        done += take;
    }
}

}  // namespace isdbs
}  // namespace dect2
