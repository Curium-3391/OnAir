// ISDB-S: the coding chain of one slot, see isdbs_slot.h.
#include "isdbs_slot.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace dect2 {
namespace isdbs {

namespace {
const int kRateK[5] = {1, 2, 3, 5, 7};     // trellis steps in a puncturing period
const int kRateN[5] = {2, 3, 4, 6, 8};     // serial values in a puncturing period
constexpr float kInvSqrt2 = 0.70710678118f;
}

// ============================================================================ encoder
SlotEncoder::SlotEncoder(int mod, std::function<void(uint8_t*)> src) : mod_(mod), src_(std::move(src)), enc_(std::max(0, convRate(mod))) {}

void SlotEncoder::nextPacket() {
    uint8_t ts[188];
    if (src_) src_(ts);
    else { memset(ts, 0xFF, sizeof ts); ts[1] = 0x1F; ts[3] = 0x10; }
    ts[0] = 0x47;
    packets_++;
    // the energy dispersal of a packet depends only on its place in the group of eight
    uint8_t in[8 * 188] = {}, sc[8 * 188];
    memcpy(in + grp_ * 188, ts, 188);
    dvbt::scramble(in, 8, sc);
    uint8_t rs[204], il[204];
    dvbt::rsEncode(sc + grp_ * 188, rs);
    il_.process(rs, il, sizeof il);
    grp_ = (grp_ + 1) & 7;
    std::vector<uint8_t> bits(sizeof il * 8);
    for (size_t i = 0; i < sizeof il; i++) for (int b = 0; b < 8; b++) bits[i * 8 + b] = (il[i] >> (7 - b)) & 1;
    if (isConv(mod_)) {
        std::vector<uint8_t> coded;
        enc_.encode(bits, coded);
        q_.insert(q_.end(), coded.begin(), coded.end());
    } else {
        q_.insert(q_.end(), bits.begin(), bits.end());
    }
}

void SlotEncoder::symbols(cf32* out, int n) {
    const size_t per = mod_ == kBpsk12 ? 1 : 2;
    while (q_.size() - qh_ < (size_t)n * per) nextPacket();
    for (int i = 0; i < n; i++) {
        if (mod_ == kBpsk12) {
            out[i] = cf32(q_[qh_] ? -1.f : 1.f, 0.f);
        } else if (mod_ == kTc8psk23) {
            out[i] = psk8Point(tc8Step(tc_, q_[qh_], q_[qh_ + 1]));
        } else {
            out[i] = cf32(q_[qh_] ? -kInvSqrt2 : kInvSqrt2, q_[qh_ + 1] ? -kInvSqrt2 : kInvSqrt2);
        }
        qh_ += per;
    }
    if (qh_ > 65536) { q_.erase(q_.begin(), q_.begin() + (long)qh_); qh_ = 0; }
}

// ============================================================================ decoder
void SlotDecoder::configure(int mod) {
    mod_ = mod;
    st_ = SlotStats();
    llr_.clear(); carry_.clear(); phaseLocked_ = false; dropLeft_ = 0; searches_ = 0;
    sym_.clear(); carrySym_.clear();
    bits_.clear(); bitOffset_ = -1; after_.clear();
    deint_ = dvbt::ConvInterleaver(true);
    warm_ = 0; groupIdx_ = 0; syncMisses_ = 0; haveGroup_ = false;
    out_.clear(); outKeys_.clear();
    bitBase_ = 0; originKey_ = 0; blockIdx_ = 0;
}

void SlotDecoder::take(std::vector<uint8_t>& out, std::vector<uint64_t>* keys) {
    out.insert(out.end(), out_.begin(), out_.end());
    if (keys) keys->insert(keys->end(), outKeys_.begin(), outKeys_.end());
    out_.clear(); outKeys_.clear();
}

void SlotDecoder::push(const cf32* s, int n) {
    if (mod_ == kUnused) return;
    if (mod_ == kBpsk12) {
        for (int i = 0; i < n; i++) llr_.push_back(s[i].real());
    } else if (isConv(mod_)) {
        for (int i = 0; i < n; i++) { llr_.push_back(s[i].real()); llr_.push_back(s[i].imag()); }
    } else {
        sym_.insert(sym_.end(), s, s + n);
    }
    if (isConv(mod_)) { if (llr_.size() >= 6048) processConv(false); }
    else if (sym_.size() >= 3248) processTc8();
}

void SlotDecoder::processConv(bool) {
    const int rate = convRate(mod_), k = kRateK[rate], nper = kRateN[rate];
    size_t n = llr_.size() / 24 * 24;
    if (!n) return;
    double mean = 0;
    for (size_t i = 0; i < n; i++) mean += std::fabs(llr_[i]);
    mean /= (double)n;
    const float scale = (float)std::min(40.0, 22.0 / std::max(1e-6, mean));
    if (!phaseLocked_) {
        // where the puncturing period starts relative to the first value is unknown (the receiver joined the stream anywhere): decode with each
        // choice and keep the one whose path metric is best
        long bestMetric = 0; int best = 0;
        for (int s = 0; s < nper; s++) {
            std::vector<int8_t> soft;
            dvbt::Viterbi::depuncture(llr_.data() + s, n - (size_t)s, rate, 0, soft, scale);
            std::vector<uint8_t> b;
            long metric = 0;
            dvbt::Viterbi::decode(soft, b, 1, &metric);
            if (s == 0 || metric > bestMetric) { bestMetric = metric; best = s; }
        }
        dropLeft_ = best;
        phaseLocked_ = true;
        llr_.erase(llr_.begin(), llr_.begin() + best);
        n = llr_.size() / 24 * 24;
        if (!n) return;
    }
    std::vector<int8_t> soft;
    dvbt::Viterbi::depuncture(llr_.data(), n, rate, 0, soft, scale);
    const size_t L = 96;
    std::vector<int8_t> window = carry_;
    window.insert(window.end(), soft.begin(), soft.end());
    const size_t steps = window.size() / 2;
    std::vector<uint8_t> dec;
    dvbt::Viterbi::decode(window, dec, 1, nullptr);
    const size_t fromStep = carry_.empty() ? 0 : L;
    const size_t toStep = steps > L ? steps - L : 0;
    for (size_t t = fromStep; t < toStep; t++) bits_.push_back(dec[t]);
    const size_t keep = std::min(steps, 2 * L);
    carry_.assign(window.end() - 2 * (long)keep, window.end());
    llr_.erase(llr_.begin(), llr_.begin() + (long)n);
    (void)k;
    bitsToPackets();
}

namespace {
// Viterbi decoder for the 8 state 8PSK code over a block of symbols; two bits (u, v) a symbol, in the order the encoder takes them
void tc8Decode(const std::vector<cf32>& y, std::vector<uint8_t>& bits) {
    static cf32 pt[8];
    static bool init = false;
    if (!init) { for (int i = 0; i < 8; i++) pt[i] = psk8Point(i); init = true; }
    const size_t steps = y.size();
    std::vector<uint8_t> from(steps * 8), inp(steps * 8);
    float pm[8] = {}, npm[8];
    for (size_t t = 0; t < steps; t++) {
        float d[8];
        for (int l = 0; l < 8; l++) { const float dr = y[t].real() - pt[l].real(), di = y[t].imag() - pt[l].imag(); d[l] = dr * dr + di * di; }
        for (int i = 0; i < 8; i++) npm[i] = 1e30f;
        for (int s = 0; s < 8; s++) {
            const int a = s & 1, b = (s >> 1) & 1, c = (s >> 2) & 1;
            for (int in = 0; in < 4; in++) {
                const int u = in & 1, v = in >> 1;
                const int ns = (b ^ u) | ((c ^ v) << 1) | (a << 2);
                const float m = pm[s] + d[(v << 2) | (u << 1) | a];
                if (m < npm[ns]) { npm[ns] = m; from[t * 8 + (size_t)ns] = (uint8_t)s; inp[t * 8 + (size_t)ns] = (uint8_t)in; }
            }
        }
        float lo = npm[0];
        for (int i = 1; i < 8; i++) lo = std::min(lo, npm[i]);
        for (int i = 0; i < 8; i++) pm[i] = npm[i] - lo;
    }
    int s = 0;
    for (int i = 1; i < 8; i++) if (pm[i] < pm[s]) s = i;
    bits.assign(steps * 2, 0);
    for (size_t t = steps; t-- > 0;) {
        const int in = inp[t * 8 + (size_t)s];
        bits[t * 2] = (uint8_t)(in & 1);
        bits[t * 2 + 1] = (uint8_t)(in >> 1);
        s = from[t * 8 + (size_t)s];
    }
}
}  // namespace

void SlotDecoder::processTc8() {
    const size_t L = 48;
    std::vector<cf32> window = carrySym_;
    window.insert(window.end(), sym_.begin(), sym_.end());
    const size_t steps = window.size();
    std::vector<uint8_t> dec;
    tc8Decode(window, dec);
    const size_t fromStep = carrySym_.empty() ? 0 : L;
    const size_t toStep = steps > L ? steps - L : 0;
    for (size_t t = fromStep; t < toStep; t++) { bits_.push_back(dec[2 * t]); bits_.push_back(dec[2 * t + 1]); }
    const size_t keep = std::min(steps, 2 * L);
    carrySym_.assign(window.end() - (long)keep, window.end());
    sym_.clear();
    bitsToPackets();
}

void SlotDecoder::bitsToPackets() {
    if (bitOffset_ < 0) {
        if (bits_.size() >= 8 * 204 * 12) {
            int bestOff = -1, bestPos = 0, bestScore = 0;
            for (int off = 0; off < 8; off++) {
                const size_t nbytes = (bits_.size() - (size_t)off) / 8;
                std::vector<uint8_t> by(nbytes);
                for (size_t i = 0; i < nbytes; i++) { unsigned c = 0; for (int j = 0; j < 8; j++) c = (c << 1) | bits_[(size_t)off + 8 * i + (size_t)j]; by[i] = (uint8_t)c; }
                for (int pos = 0; pos < 204; pos++) {
                    int score = 0, total = 0;
                    for (size_t i = (size_t)pos; i < nbytes; i += 204) { total++; score += (by[i] == 0x47 || by[i] == 0xB8); }
                    if (total >= 8 && score > bestScore) { bestScore = score; bestOff = off; bestPos = pos; }
                }
            }
            if (bestOff >= 0 && bestScore >= 8 && bestScore * 4 >= 3 * (int)((bits_.size() - (size_t)bestOff) / 8 / 204)) {
                bitOffset_ = bestOff;
                { const size_t drop = (size_t)bestOff + 8 * (size_t)bestPos; bits_.erase(bits_.begin(), bits_.begin() + (long)drop); bitBase_ += drop; }
                originKey_ = bitBase_; blockIdx_ = 0;
                st_.synced = true;
                searches_ = 0;
                deint_ = dvbt::ConvInterleaver(true);
                warm_ = 12;
                after_.clear();
            } else if (bits_.size() > 8 * 204 * 40) {
                bitBase_ += bits_.size() - 8 * 204 * 20;
                bits_.erase(bits_.begin(), bits_.end() - 8 * 204 * 20);
                // the puncturing phase was chosen on the first block, which may have been noise: choose it again from time to time
                if (++searches_ >= 8) { searches_ = 0; phaseLocked_ = false; carry_.clear(); }
            }
        }
        if (bitOffset_ < 0) return;
    }
    const size_t nb = bits_.size() / 8 / 12 * 12;
    if (!nb) return;
    std::vector<uint8_t> by(nb), di(nb);
    for (size_t i = 0; i < nb; i++) { unsigned c = 0; for (int j = 0; j < 8; j++) c = (c << 1) | bits_[8 * i + (size_t)j]; by[i] = (uint8_t)c; }
    bits_.erase(bits_.begin(), bits_.begin() + (long)(nb * 8));
    bitBase_ += nb * 8;
    deint_.process(by.data(), di.data(), nb);
    after_.insert(after_.end(), di.begin(), di.end());
    size_t pos = 0;
    while (after_.size() - pos >= 204) {
        uint8_t* blk = &after_[pos];
        const uint64_t key = originKey_ + 1632ull * blockIdx_++;
        if (warm_ > 0) { warm_--; pos += 204; continue; }
        const bool syncByteOk = blk[0] == 0x47 || blk[0] == 0xB8;
        const int r = dvbt::rsDecode(blk);
        st_.packets++;
        uint8_t pkt[188];
        memcpy(pkt, blk, 188);
        if (r < 0) st_.rsFailed++;
        else if (r == 0) st_.rsClean++;
        else { st_.rsCorrected++; st_.bytesCorrected += (uint64_t)r; }
        if (r >= 0 && pkt[0] == 0xB8) { groupIdx_ = 0; haveGroup_ = true; }
        else if (haveGroup_) groupIdx_ = (groupIdx_ + 1) % 8;
        if (haveGroup_) {
            uint8_t buf[8 * 188] = {};
            memcpy(buf + groupIdx_ * 188, pkt, 188);
            dvbt::descramble(buf, (size_t)groupIdx_ + 1, 0);
            memcpy(pkt, buf + groupIdx_ * 188, 188);
            if (r < 0) pkt[1] |= 0x80;
            out_.insert(out_.end(), pkt, pkt + 188);
            outKeys_.push_back(key);
        }
        if (r >= 0 || syncByteOk) syncMisses_ = 0;
        else if (++syncMisses_ > 100) {
            st_.synced = false; bitOffset_ = -1; bitBase_ += bits_.size(); bits_.clear(); after_.clear(); syncMisses_ = 0; phaseLocked_ = false; carry_.clear(); carrySym_.clear();
            return;
        }
        pos += 204;
    }
    after_.erase(after_.begin(), after_.begin() + (long)pos);
}

}  // namespace isdbs
}  // namespace dect2
