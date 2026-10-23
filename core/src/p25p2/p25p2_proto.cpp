// P25 Phase 2 protocol pieces (see p25p2_proto.h for what is established and what is this program's own layout).
#include "dect2/p25p2/p25p2_proto.h"
#include "dect2/p25/p25_proto.h"
#include <cstring>

namespace dect2 {
namespace p25p2 {

const char* duidName(int d) {
    switch (d) {
    case kDuid4V: return "4V voice";
    case kDuid2V: return "2V voice";
    case kDuidSacch: return "SACCH";
    case kDuidFacch: return "FACCH";
    default: return "unknown";
    }
}

// ---------------------------------------------------------------- ISCH
void ischSync(uint8_t d[kIschDibits]) {
    for (int i = 0; i < kIschDibits; i++) d[i] = (uint8_t)((kSyncWord >> (38 - 2 * i)) & 3);
}

void ischInfo(int slot, int sf, uint8_t d[kIschDibits]) {
    const int info = ((slot & 15) << 4) | ((sf & 3) << 2);   // 8 bits, the low two spare
    uint8_t bits[40];
    for (int i = 0; i < 8; i++)
        for (int k = 0; k < 5; k++) bits[i * 5 + k] = (uint8_t)((info >> (7 - i)) & 1);
    for (int i = 0; i < kIschDibits; i++) d[i] = (uint8_t)((bits[2 * i] << 1) | bits[2 * i + 1]);
}

bool ischDecode(const uint8_t d[kIschDibits], int& slot, int& sf, int* errs) {
    uint8_t bits[40];
    for (int i = 0; i < kIschDibits; i++) { bits[2 * i] = (uint8_t)((d[i] >> 1) & 1); bits[2 * i + 1] = (uint8_t)(d[i] & 1); }
    int info = 0, bad = 0;
    for (int i = 0; i < 8; i++) {
        int ones = 0;
        for (int k = 0; k < 5; k++) ones += bits[i * 5 + k];
        const int b = ones >= 3;
        bad += b ? 5 - ones : ones;
        info = (info << 1) | b;
    }
    if (errs) *errs = bad;
    const int s = (info >> 4) & 15;
    if (bad > 6 || (info & 3) != 0 || s >= kSlotsPerSuperframe) return false;
    slot = s; sf = (info >> 2) & 3;
    return true;
}

int syncDistance(const uint8_t d[kIschDibits]) {
    uint64_t v = 0;
    for (int i = 0; i < kIschDibits; i++) v = (v << 2) | (d[i] & 3);
    return __builtin_popcountll(v ^ kSyncWord);
}

// ---------------------------------------------------------------- DUID
void duidEncode(int duid, uint8_t d[kDuidDibits]) {
    const int v = ((duid & 15) << 4) | (~duid & 15);
    for (int i = 0; i < kDuidDibits; i++) d[i] = (uint8_t)((v >> (6 - 2 * i)) & 3);
}

int duidDecode(const uint8_t d[kDuidDibits]) {
    int v = 0;
    for (int i = 0; i < kDuidDibits; i++) v = (v << 2) | (d[i] & 3);
    int best = -1, bestDist = 9;
    for (int duid : {kDuid4V, kDuid2V, kDuidSacch, kDuidFacch}) {
        const int dist = __builtin_popcount((unsigned)(v ^ (((duid & 15) << 4) | (~duid & 15))));
        if (dist < bestDist) { bestDist = dist; best = duid; }
    }
    return bestDist <= 1 ? best : -1;   // one bit error is corrected (the four codewords are at least 4 apart)
}

// ---------------------------------------------------------------- MAC PDU codewords
void macEncode(bool facch, const uint8_t* mac, uint8_t* dibits) {
    const int nBytes = facch ? kFacchMacBytes : kSacchMacBytes;
    const int n = facch ? 52 : 46, k = facch ? 30 : 26;
    uint8_t bits[180] = {};
    for (int i = 0; i < nBytes * 8; i++) bits[i] = (uint8_t)((mac[i >> 3] >> (7 - (i & 7))) & 1);
    const uint16_t crc = p25::crcCcitt(bits, nBytes * 8);
    for (int i = 0; i < 16; i++) bits[nBytes * 8 + i] = (uint8_t)((crc >> (15 - i)) & 1);
    uint8_t sym[63] = {};
    for (int i = 0; i < k; i++) {
        int v = 0;
        for (int b = 0; b < 6; b++) v = (v << 1) | bits[i * 6 + b];
        sym[i] = (uint8_t)v;
    }
    p25::rsEncode(sym, n, k);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < 3; j++) dibits[i * 3 + j] = (uint8_t)((sym[i] >> (4 - 2 * j)) & 3);
}

bool macDecode(bool facch, const uint8_t* dibits, uint8_t* mac, int* corrected) {
    const int nBytes = facch ? kFacchMacBytes : kSacchMacBytes;
    const int n = facch ? 52 : 46, k = facch ? 30 : 26;
    uint8_t sym[63] = {};
    for (int i = 0; i < n; i++) sym[i] = (uint8_t)(((dibits[i * 3] & 3) << 4) | ((dibits[i * 3 + 1] & 3) << 2) | (dibits[i * 3 + 2] & 3));
    int errs = 0;
    if (!p25::rsDecode(sym, n, k, &errs)) return false;
    if (corrected) *corrected = errs;
    uint8_t bits[180];
    for (int i = 0; i < k; i++)
        for (int b = 0; b < 6; b++) bits[i * 6 + b] = (uint8_t)((sym[i] >> (5 - b)) & 1);
    uint16_t crc = 0;
    for (int i = 0; i < 16; i++) crc = (uint16_t)((crc << 1) | bits[nBytes * 8 + i]);
    if (crc != p25::crcCcitt(bits, nBytes * 8)) return false;
    for (int i = 0; i < nBytes; i++) {
        int v = 0;
        for (int b = 0; b < 8; b++) v = (v << 1) | bits[i * 8 + b];
        mac[i] = (uint8_t)v;
    }
    return true;
}

// ---------------------------------------------------------------- MAC PDU
const char* macOpName(int op) {
    switch (op) {
    case kMacSignal: return "MAC_SIGNAL";
    case kMacPtt: return "MAC_PTT";
    case kMacEndPtt: return "MAC_END_PTT";
    case kMacIdle: return "MAC_IDLE";
    case kMacActive: return "MAC_ACTIVE";
    case kMacHangtime: return "MAC_HANGTIME";
    default: return "MAC reserved";
    }
}

const char* macMsgName(int op) {
    switch (op) {
    case kMsgNull: return "Null";
    case kMsgGrpVUser: return "GRP_V_CH_USR";
    case kMsgUuVUser: return "UU_V_CH_USR";
    case kMsgGrpVGrant: return "GRP_V_CH_GRANT";
    case kMsgGrpVGrantUpd: return "GRP_V_CH_GRANT_UPDT";
    case kMsgUuVGrant: return "UU_V_CH_GRANT";
    case kMsgIdenUpVu: return "IDEN_UP_VU";
    case kMsgRfssSts: return "RFSS_STS_BCST";
    case kMsgNetSts: return "NET_STS_BCST";
    case kMsgAdjSts: return "ADJ_STS_BCST";
    case kMsgIdenUp: return "IDEN_UP";
    default: return "unknown";
    }
}

int macMsgLen(int op) {
    switch (op) {
    case kMsgNull: return 1;
    case kMsgGrpVUser: return 7;
    case kMsgUuVUser: return 8;
    case kMsgGrpVGrant: case kMsgGrpVGrantUpd: case kMsgUuVGrant: case kMsgIdenUpVu:
    case kMsgRfssSts: case kMsgNetSts: case kMsgAdjSts: case kMsgIdenUp: return 9;
    default: return 0;
    }
}

namespace {
std::vector<uint8_t> pad(std::vector<uint8_t> v, int n) { v.resize((size_t)n, 0); return v; }
std::vector<uint8_t> argMsg(int op, uint64_t a) {
    std::vector<uint8_t> m(9);
    m[0] = (uint8_t)op;
    for (int i = 0; i < 8; i++) m[1 + i] = (uint8_t)(a >> (56 - 8 * i));
    return m;
}
} // namespace

std::vector<uint8_t> macPtt(int n, int src, int group, int algId) {
    std::vector<uint8_t> v((size_t)kPttLen, 0);
    v[0] = macHeaderByte(kMacPtt, 0);
    v[kPttAlgOff] = (uint8_t)algId;
    put24(&v[kPttSrcOff], (uint32_t)src);
    put16(&v[kPttGrpOff], (uint32_t)group);
    return pad(v, n);
}

std::vector<uint8_t> macEndPtt(int n, int src, int group, int colorCode) {
    std::vector<uint8_t> v((size_t)kEndLen, 0);
    v[0] = macHeaderByte(kMacEndPtt, 0);
    put16(&v[kEndColorOff], (uint32_t)(colorCode & 0xFFF) << 4);
    put24(&v[kEndSrcOff], (uint32_t)src);
    put16(&v[kEndGrpOff], (uint32_t)group);
    return pad(v, n);
}

std::vector<uint8_t> macActive(int n, int svcOpts, int group, int src) {
    std::vector<uint8_t> v(8, 0);
    v[0] = macHeaderByte(kMacActive, 0);
    v[1] = kMsgGrpVUser; v[2] = (uint8_t)svcOpts;
    put16(&v[3], (uint32_t)group); put24(&v[5], (uint32_t)src);
    return pad(v, n);
}

std::vector<uint8_t> macActiveUu(int n, int svcOpts, int target, int src) {
    std::vector<uint8_t> v(9, 0);
    v[0] = macHeaderByte(kMacActive, 0);
    v[1] = kMsgUuVUser; v[2] = (uint8_t)svcOpts;
    put24(&v[3], (uint32_t)target); put24(&v[6], (uint32_t)src);
    return pad(v, n);
}

std::vector<uint8_t> macSignal(int n, const std::vector<std::vector<uint8_t>>& msgs) {
    std::vector<uint8_t> v{macHeaderByte(kMacSignal, 0)};
    for (const auto& m : msgs) v.insert(v.end(), m.begin(), m.end());
    return pad(v, n);
}

std::vector<uint8_t> macIdle(int n) { return pad({macHeaderByte(kMacIdle, 0)}, n); }

std::vector<uint8_t> msgGrpVGrant(int svc, int channel, int group, int src) {
    return argMsg(kMsgGrpVGrant, ((uint64_t)(svc & 0xFF) << 56) | ((uint64_t)(channel & 0xFFFF) << 40) | ((uint64_t)(group & 0xFFFF) << 24) | ((uint64_t)src & 0xFFFFFF));
}
std::vector<uint8_t> msgGrpVGrantUpd(int ch1, int g1, int ch2, int g2) {
    return argMsg(kMsgGrpVGrantUpd, ((uint64_t)(ch1 & 0xFFFF) << 48) | ((uint64_t)(g1 & 0xFFFF) << 32) | ((uint64_t)(ch2 & 0xFFFF) << 16) | (uint64_t)(g2 & 0xFFFF));
}
std::vector<uint8_t> msgUuVGrant(int channel, int target, int src) {
    return argMsg(kMsgUuVGrant, ((uint64_t)(channel & 0xFFFF) << 48) | ((uint64_t)(target & 0xFFFFFF) << 24) | ((uint64_t)src & 0xFFFFFF));
}
std::vector<uint8_t> msgRfssSts(int lra, int sysId, int rfss, int site, int channel, int ssc) {
    return argMsg(kMsgRfssSts, ((uint64_t)(lra & 0xFF) << 56) | ((uint64_t)(sysId & 0xFFF) << 40) | ((uint64_t)(rfss & 0xFF) << 32) | ((uint64_t)(site & 0xFF) << 24) |
                                   ((uint64_t)(channel & 0xFFFF) << 8) | (uint64_t)(ssc & 0xFF));
}
std::vector<uint8_t> msgNetSts(int lra, int wacn, int sysId, int channel, int ssc) {
    return argMsg(kMsgNetSts, ((uint64_t)(lra & 0xFF) << 56) | ((uint64_t)(wacn & 0xFFFFF) << 36) | ((uint64_t)(sysId & 0xFFF) << 24) | ((uint64_t)(channel & 0xFFFF) << 8) | (uint64_t)(ssc & 0xFF));
}
std::vector<uint8_t> msgAdjSts(int lra, int sysId, int rfss, int site, int channel, int ssc) {
    return argMsg(kMsgAdjSts, ((uint64_t)(lra & 0xFF) << 56) | ((uint64_t)(sysId & 0xFFF) << 40) | ((uint64_t)(rfss & 0xFF) << 32) | ((uint64_t)(site & 0xFF) << 24) |
                                   ((uint64_t)(channel & 0xFFFF) << 8) | (uint64_t)(ssc & 0xFF));
}
std::vector<uint8_t> msgIdenUp(int id, int bw125, int txOffset250k, int spacing125, uint32_t base5Hz) {
    return argMsg(kMsgIdenUp, ((uint64_t)(id & 15) << 60) | ((uint64_t)(bw125 & 0x1FF) << 51) | ((uint64_t)(txOffset250k & 0x1FF) << 42) | ((uint64_t)(spacing125 & 0x3FF) << 32) | base5Hz);
}

} // namespace p25p2
} // namespace dect2
