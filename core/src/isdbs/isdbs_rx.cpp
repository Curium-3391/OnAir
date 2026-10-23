// ISDB-S receiver, see isdbs_rx.h.
#include "dect2/isdbs/isdbs_rx.h"
#include "../dvbs/dvbs_front.h"
#include "dect2/dsp/dsp_compat.h"
#include "isdbs_slot.h"
#include "dect2/common/mathconst.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>

namespace dect2 {

using namespace isdbs;

namespace {
constexpr double kTwoPi = 2.0 * dect2::math::kPi;
constexpr double kMinRate = 39.0e6;
constexpr size_t kCoarseN = 32768;
constexpr size_t kSearchWin = 2 * (size_t)kFrameSyms + 128;
constexpr int kBack = 16;                 // symbols kept before the frame start for the local re-alignment
constexpr double kSyncThr = 0.30;

struct SyncHit { int pos = -1; double score = 0; };

// metric of the sync word at y[0..31]: |correlation|^2 / (32 * energy)
inline double syncMetric(const cf32* y, cf32* corr = nullptr) {
    const auto& w = syncWord();
    float cr = 0, ci = 0, e = 0;
    for (int i = 0; i < kSyncSyms; i++) { cr += y[i].real() * w[(size_t)i]; ci += y[i].imag() * w[(size_t)i]; e += std::norm(y[i]); }
    if (corr) *corr = cf32(cr, ci);
    return e > 1e-9f ? (double)(cr * cr + ci * ci) / (32.0 * e) : 0.0;
}

// first frame start in y (len symbols, at least 2 frames): the sync word at p and again one frame later
SyncHit findSync(const cf32* y, size_t len) {
    SyncHit h;
    if (len < (size_t)kFrameSyms + kSyncSyms + 1) return h;
    const size_t n = len - kFrameSyms - kSyncSyms;
    for (size_t p = 0; p < n; p++) {
        const double m1 = syncMetric(y + p);
        if (m1 < kSyncThr) continue;
        const double m2 = syncMetric(y + p + kFrameSyms);
        const double s = std::min(m1, m2);
        if (s > h.score && s >= kSyncThr) { h.score = s; h.pos = (int)p; }
    }
    return h;
}

// carrier offset (cycles per symbol) from the 8th power of the symbols: BPSK, QPSK and 8PSK points all go to the same point
double coarseCfo(const cf32* z, size_t n, double& ratio) {
    ratio = 0;
    int lg = 0;
    while (((size_t)2 << lg) <= n && lg < 15) lg++;
    const int N = 1 << lg;
    std::vector<float> re((size_t)N), im((size_t)N);
    for (int i = 0; i < N; i++) {
        const float a = std::abs(z[i]);
        cf32 u = a > 1e-9f ? z[i] / a : cf32(1, 0);
        u *= u; u *= u; u *= u;
        re[(size_t)i] = u.real(); im[(size_t)i] = u.imag();
    }
    fftSplit(re.data(), im.data(), lg, false);
    std::vector<float> mag((size_t)N);
    double sum = 0;
    for (int i = 0; i < N; i++) { mag[(size_t)i] = std::sqrt(re[(size_t)i] * re[(size_t)i] + im[(size_t)i] * im[(size_t)i]); sum += (double)mag[(size_t)i] * mag[(size_t)i]; }
    int kp = 0;
    for (int i = 1; i < N; i++) if (mag[(size_t)i] > mag[(size_t)kp]) kp = i;
    const double mean = (sum - (double)mag[(size_t)kp] * mag[(size_t)kp]) / (N - 1);
    ratio = (double)mag[(size_t)kp] * mag[(size_t)kp] / std::max(1e-30, mean);
    const float a = mag[(size_t)((kp + N - 1) % N)], b = mag[(size_t)kp], c = mag[(size_t)((kp + 1) % N)];
    const double den = (double)a - 2.0 * b + c;
    const double d = den != 0 ? 0.5 * ((double)a - c) / den : 0.0;
    double f8 = ((double)kp + d) / N;
    if (f8 > 0.5) f8 -= 1.0;
    return f8 / 8.0;
}
}  // namespace

struct IsdbsReceiver::Impl {
    double rate = 0, offsetHz = 0, centerHz = 0;
    bool configured = false;
    dvbs::DvbsFront front;
    std::vector<cf32> sy;                       // symbols from the front end, sy[0] is at absolute index `base`
    size_t head = 0;                            // start of the frame being waited for (locked) or of the unsearched symbols
    bool locked = false;
    double cfo = 0;                             // cycles per symbol
    int misses = 0;
    float snrLin = 0;
    // super-frame assembly
    bool collecting = false;
    int nextFrame = 0;
    std::vector<cf32> sfData[kFramesPerSuper];
    uint8_t slices[kFramesPerSuper][kTmccSliceBits];
    bool sliceOk[kFramesPerSuper];
    Tmcc cfg;
    std::vector<IsdbsStreamRow> rows;
    int sel = -1, active = -1;
    int selRequest = -1;
    SlotDecoder dec[kSlots];
    bool decOn[kSlots] = {};
    SlotStats baseStats;
    std::vector<uint8_t> tsOut;
    struct Held { uint64_t key; int slot; uint8_t pkt[188]; };
    std::vector<Held> held;
    uint64_t tsPacketsWindow = 0;
    std::chrono::steady_clock::time_point lastPacket{};
    double sigSecs = 0, tsRateSecs = 0;
    uint64_t tsRatePackets = 0;
    double tsBitrate = 0;
    std::vector<cf32> consts;
    // telemetry
    std::mutex mu;
    IsdbsTelemetry tel;
    uint64_t seq = 0;
    std::function<void(const std::string&)> log;
    std::function<void(const uint8_t*, size_t, double)> onPackets;

    void say(const std::string& s) { if (log) log(s); }

    void resetAll() {
        front.reset();
        sy.clear(); head = 0; skipSyms = 0; failures = 0; locked = false; cfo = 0; misses = 0; snrLin = 0;
        collecting = false; nextFrame = 0;
        cfg = Tmcc(); rows.clear(); active = -1;
        for (auto& d : dec) d.configure(kUnused);
        memset(decOn, 0, sizeof decOn);
        baseStats = SlotStats();
        tsOut.clear(); held.clear();
        tsRateSecs = 0; tsRatePackets = 0; tsBitrate = 0; consts.clear();
        std::lock_guard<std::mutex> l(mu);
        const double ir = tel.inputRate, cf = tel.centerHz;
        tel = IsdbsTelemetry();
        tel.inputRate = ir; tel.centerHz = cf;
        tel.state = configured ? 1 : 0;
        tel.seq = ++seq;
    }

    void publish(int state) {
        std::lock_guard<std::mutex> l(mu);
        tel.state = state;
        tel.cfoHz = cfo * kSymbolRate + offsetHz;
        tel.snrDb = snrLin > 0 ? (float)(10.0 * std::log10(snrLin)) : 0.f;
        tel.locked = locked;
        tel.tmcc = cfg;
        tel.streams = rows;
        tel.selected = active;
        tel.tsBitrate = tsBitrate;
        tel.constellation = consts;
        SlotStats s = baseStats;
        for (int i = 0; i < kSlots; i++) if (decOn[i]) { const auto& q = dec[i].stats(); s.packets += q.packets; s.rsClean += q.rsClean; s.rsCorrected += q.rsCorrected; s.rsFailed += q.rsFailed; }
        tel.packets = s.packets; tel.rsClean = s.rsClean; tel.rsCorrected = s.rsCorrected; tel.rsFailed = s.rsFailed;
        tel.blocksOk = s.rsClean + s.rsCorrected; tel.blocksBad = s.rsFailed;
        tel.dataValid = std::chrono::steady_clock::now() - lastPacket < std::chrono::seconds(2) && lastPacket.time_since_epoch().count() != 0;
        tel.seq = ++seq;
    }

    // ---- searching: the symbols from `head` on
    // residual carrier offset (cycles per symbol, within +-0.0024) of the frame in F, which the candidate has already taken most of: the 192
    // burst phasors turn by 2 pi f 207 from one block to the next
    static double refineCfo(const cf32* F, double* coherence = nullptr) {
        const auto& burst = burstSequence();
        std::vector<std::complex<double>> c(kBlocks);
        for (int j = 0; j < kBlocks; j++) {
            std::complex<double> a(0, 0);
            for (int i = 0; i < kBurstSyms; i++) a += std::complex<double>(F[kHeaderSyms + j * kBlockSyms + kBlockData + i]) * (double)burst[(size_t)(j * kBurstSyms + i)];
            c[(size_t)j] = a;
        }
        double bestV = -1, bestF = 0, total = 0;
        for (const auto& v : c) total += std::norm(v);
        for (int q = -120; q <= 120; q++) {
            const double fr = q * 2e-5;
            const double w = -kTwoPi * fr * kBlockSyms;
            const std::complex<double> st(std::cos(w), std::sin(w));
            std::complex<double> rot(1, 0), acc(0, 0);
            for (int j = 0; j < kBlocks; j++) { acc += c[(size_t)j] * rot; rot *= st; }
            const double v = std::norm(acc);
            if (v > bestV) { bestV = v; bestF = fr; }
        }
        if (coherence) *coherence = total > 1e-9 ? bestV / ((double)kBlocks * total) : 0.0;
        return bestF;
    }

    int failures = 0;
    size_t skipSyms = 0;                        // symbols to let go by before the next search
    bool search() {
        if (sy.size() - head < kSearchWin) return false;
        const cf32* w = sy.data() + head;
        double ratio = 0;
        const double c8 = coarseCfo(w, kCoarseN, ratio);
        std::vector<double> cands;
        if (ratio > 10.0) cands.push_back(c8);
        cands.push_back(0.0);
        const bool grid = (failures % 4) == 3;   // the slow search over the whole range, now and then
        if (grid) for (int k = 1; k <= 14; k++) { cands.push_back(k * 0.0048); cands.push_back(-k * 0.0048); }
        SyncHit best; double bestF = 0, bestCoh = 0;
        std::vector<cf32> d(kSearchWin), fr(kFrameSyms);
        for (double f : cands) {
            for (size_t i = 0; i < kSearchWin; i++) {
                const double a = -kTwoPi * f * (double)i;
                d[i] = w[i] * cf32((float)std::cos(a), (float)std::sin(a));
            }
            const SyncHit h = findSync(d.data(), kSearchWin);
            if (h.pos < 0) continue;
            // a sync word found with a carrier that is wrong by a fraction of the 32 symbol window still correlates; the 192 bursts tell right from wrong
            for (int i = 0; i < kFrameSyms; i++) fr[(size_t)i] = d[(size_t)(h.pos + i)];
            double coh = 0;
            double fres = refineCfo(fr.data(), &coh);
            {   // the bursts, 207 symbols apart, cannot tell a carrier from that plus a multiple of 1 / 207: the two halves of the sync word can (coarsely)
                const auto& sw = syncWord();
                std::complex<double> x(0, 0);
                for (int rep = 0; rep < 2; rep++) {
                    const cf32* y = &d[(size_t)(h.pos + rep * kFrameSyms)];
                    std::complex<double> c1(0, 0), c2(0, 0);
                    for (int i = 0; i < 16; i++) { c1 += std::complex<double>(y[i]) * (double)sw[(size_t)i]; c2 += std::complex<double>(y[16 + i]) * (double)sw[(size_t)(16 + i)]; }
                    x += c2 * std::conj(c1);
                }
                const double fh = std::arg(x) / (kTwoPi * 16.0);
                fres += std::round((fh - fres) * kBlockSyms) / kBlockSyms;
            }
            if (coh > bestCoh) { bestCoh = coh; best = h; bestF = f + fres; }
        }
        if (best.pos < 0 || bestCoh < 0.3) {
            // nothing in this window: drop most of it, keep enough that a frame start cannot fall between two windows
            failures++;
            head += kSearchWin - (size_t)kFrameSyms - kSyncSyms;
            if (grid) skipSyms = 24 * (size_t)kFrameSyms;   // that took longer than the signal lasted: let some of it go by
            return true;
        }
        failures = 0;
        locked = true; cfo = bestF; misses = 0;
        head += (size_t)best.pos;
        collecting = false; nextFrame = 0;
        say("ISDB-S frame sync found, carrier " + std::to_string((long)(cfo * kSymbolRate + offsetHz)) + " Hz from the centre");
        return true;
    }

    // ---- one frame at sy[head ...], already aligned
    bool frame() {
        const size_t need = (size_t)kFrameSyms + 12;
        if (sy.size() - head < need) return false;
        // local alignment: the sync word may have moved a few symbols (a slip of the symbol clock)
        std::vector<cf32> F(kFrameSyms + 64);
        auto derot = [&](size_t from, size_t n, std::vector<cf32>& out) {
            out.resize(n);
            for (size_t i = 0; i < n; i++) {
                const double a = -kTwoPi * cfo * (double)i;
                out[i] = sy[from + i] * cf32((float)std::cos(a), (float)std::sin(a));
            }
        };
        int shift = 0;
        {
            std::vector<cf32> t;
            double m0 = 0, bestM = 0;
            for (int d = -3; d <= 3; d++) {
                if ((long)head + d < 0) continue;
                derot(head + (size_t)d, kSyncSyms, t);
                const double m = syncMetric(t.data());
                if (d == 0) m0 = m;
                if (m > bestM) { bestM = m; shift = d; }
            }
            if (bestM < m0 + 0.1) { shift = 0; bestM = m0; }
            // the sync word has to be there: otherwise this is not a frame start
            if (bestM < 0.15) {
                head += (size_t)kFrameSyms;
                if (++misses >= 3) {
                    locked = false; collecting = false; cfg.valid = false;
                    { std::lock_guard<std::mutex> l(mu); tel.syncLosses++; }
                    say("ISDB-S frame sync lost");
                    publish(1);
                }
                return true;
            }
            misses = 0;
        }
        head += (size_t)((long)shift);
        derot(head, kFrameSyms, F);
        // ---- carrier phase and frequency: the sync word and the 192 bursts are known
        const auto& burst = burstSequence();
        double pos[kBlocks + 1], ph[kBlocks + 1];
        cf32 c;
        syncMetric(F.data(), &c);
        pos[0] = 15.5; ph[0] = std::atan2(c.imag(), c.real());
        for (int j = 0; j < kBlocks; j++) {
            const cf32* b = &F[(size_t)(kHeaderSyms + j * kBlockSyms + kBlockData)];
            float cr = 0, ci = 0;
            for (int i = 0; i < kBurstSyms; i++) { const float s = burst[(size_t)(j * kBurstSyms + i)]; cr += b[i].real() * s; ci += b[i].imag() * s; }
            pos[j + 1] = kHeaderSyms + j * kBlockSyms + kBlockData + 1.5;
            ph[j + 1] = std::atan2(ci, cr);
        }
        for (int k = 1; k <= kBlocks; k++) {
            double d = ph[k] - ph[k - 1];
            while (d > dect2::math::kPi) d -= kTwoPi;
            while (d < -dect2::math::kPi) d += kTwoPi;
            ph[k] = ph[k - 1] + d;
        }
        // smoothing: a straight line through the neighbours of each point (the phase moves slowly: a frequency error and a little noise)
        double sm[kBlocks + 1];
        for (int k = 0; k <= kBlocks; k++) {
            const int lo = std::max(0, k - 4), hi = std::min(kBlocks, k + 4);
            double sx = 0, sy2 = 0, sxx = 0, sxy = 0; int n = 0;
            for (int q = lo; q <= hi; q++) { const double x = pos[q] - pos[k]; sx += x; sy2 += ph[q]; sxx += x * x; sxy += x * ph[q]; n++; }
            const double den = n * sxx - sx * sx;
            sm[k] = den > 1e-9 ? (sy2 - sx * (n * sxy - sx * sy2) / den) / n : ph[k];
        }
        // derotate: the phase between two points is the straight line between their smoothed values
        {
            int k = 0;
            double a = sm[0], slp = (sm[1] - sm[0]) / (pos[1] - pos[0]);
            for (int i = 0; i < kFrameSyms; i++) {
                while (k + 1 < kBlocks && i > pos[k + 1]) { k++; slp = (sm[k + 1] - sm[k]) / (pos[k + 1] - pos[k]); }
                a = sm[k] + slp * (i - pos[k]);
                F[(size_t)i] *= cf32((float)std::cos(-a), (float)std::sin(-a));
            }
        }
        // the residual frequency moves the NCO for the next frame
        const double slope = (sm[kBlocks] - sm[0]) / (pos[kBlocks] - pos[0]) / kTwoPi;   // cycles per symbol
        cfo += 0.7 * slope;
        // Es/N0 from the burst symbols (the level is the receiver's own AGC, which includes the noise: take the amplitude of the signal out first)
        {
            double sr = 0, e = 0; int n = 0;
            for (int j = 0; j < kBlocks; j++) for (int i = 0; i < kBurstSyms; i++) {
                const cf32 y = F[(size_t)(kHeaderSyms + j * kBlockSyms + kBlockData + i)];
                const float s = burst[(size_t)(j * kBurstSyms + i)];
                sr += y.real() * s; e += std::norm(y); n++;
            }
            const double a = sr / n, nv = e / n - a * a;
            if (a > 0.05 && nv > 1e-6) { const float v = (float)(a * a / nv); snrLin = snrLin > 0 ? 0.8f * snrLin + 0.2f * v : v; }
        }
        // frame number
        int fi = 0;
        {
            double bestV = -1e30;
            for (int r = 0; r < kFramesPerSuper; r++) {
                const auto& w = frameIdWord(r);
                double v = 0;
                for (int i = 0; i < kFrameIdSyms; i++) v += F[(size_t)(kSyncSyms + kTmccSyms + i)].real() * w[(size_t)i];
                if (v > bestV) { bestV = v; fi = r; }
            }
        }
        // TMCC slice
        uint8_t bits[64];
        for (int i = 0; i < 64; i++) bits[i] = (F[(size_t)(kSyncSyms + i)].real() + F[(size_t)(kSyncSyms + 64 + i)].real()) < 0 ? 1 : 0;
        const bool sOk = tmccSliceCheck(bits);
        {
            std::lock_guard<std::mutex> l(mu);
            tel.frames++;
            if (sOk) tel.tmccGood++; else tel.tmccBad++;
        }
        // the super-frame in progress
        if (fi == 0) { collecting = true; nextFrame = 0; for (int q = 0; q < kFramesPerSuper; q++) sliceOk[q] = false; }
        if (collecting && fi != nextFrame) { collecting = false; }
        if (collecting) {
            sfData[fi].resize((size_t)kBlocks * kBlockData);
            for (int j = 0; j < kBlocks; j++) memcpy(&sfData[fi][(size_t)j * kBlockData], &F[(size_t)(kHeaderSyms + j * kBlockSyms)], kBlockData * sizeof(cf32));
            sliceOk[fi] = sOk;
            if (sOk) memcpy(slices[fi], bits, kTmccSliceBits);
            nextFrame = fi + 1;
            if (nextFrame == kFramesPerSuper) { finishSuper(); collecting = false; }
        }
        head += (size_t)kFrameSyms;
        if (!collecting || cfg.valid == false) publish(cfg.valid ? 3 : 2);
        return true;
    }

    void applyConfig(const Tmcc& t) {
        // add what the old decoders counted, then set up the new ones
        for (int i = 0; i < kSlots; i++) if (decOn[i]) { const auto& q = dec[i].stats(); baseStats.packets += q.packets; baseStats.rsClean += q.rsClean; baseStats.rsCorrected += q.rsCorrected; baseStats.rsFailed += q.rsFailed; }
        cfg = t;
        held.clear();
        rows.clear();
        int bestSlots = 0, bestRel = -1;
        for (int r = 0; r < kMaxStreams; r++) {
            const int n = t.slotsOf(r);
            if (!n) continue;
            IsdbsStreamRow row;
            row.rel = r; row.tsId = t.tsId[(size_t)r]; row.slots = n; row.mod = t.modOf(r);
            bool mixed = false;
            double br = 0;
            for (int s = 0; s < kSlots; s++) if (t.slotTs[(size_t)s] == r && t.slotMod(s) != kUnused) { if (t.slotMod(s) != row.mod) mixed = true; br += slotTsBitrate(t.slotMod(s)); }
            row.mixed = mixed; row.bitrate = br;
            rows.push_back(row);
            if (n > bestSlots) { bestSlots = n; bestRel = r; }
        }
        active = selRequest >= 0 && t.slotsOf(selRequest) > 0 ? selRequest : bestRel;
        for (int s = 0; s < kSlots; s++) {
            decOn[s] = active >= 0 && t.slotTs[(size_t)s] == active && t.slotMod(s) != kUnused;
            dec[s].configure(decOn[s] ? t.slotMod(s) : kUnused);
        }
        std::string d;
        for (const auto& r : rows) d += (d.empty() ? "" : "; ") + std::to_string(r.slots) + " slots " + modName(r.mod) + " (TS id " + std::to_string(r.tsId) + ")";
        say("ISDB-S TMCC: " + d);
        tsRateSecs = 0; tsRatePackets = 0;
    }

    void finishSuper() {
        int nOk = 0;
        for (int q = 0; q < kFramesPerSuper; q++) nOk += sliceOk[q];
        Tmcc t;
        bool have = false;
        if (nOk == kFramesPerSuper) {
            uint8_t all[kFramesPerSuper * kTmccSliceBits];
            for (int q = 0; q < kFramesPerSuper; q++) memcpy(all + (size_t)q * kTmccSliceBits, slices[q], kTmccSliceBits);
            have = tmccUnpack(all, t);
        } else if (cfg.valid && nOk > 0) {
            // slices that failed their check: the ones that passed must say the same as the last good TMCC
            const auto ref = tmccPack(cfg);
            bool same = true;
            for (int q = 0; q < kFramesPerSuper && same; q++) if (sliceOk[q] && memcmp(slices[q], &ref[(size_t)q * kTmccSliceBits], kTmccSliceBits) != 0) same = false;
            if (same) { t = cfg; have = true; }
        }
        if (!have) { publish(2); return; }
        if (!cfg.valid || t != cfg) applyConfig(t);
        if (active < 0) { publish(3); return; }
        // decode: frame by frame, slot by slot. The packets of the slots of one stream are put back in the order the transmitter took them from
        // its source: by where each one starts in its slot's bit stream, then by slot
        std::vector<cf32> sl(kSlotSyms);
        std::vector<uint8_t> pk;
        std::vector<uint64_t> keys;
        for (int f = 0; f < kFramesPerSuper; f++) {
            for (int s = 0; s < kSlots; s++) {
                if (!decOn[s]) continue;
                for (int q = 0; q < 4; q++) memcpy(&sl[(size_t)q * kBlockData], &sfData[f][(size_t)(q * kSlots + s) * kBlockData], kBlockData * sizeof(cf32));
                dec[s].push(sl.data(), kSlotSyms);
                pk.clear(); keys.clear();
                dec[s].take(pk, &keys);
                for (size_t i = 0; i < keys.size(); i++) {
                    Held h;
                    h.key = keys[i]; h.slot = s;
                    memcpy(h.pkt, &pk[i * 188], 188);
                    held.push_back(h);
                }
            }
        }
        // release what no slot can still put something in front of
        uint64_t wm = ~0ull;
        bool anySync = false;
        for (int s = 0; s < kSlots; s++) if (decOn[s] && dec[s].stats().synced) { anySync = true; wm = std::min(wm, dec[s].watermark()); }
        std::sort(held.begin(), held.end(), [](const Held& a, const Held& b) { return a.key != b.key ? a.key < b.key : a.slot < b.slot; });
        tsOut.clear();
        size_t used = 0;
        if (anySync) while (used < held.size() && held[used].key < wm) { tsOut.insert(tsOut.end(), held[used].pkt, held[used].pkt + 188); used++; }
        held.erase(held.begin(), held.begin() + (long)used);
        if (held.size() > 4096) held.erase(held.begin(), held.end() - 2048);   // a slot that never catches up must not hold the rest back for ever
        // a few data symbols for the constellation
        consts.clear();
        for (int s = 0; s < kSlots && consts.size() < 400; s++) if (decOn[s]) {
            for (int i = 0; i < kBlockData && consts.size() < 400; i += 2) consts.push_back(sfData[7][(size_t)s * kBlockData + (size_t)i]);
            break;
        }
        const double secs = kFramesPerSuper * kFrameSeconds;
        { std::lock_guard<std::mutex> l(mu); tel.superFrames++; }
        const size_t np = tsOut.size() / 188;
        if (np) {
            lastPacket = std::chrono::steady_clock::now();
            if (onPackets) onPackets(tsOut.data(), np, secs);
        }
        tsRateSecs += secs; tsRatePackets += np;
        if (tsRateSecs >= 1.0) { tsBitrate = (double)tsRatePackets * 188 * 8 / tsRateSecs; tsRateSecs = 0; tsRatePackets = 0; }
        publish(np ? 4 : 3);
    }

    void run() {
        for (;;) {
            if (!locked && skipSyms) {
                const size_t k = std::min(skipSyms, sy.size() - head);
                head += k; skipSyms -= k;
                if (skipSyms) break;
            }
            bool progressed = locked ? frame() : search();
            if (!progressed) break;
            // free what has been used
            if (head > (size_t)kFrameSyms * 2 + 4096) {
                const size_t cut = head - kBack;
                sy.erase(sy.begin(), sy.begin() + (long)cut);
                head -= cut;
            }
        }
    }
};

IsdbsReceiver::IsdbsReceiver() : p_(new Impl) {}
IsdbsReceiver::~IsdbsReceiver() = default;

void IsdbsReceiver::configure(double inputRateHz) {
    Impl& m = *p_;
    m.rate = inputRateHz;
    m.configured = ready();
    if (m.configured) {
        dvbs::DvbsFront::Config c;
        c.fs = inputRateHz; c.symbolRate = kSymbolRate; c.rollOff = kRollOff; c.centerHz = m.offsetHz;
        m.front.configure(c);
    }
    { std::lock_guard<std::mutex> l(m.mu); m.tel.inputRate = inputRateHz; }
    m.resetAll();
}

bool IsdbsReceiver::ready() const { return p_->rate >= kMinRate; }

void IsdbsReceiver::reset() { if (p_->configured) p_->resetAll(); }

void IsdbsReceiver::setSignalOffset(double hz) {
    p_->offsetHz = hz;
    if (p_->configured) p_->front.setCenter(hz);
}

void IsdbsReceiver::setCenterHz(double hz) { std::lock_guard<std::mutex> l(p_->mu); p_->centerHz = hz; p_->tel.centerHz = hz; }

void IsdbsReceiver::selectStream(int rel) { p_->selRequest = rel; }

void IsdbsReceiver::setLogCallback(std::function<void(const std::string&)> cb) { p_->log = std::move(cb); }
void IsdbsReceiver::setPacketCallback(std::function<void(const uint8_t*, size_t, double)> cb) { p_->onPackets = std::move(cb); }

void IsdbsReceiver::feed(const std::complex<float>* x, size_t n) {
    Impl& m = *p_;
    if (!m.configured || !n) return;
    m.front.process(x, n, m.sy);
    m.run();
    // a stream change the interface asked for
    if (m.cfg.valid && m.selRequest != m.active && m.selRequest >= 0 && m.cfg.slotsOf(m.selRequest) > 0) { Tmcc t = m.cfg; m.applyConfig(t); }
}

bool IsdbsReceiver::telemetry(IsdbsTelemetry& out, uint64_t lastSeq) {
    std::lock_guard<std::mutex> l(p_->mu);
    if (p_->tel.seq == lastSeq) return false;
    out = p_->tel;
    return true;
}

ModeTuning isdbsTuning() {
    ModeTuning t;
    t.stdMode = 156;                       // family 154, engine standard 156
    t.id = "isdbs";
    t.name = "ISDB-S (BS)";
    t.minMhz = 950; t.maxMhz = 2150; t.defMhz = 1049.48;   // L-band IF after an LNB; BS-1 at 11727.48 MHz with the usual 10.678 GHz oscillator
    t.sampleRate = 48e6;
    t.bandwidthMhz = 34.5;
    t.minSampleRate = kMinRate;
    t.carrierAtCentre = true;
    return t;
}

std::string isdbsSummary(const IsdbsTelemetry& t) {
    char b[200];
    if (t.state == 0) return "ISDB-S: no input";
    if (t.state == 1) return "ISDB-S: searching for a BS transponder";
    if (t.state == 2) { snprintf(b, sizeof b, "ISDB-S: frame sync, waiting for the TMCC (Es/N0 %.1f dB)", t.snrDb); return b; }
    snprintf(b, sizeof b, "ISDB-S: %zu streams, TS %d, %llu packets, RS fail %llu, Es/N0 %.1f dB", t.streams.size(), t.selected, (unsigned long long)t.packets, (unsigned long long)t.rsFailed, t.snrDb);
    return b;
}

}  // namespace dect2
