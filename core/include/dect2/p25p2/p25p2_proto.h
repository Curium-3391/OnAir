// APCO Project 25 Phase 2 (TDMA, TIA-102.BBAB physical layer / BBAC MAC) protocol pieces shared by the receiver and the test signal.
// Receive only: nothing here is used to transmit.
//
// WHAT IS KNOWN AND WHAT IS NOT. Written without access to the standards documents, from public descriptions as remembered. Only the broad facts are
// held to be right: 6000 symbols/s (12 kbit/s) on 12.5 kHz, two time slots (30 ms each, 180 dibits), a 12-slot superframe of 360 ms, differential QPSK
// steps of +-pi/4 and +-3pi/4 (H-DQPSK uplink / H-CPM downlink are received as the same dibits here), the 40-bit frame sync word 0x575D57F7FF, the
// voice frame of 72 bits (AMBE+2 half rate, 4V = four frames, 2V = two), Reed-Solomon over GF(64) for the MAC PDUs (RS(46,26) SACCH, RS(52,30) FACCH:
// the same code family as Phase 1, p25::rsEncode), MAC opcodes (signal, PTT, end PTT, idle, active, hangtime) and the TSBK-equivalent MAC messages
// (the Phase 1 TSBK opcode plus 0x40: grant 0x40, update 0x42, unit-to-unit 0x44, RFSS / network / adjacent status 0x7A..0x7C, identifier update 0x7D / 0x74).
// NOT established, so these are this program's own layout, kept in this one file so that a correction is a one-place change: the ISCH coding of the
// slot number (the real I-ISCH code is not known here, a 5-fold repetition stands in), where in the superframe the sync-carrying ISCH sits (every 12th
// slot here), the exact bit positions inside a burst (a 4-dibit DUID, then the payload), the interleave of the MAC PDU codewords (none), the CRC of
// the MAC PDU (CCITT as in Phase 1), the AMBE+2 bit order inside a voice frame (the same as DMR / NXDN), and the pulse shape (root raised cosine, 0.2).
// The encryption sync (ESS) areas of the voice bursts are never decoded: an encrypted call is recognised from the algorithm ID of the PTT message only.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace dect2 {
namespace p25p2 {

constexpr double kSymbolRate = 6000.0;
constexpr int kSlotDibits = 180;               // 30 ms
constexpr int kIschDibits = 20;                // 40 bits at the start of every slot
constexpr int kSlotsPerSuperframe = 12;        // 360 ms
constexpr uint64_t kSyncWord = 0x575D57F7FFULL;   // 40 bits, MSB first
constexpr int kVoiceDibits = 36;               // one 72-bit AMBE+2 frame
constexpr int kDuidDibits = 4;                 // after the ISCH
constexpr int kPayloadDibits = kSlotDibits - kIschDibits - kDuidDibits;   // 156

// DUIDs of the bursts (4 bits, sent with their complement)
enum Duid { kDuid4V = 0x0, kDuid2V = 0x6, kDuidSacch = 0x9, kDuidFacch = 0xC };
const char* duidName(int d);

// Dibit <-> differential phase step in units of pi/4: 00 +1, 01 +3, 10 -1, 11 -3 (the C4FM table of Phase 1)
inline int dibitStep(int d) { static const int t[4] = {1, 3, -1, -3}; return t[d & 3]; }
inline int stepDibit(int s) { return s == 3 ? 1 : s == 1 ? 0 : s == -1 ? 2 : 3; }

// Root raised cosine impulse response at time t (in symbols), roll-off alpha (0.2: this program's assumption)
inline double rrcImpulse(double t, double a) {
    const double pi = 3.14159265358979323846;
    const double at = a * t;
    if (t > -1e-9 && t < 1e-9) return 1.0 - a + 4.0 * a / pi;
    const double q = 1.0 / (4.0 * a);
    if ((t > q - 1e-9 && t < q + 1e-9) || (t > -q - 1e-9 && t < -q + 1e-9)) {
        const double r = pi / (4.0 * a);
        return (a / 1.4142135623730951) * ((1.0 + 2.0 / pi) * __builtin_sin(r) + (1.0 - 2.0 / pi) * __builtin_cos(r));
    }
    return (__builtin_sin(pi * t * (1.0 - a)) + 4.0 * at * __builtin_cos(pi * t * (1.0 + a))) / (pi * t * (1.0 - 16.0 * at * at));
}

// ---- ISCH
// The sync-carrying ISCH is the sync word itself; the others carry the slot number (4 bits) and the superframe number in the ultraframe (2 bits).
void ischSync(uint8_t dibits[kIschDibits]);
void ischInfo(int slot, int superframe, uint8_t dibits[kIschDibits]);
// Decodes an information ISCH; false when it cannot be one. errs: repetition disagreements (of 40).
bool ischDecode(const uint8_t dibits[kIschDibits], int& slot, int& superframe, int* errs = nullptr);
inline bool slotCarriesSync(int slot) { return slot == 0; }
int syncDistance(const uint8_t dibits[kIschDibits]);          // bits that differ from the sync word

// ---- bursts (slot payload after the ISCH)
// DUID: 4 bits + complement as 4 dibits. Returns the DUID or -1.
void duidEncode(int duid, uint8_t dibits[kDuidDibits]);
int duidDecode(const uint8_t dibits[kDuidDibits]);

// MAC PDU codewords. SACCH: 17 MAC bytes + CRC-16 + 4 spare bits = 26 hexbits -> RS(46,26) -> 138 dibits. FACCH: 20 bytes + CRC-16 + 4 spare bits = 30 hexbits
// -> RS(52,30) -> 156 dibits.
constexpr int kSacchMacBytes = 17, kFacchMacBytes = 20;
constexpr int kSacchDibits = 138, kFacchDibits = 156;
void macEncode(bool facch, const uint8_t* mac, uint8_t* dibits);        // mac: kSacchMacBytes or kFacchMacBytes bytes
bool macDecode(bool facch, const uint8_t* dibits, uint8_t* mac, int* corrected = nullptr);   // RS and CRC must pass

// ---- MAC PDU
enum MacOp { kMacSignal = 0, kMacPtt = 1, kMacEndPtt = 2, kMacIdle = 3, kMacActive = 4, kMacHangtime = 6 };
const char* macOpName(int op);
// Message opcodes inside a MAC PDU (the TSBK opcode plus 0x40 for the ones that exist in both)
enum MacMsg {
    kMsgNull = 0x00, kMsgGrpVUser = 0x01, kMsgUuVUser = 0x02,
    kMsgGrpVGrant = 0x40, kMsgGrpVGrantUpd = 0x42, kMsgUuVGrant = 0x44,
    kMsgIdenUpVu = 0x74, kMsgRfssSts = 0x7A, kMsgNetSts = 0x7B, kMsgAdjSts = 0x7C, kMsgIdenUp = 0x7D,
};
const char* macMsgName(int op);
int macMsgLen(int op);                 // bytes including the opcode; 0 when the opcode is not known here (parsing of the PDU stops)

struct MacHeader { int opcode = 0, offset = 0; };
inline uint8_t macHeaderByte(int opcode, int offset) { return (uint8_t)(((opcode & 7) << 5) | ((offset & 7) << 2)); }
inline MacHeader macHeader(uint8_t b) { return {(b >> 5) & 7, (b >> 2) & 7}; }

// PTT fields (after the header byte): message indicator 9 bytes (skipped, never read), algorithm ID 1, key ID 2 (skipped), source 3, group 2
constexpr int kPttMiOff = 1, kPttAlgOff = 10, kPttKeyOff = 11, kPttSrcOff = 13, kPttGrpOff = 16, kPttLen = 18;
// END_PTT fields: colour code (12 bits) + 4 reserved bits 2 bytes, source 3, group 2
constexpr int kEndColorOff = 1, kEndSrcOff = 3, kEndGrpOff = 6, kEndLen = 8;
constexpr int kAlgClear = 0x80;

inline uint32_t be24(const uint8_t* p) { return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2]; }
inline uint32_t be16(const uint8_t* p) { return ((uint32_t)p[0] << 8) | p[1]; }
inline uint64_t be64(const uint8_t* p) { uint64_t v = 0; for (int i = 0; i < 8; i++) v = (v << 8) | p[i]; return v; }
inline void put24(uint8_t* p, uint32_t v) { p[0] = (uint8_t)(v >> 16); p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)v; }
inline void put16(uint8_t* p, uint32_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }

// Builders (the test signal and the unit tests); each returns a MAC PDU padded with zero bytes to n bytes
std::vector<uint8_t> macPtt(int n, int src, int group, int algId);
std::vector<uint8_t> macEndPtt(int n, int src, int group, int colorCode);
std::vector<uint8_t> macActive(int n, int svcOpts, int group, int src);               // MAC_ACTIVE carrying a group voice channel user message
std::vector<uint8_t> macActiveUu(int n, int svcOpts, int target, int src);            // ... a unit-to-unit voice channel user message
std::vector<uint8_t> macSignal(int n, const std::vector<std::vector<uint8_t>>& msgs); // MAC_SIGNAL with the given messages (each starts with its opcode byte)
std::vector<uint8_t> macIdle(int n);
std::vector<uint8_t> msgGrpVGrant(int svc, int channel, int group, int src);
std::vector<uint8_t> msgGrpVGrantUpd(int ch1, int g1, int ch2, int g2);
std::vector<uint8_t> msgUuVGrant(int channel, int target, int src);
std::vector<uint8_t> msgRfssSts(int lra, int sysId, int rfss, int site, int channel, int ssc);
std::vector<uint8_t> msgNetSts(int lra, int wacn, int sysId, int channel, int ssc);
std::vector<uint8_t> msgAdjSts(int lra, int sysId, int rfss, int site, int channel, int ssc);
// bandwidth in 125 Hz units (9 bits), transmit offset in 250 kHz units (8 bits + sign, sign set for positive), spacing in 125 Hz units, base in 5 Hz units
std::vector<uint8_t> msgIdenUp(int id, int bw125, int txOffset250k, int spacing125, uint32_t base5Hz);

} // namespace p25p2
} // namespace dect2
