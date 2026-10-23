// TETRAPOL data blocks, HDLC, TPDU and the signalling messages OnAir reads (tetrapol_link.h).
#include "dect2/tetrapol/tetrapol_link.h"
#include <cstring>

namespace dect2 {
namespace tetrapol {

namespace {

int fnOf(const DataFrame& f) { return (f.payload[0] & 1) | ((f.payload[1] & 1) << 1); }

// MSB-first field of a byte string (bit 0 = the top bit of t[0])
int field(const uint8_t* t, int bitOff, int n) {
    int v = 0;
    for (int i = 0; i < n; i++) {
        const int b = bitOff + i;
        v = (v << 1) | ((t[b / 8] >> (7 - b % 8)) & 1);
    }
    return v;
}

} // namespace

bool BlockAssembler::finish(std::vector<uint8_t>& bytes, int dataFrames) {
    if (bad_ > 0) {
        // one frame lost: the parity frame (data[2..65] of all frames XOR to zero) gives it back; no parity in 1 or 2 frame blocks
        if (n_ < 3 || bad_ > 1) { reset(); return false; }
        int k = -1;
        for (int i = 0; i < n_; i++) if (!ok_[i]) k = i;
        if (k < 0 || k == n_ - 1) { /* only the parity frame was lost: nothing to repair */ }
        else {
            for (int b = 2; b < kPayloadBits; b++) {
                uint8_t v = 0;
                for (int i = 0; i < n_; i++) if (i != k) v ^= fr_[i].payload[b];
                fr_[k].payload[b] = v;
            }
            repaired_++;
        }
    }
    bytes.assign((size_t)dataFrames * 8, 0);
    for (int i = 0; i < dataFrames; i++)
        for (int b = 0; b < 64; b++)
            if (fr_[i].payload[2 + b] & 1) bytes[(size_t)(i * 8 + b / 8)] |= (uint8_t)(1u << (b % 8));
    reset();
    return true;
}

bool BlockAssembler::push(const DataFrame& f, bool ok, std::vector<uint8_t>& bytes) {
    if (n_ > kBlockFramesMax) reset();
    const int fn = ok ? fnOf(f) : -1;
    if (n_ == 0) {
        if (!ok) return false;
        if (fn == 0) { fr_[0] = f; ok_[0] = true; n_ = 1; return finish(bytes, 1); }
        if (fn != 1) return false;   // a frame from the middle of a block we missed the start of
        fr_[0] = f; ok_[0] = true; fn_[0] = 1; n_ = 1;
        return false;
    }
    fr_[n_] = f; ok_[n_] = ok; fn_[n_] = fn;
    n_++;
    if (!ok) {
        // a lost frame: only a multi-frame block can get it back (by its parity frame); a lost second frame may still be one
        if (++bad_ > 1) reset();
        return false;
    }
    if (n_ == 2) {
        if (fn == 3) return finish(bytes, 2);
        if (fn == 2) return false;
        reset();
        return fn == 0 || fn == 1 ? push(f, true, bytes) : false;
    }
    // multi-frame block: 01, 10, 11 ..., 10, then the parity frame 01 (at least three data frames)
    if (fn == 1) {
        const int prev = fn_[n_ - 2];
        if (n_ - 1 >= 3 && (prev == 2 || prev == -1)) return finish(bytes, n_ - 1);
        reset();
        return push(f, true, bytes);
    }
    if (fn == 0) { reset(); return push(f, true, bytes); }
    return false;
}

std::vector<DataFrame> blockFrames(const std::vector<uint8_t>& bytes, int scramble, int asb0) {
    const int n = (int)(bytes.size() / 8);
    std::vector<DataFrame> out((size_t)(n >= 3 ? n + 1 : n));
    for (int i = 0; i < n; i++) {
        DataFrame& f = out[(size_t)i];
        for (int b = 0; b < 64; b++) f.payload[2 + b] = (uint8_t)((bytes[(size_t)(i * 8 + b / 8)] >> (b % 8)) & 1);
    }
    auto setFn = [](DataFrame& f, int fn) { f.payload[0] = (uint8_t)(fn & 1); f.payload[1] = (uint8_t)((fn >> 1) & 1); };
    if (n == 1) setFn(out[0], 0);
    else if (n == 2) { setFn(out[0], 1); setFn(out[1], 3); }
    else if (n >= 3) {
        setFn(out[0], 1);
        setFn(out[1], 2);
        for (int i = 2; i < n - 1; i++) setFn(out[(size_t)i], 3);
        setFn(out[(size_t)(n - 1)], 2);
        DataFrame& p = out[(size_t)n];
        setFn(p, 1);
        for (int b = 2; b < kPayloadBits; b++) {
            uint8_t v = 0;
            for (int i = 0; i < n; i++) v ^= out[(size_t)i].payload[b];
            p.payload[b] = v;
        }
    }
    for (auto& f : out) { f.scramble = scramble; f.asb[0] = (uint8_t)(asb0 & 1); }
    return out;
}

// FCS-16 over the bits in the order they are sent (LSB of each byte first): x^16 + x^12 + x^5 + 1, the first 16 bits inverted, and a frame
// whose FCS is right leaves 0xffff
static uint32_t fcsRun(const uint8_t* bytes, int nbits, int zeros) {
    uint32_t r = 0;
    for (int i = 0; i < nbits + zeros; i++) {
        uint32_t bit = i < nbits ? (uint32_t)((bytes[i / 8] >> (i % 8)) & 1) : 0u;
        if (i < 16) bit ^= 1u;
        r = (r << 1) | bit;
        if (r & 0x10000u) r ^= 0x11021u;
    }
    return r & 0xffffu;
}

bool fcsOk(const uint8_t* bytes, int nbytes) {
    if (nbytes < 3) return false;
    return fcsRun(bytes, nbytes * 8, 0) == 0xffffu;
}

bool parseHdlc(const std::vector<uint8_t>& b, Hdlc& h) {
    h = Hdlc();
    if (b.size() < 5) return false;
    h.z = field(b.data(), 0, 1);
    h.y = field(b.data(), 1, 3);
    h.x = field(b.data(), 4, 12);
    h.cmd = b[2];
    h.info.assign(b.begin() + 3, b.end() - 2);
    h.fcsOk = fcsOk(b.data(), (int)b.size());
    return h.fcsOk;
}

std::vector<uint8_t> buildHdlc(int z, int y, int x, uint8_t cmd, const std::vector<uint8_t>& info, int nbytes) {
    std::vector<uint8_t> b((size_t)nbytes, 0);
    b[0] = (uint8_t)(((z & 1) << 7) | ((y & 7) << 4) | ((x >> 8) & 15));
    b[1] = (uint8_t)(x & 255);
    b[2] = cmd;
    for (size_t i = 0; i < info.size() && 3 + i < (size_t)nbytes - 2; i++) b[3 + i] = info[i];
    const uint32_t r = fcsRun(b.data(), (nbytes - 2) * 8, 16);
    const uint32_t f = r ^ 0xffffu;   // sent highest coefficient first
    for (int i = 0; i < 16; i++) {
        const int pos = (nbytes - 2) * 8 + i;
        if ((f >> (15 - i)) & 1) b[(size_t)(pos / 8)] |= (uint8_t)(1u << (pos % 8));
    }
    return b;
}

bool tsduOf(const Hdlc& h, std::vector<uint8_t>& tsdu) {
    tsdu.clear();
    if (!h.fcsOk || h.info.empty()) return false;
    const uint8_t* d = h.info.data();
    const int n = (int)h.info.size();
    const bool ext = (d[0] >> 7) & 1, seg = (d[0] >> 6) & 1;
    if ((h.cmd & 0xef) == kCmdUi || h.cmd == 0xc3 || h.cmd == 0xa3 || (h.cmd & 0xef) == 0x07) {
        // unnumbered information (UI, UI on the VCH / CD, UI P0): PAS 0001-3-3 9.5.1.2
        if (ext || seg) return false;   // segmented broadcasts are not reassembled
        if (n > 3) {
            const int len = d[1];
            if (len < 1 || len > n - 2) return false;
            tsdu.assign(d + 2, d + 2 + len);
        } else {
            tsdu.assign(d + 1, d + n);
        }
        return !tsdu.empty();
    }
    if ((h.cmd & 1) == 0) {
        // information frame: connection-oriented TPDU, unsegmented with data
        const bool hasData = (d[0] >> 5) & 1;
        const int code = d[0] & 0x1f;
        if (ext || seg || !hasData || n < 3) return false;
        int off = 2;
        if ((code & 0x18) == 0) off += 2;   // CR: two reference bytes first
        if (off >= n) return false;
        const int len = d[off];
        if (len < 1 || len > n - off - 1) return false;
        tsdu.assign(d + off + 1, d + off + 1 + len);
        return true;
    }
    return false;
}

std::vector<uint8_t> uiInfo(const std::vector<uint8_t>& tsdu, int nbytes) {
    std::vector<uint8_t> info;
    info.push_back(0x00);   // ext 0, seg 0, prio 0, tsap 0
    if (nbytes > 8) info.push_back((uint8_t)tsdu.size());
    info.insert(info.end(), tsdu.begin(), tsdu.end());
    return info;
}

int uiBlockBytes(size_t len) {
    if (len <= 2) return 8;
    int n = 16;
    while ((size_t)(n - 7) < len) n += 8;
    return n;
}

bool parseSysInfo(const uint8_t* t, int len, SysInfo& s) {
    s = SysInfo();
    if (len < 9 || t[0] != kDSystemInfo) return false;
    const int mode = (t[1] >> 5) & 7;
    if (mode == 0) {
        if (len < 17) return false;
        s.country = t[3];
        s.systemId = t[4];
        s.locArea = t[5];
        s.bnId = t[6];
        const int type = field(t + 7, 0, 2);
        if (type == 0) { s.bsId = field(t + 7, 2, 6); s.rswId = field(t + 7, 8, 4); }
        else if (type == 1) { s.bsId = field(t + 7, 8, 4); s.rswId = field(t + 7, 2, 6); }
        s.uScramble = t[10];
        return true;
    }
    if (mode > 4) return false;
    s.disconnected = true;
    s.bnId = t[3];
    s.uScramble = t[4];
    s.band = field(t + 7, 0, 4);
    s.channelId = field(t + 7, 4, 12);
    return true;
}

std::vector<uint8_t> buildSysInfo(const SysInfo& s) {
    std::vector<uint8_t> t(17, 0);
    t[0] = kDSystemInfo;
    t[1] = 0;   // cell state: normal mode
    t[3] = (uint8_t)s.country;
    t[4] = (uint8_t)s.systemId;
    t[5] = (uint8_t)s.locArea;
    t[6] = (uint8_t)s.bnId;
    // cell id, format 0: type 2 bits, BS 6 bits, RSW 4 bits; then 4 bits of the cell BN
    t[7] = (uint8_t)(s.bsId & 63);
    t[8] = (uint8_t)((s.rswId & 15) << 4);
    t[10] = (uint8_t)s.uScramble;
    return t;
}

bool parseCallSignal(const uint8_t* t, int len, CallSignal& c) {
    c = CallSignal();
    if (len < 1) return false;
    switch (t[0]) {
    case kDGroupActivation:
        if (len < 9) return false;
        c.kind = CallSignal::GroupActivation;
        c.groupId = field(t + 1, 4, 12);
        c.channelId = field(t + 4, 4, 12);
        c.dScramble = t[7];
        c.keyRef = t[8];
        return true;
    case kDCallConnect:
        if (len < 15) return false;
        c.kind = CallSignal::CallConnect;
        c.channelId = field(t + 2, 4, 12);
        c.dScramble = t[5];
        c.keyRef = t[6];
        return true;
    case kDCallStart: {
        c.kind = CallSignal::CallStart;
        int i = 1;
        while (i < len) {
            const int iei = t[i++];
            if (iei == kIeiKeyReference) {
                if (i >= len) return false;
                c.keyRef = t[i++];
            } else if (iei == kIeiKeyOfCall) {
                if (i >= len) return false;
                i += 1 + t[i];   // skipped, never read
            } else {
                return false;   // an element OnAir does not know: the message is not used
            }
        }
        return i == len;
    }
    case kDCallEnd: case kDGroupEnd: case kDRelease: case kDBackCch: case kDGroupIdle:
        c.kind = CallSignal::CallEnd;
        return true;
    default:
        return false;
    }
}

std::vector<uint8_t> buildGroupActivation(int groupId, int channelId, int dScramble, int keyRef) {
    std::vector<uint8_t> t(9, 0);
    t[0] = kDGroupActivation;
    t[1] = (uint8_t)((groupId >> 8) & 15);
    t[2] = (uint8_t)(groupId & 255);
    t[4] = (uint8_t)((channelId >> 8) & 15);
    t[5] = (uint8_t)(channelId & 255);
    t[6] = (uint8_t)dScramble;
    t[7] = (uint8_t)dScramble;
    t[8] = (uint8_t)keyRef;
    return t;
}

std::vector<uint8_t> buildCallStart(int keyRef) {
    std::vector<uint8_t> t{kDCallStart};
    if (keyRef >= 0) { t.push_back(kIeiKeyReference); t.push_back((uint8_t)keyRef); }
    return t;
}

std::vector<uint8_t> buildCallEnd() { return {kDCallEnd, 0}; }

} // namespace tetrapol
} // namespace dect2
