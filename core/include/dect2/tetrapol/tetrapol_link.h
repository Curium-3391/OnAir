// TETRAPOL data link and the few signalling messages OnAir reads (PAS 0001-3-2 / 0001-3-3), on top of the data frames of tetrapol_codec.h.
//   data block:  1 frame (frame number 00), 2 frames (01, 11) or a multi-frame block (01, 10, 11 ..., 10, then a parity frame 01) carrying
//                64 bits per frame (data[2..65]), packed into bytes LSB first in the order the bits are sent
//   HDLC frame:  address (z 1, y 3, x 12 bits), command byte, information, FCS-16 (x^16 + x^12 + x^5 + 1, first 16 bits inverted)
//   TPDU:        UI (ext, seg, prio, tsap; a length byte when longer than one frame) or a connection-oriented DT with its length byte
//   TSDU:        D_SYSTEM_INFO (0x90), and for the voice gate the messages that carry a key reference: D_GROUP_ACTIVATION (0x55),
//                D_CALL_CONNECT (0x34), D_CALL_START (0x3e); a call ends with D_CALL_END (0xe2), D_GROUP_END (0x57), D_RELEASE (0x0c),
//                D_BACK_CCH (0x0b) or D_GROUP_IDLE (0x58).
// Key reference (PAS 0001-3-2): key type in the high nibble, key index in the low nibble; key index 0 is a clear call. Nothing else about
// keys is read: an encrypted call is never decoded and no key is ever used.
#pragma once
#include "dect2/tetrapol/tetrapol_codec.h"
#include <cstdint>
#include <vector>

namespace dect2 {
namespace tetrapol {

constexpr int kBlockFramesMax = 16;

// Gathers data frames into data blocks. push() returns true with the block's bytes when a block is complete.
class BlockAssembler {
public:
    bool push(const DataFrame& f, bool ok, std::vector<uint8_t>& bytes);
    void reset() { n_ = 0; bad_ = 0; }
    uint64_t repaired() const { return repaired_; }
private:
    bool finish(std::vector<uint8_t>& bytes, int dataFrames);
    DataFrame fr_[kBlockFramesMax + 1];
    bool ok_[kBlockFramesMax + 1] = {};
    int fn_[kBlockFramesMax + 1] = {};
    int n_ = 0, bad_ = 0;
    uint64_t repaired_ = 0;
};
// A data block of bytes (a multiple of 8) as data frames with their frame numbers (and the parity frame of a multi-frame block).
std::vector<DataFrame> blockFrames(const std::vector<uint8_t>& bytes, int scramble, int asb0 = 0);

struct Hdlc {
    int z = 0, y = 0, x = 0;
    uint8_t cmd = 0;
    std::vector<uint8_t> info;   // between the command byte and the FCS
    bool fcsOk = false;
};
bool parseHdlc(const std::vector<uint8_t>& block, Hdlc& h);
bool fcsOk(const uint8_t* bytes, int nbytes);
// An HDLC frame of exactly nbytes (a multiple of 8) with the FCS at the end; info is padded with zeros.
std::vector<uint8_t> buildHdlc(int z, int y, int x, uint8_t cmd, const std::vector<uint8_t>& info, int nbytes);

constexpr uint8_t kCmdUi = 0x03;
// The TSDU inside an HDLC frame (UI or an unsegmented connection-oriented DT). False when there is none in this frame.
bool tsduOf(const Hdlc& h, std::vector<uint8_t>& tsdu);
// UI information field carrying one TSDU for a block of nbytes (single frame: no length byte)
std::vector<uint8_t> uiInfo(const std::vector<uint8_t>& tsdu, int nbytes);
// The smallest data block (in bytes) that holds this TSDU as a UI frame
int uiBlockBytes(size_t tsduLen);

enum Codop : uint8_t {
    kDSystemInfo = 0x90, kDGroupActivation = 0x55, kDCallConnect = 0x34, kDCallStart = 0x3e,
    kDCallEnd = 0xe2, kDGroupEnd = 0x57, kDRelease = 0x0c, kDBackCch = 0x0b, kDGroupIdle = 0x58,
};
constexpr uint8_t kIeiKeyReference = 0x83, kIeiKeyOfCall = 0x03;

struct SysInfo {
    int country = -1, systemId = -1, locArea = -1, bnId = -1, bsId = -1, rswId = -1, uScramble = -1, channelId = -1, band = 0;
    bool disconnected = false;
};
bool parseSysInfo(const uint8_t* t, int len, SysInfo& s);
std::vector<uint8_t> buildSysInfo(const SysInfo& s);   // normal mode (cell_state 0)

// What a TSDU says about a call. keyRef -1: no key reference in the message.
struct CallSignal {
    enum Kind { None, GroupActivation, CallConnect, CallStart, CallEnd } kind = None;
    int groupId = -1, channelId = -1, keyRef = -1, dScramble = -1;
};
bool parseCallSignal(const uint8_t* t, int len, CallSignal& c);
inline bool keyRefClear(int keyRef) { return keyRef >= 0 && (keyRef & 0x0f) == 0; }
std::vector<uint8_t> buildGroupActivation(int groupId, int channelId, int dScramble, int keyRef);
std::vector<uint8_t> buildCallStart(int keyRef);   // keyRef < 0: no key reference element
std::vector<uint8_t> buildCallEnd();

} // namespace tetrapol
} // namespace dect2
