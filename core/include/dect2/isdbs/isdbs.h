// ISDB-S (Japanese BS satellite television, ARIB STD-B20): the frame, the TMCC control information and a test signal generator.
//
// Written from public descriptions of the system, not from the standard's text and not from any other implementation. What is taken from
// those descriptions: 28.86 Msymbol/s with roll-off 0.35, a frame of 39936 symbols (192 header symbols, then 192 blocks of 203 data
// symbols and 4 burst symbols), 48 slots of 4 blocks each (block j belongs to slot j mod 48), 8 frames in a super-frame, the BPSK header,
// BPSK / QPSK / trellis coded 8PSK slots (the modulation can differ from slot to slot: hierarchical transmission), the mother code K = 7
// (171,133) with the DVB-S puncturing for BPSK and QPSK, Reed-Solomon (204,188) and the MPEG-2 transport stream. What is NOT known to be
// the same as on the air (see docs/ISDBS.md): the sync words and the layout of the TMCC bits, the exact trellis of the 8PSK code, the
// burst scrambler, the energy dispersal and byte interleaver (here the DVB-S ones), and how packets are laid on the slots (here every slot
// is its own continuous coded stream). A transmitter built from this file will not be understood by a real BS receiver, and nothing in
// the receiver has been tried on a real BS transponder.
#pragma once
#include "dect2/common/ring.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dect2 {
namespace isdbs {

constexpr double kSymbolRate = 28.860e6;   // symbols per second
constexpr double kRollOff = 0.35;
constexpr int kFrameSyms = 39936;          // per frame
constexpr int kHeaderSyms = 192;           // sync 32, TMCC 128, frame number 32 (BPSK)
constexpr int kSyncSyms = 32, kTmccSyms = 128, kFrameIdSyms = 32;
constexpr int kBlocks = 192;               // per frame
constexpr int kBlockData = 203;            // data symbols of a block
constexpr int kBurstSyms = 4;              // known BPSK symbols that end every block
constexpr int kBlockSyms = kBlockData + kBurstSyms;
constexpr int kSlots = 48;                 // per frame; block j belongs to slot j % 48
constexpr int kSlotSyms = 4 * kBlockData;  // data symbols of one slot in one frame
constexpr int kFramesPerSuper = 8;
constexpr int kMaxStreams = 8;             // relative transport stream numbers 0..7
constexpr double kFrameSeconds = kFrameSyms / kSymbolRate;
static_assert(kHeaderSyms + kBlocks * kBlockSyms == kFrameSyms, "frame layout");
static_assert(kBlocks == 4 * kSlots, "four blocks a slot");

// Modulation and code of a slot (the value in the TMCC)
enum Mod { kUnused = 0, kBpsk12 = 1, kQpsk12 = 2, kQpsk23 = 3, kQpsk34 = 4, kQpsk56 = 5, kQpsk78 = 6, kTc8psk23 = 7 };
const char* modName(int mod);
int bitsPerSymbol(int mod);                // 1 BPSK, 2 QPSK, 2 TC8PSK (information bits per symbol before the inner code: see slotBitsPerFrame)
int convRate(int mod);                     // index into dvbt's rates (0 1/2 .. 4 7/8), -1 for BPSK-less / TC8PSK / unused
bool isConv(int mod);                      // BPSK and QPSK: convolutional inner code
// coded bits a slot sends in one frame
inline int slotCodedBits(int mod) { return mod == kBpsk12 ? kSlotSyms : mod == kUnused ? 0 : 2 * kSlotSyms; }
// net bits a slot carries in one frame: before the Reed-Solomon code and the packet header overhead, i.e. the coded bits times the code rate
double slotNetBits(int mod);
// transport stream rate (bit/s) of one slot, 188 byte packets, all overhead out
double slotTsBitrate(int mod);

// ---- TMCC: what the transmitter says about the slots. 5 bit change counter + 43 payload bits in every frame, 8 frames per super-frame.
struct Tmcc {
    bool valid = false;
    int change = 0;                        // 0..31, steps whenever anything else changes
    int mode[4] = {0, 0, 0, 0};            // Mod of up to four groups of slots (hierarchical modes)
    int count[4] = {0, 0, 0, 0};           // slots in each group (the groups follow each other from slot 0)
    std::array<uint8_t, kSlots> slotTs{};  // relative transport stream number (0..7) of each slot
    std::array<uint16_t, kMaxStreams> tsId{};   // transport stream id of each relative number
    bool operator==(const Tmcc& o) const;
    bool operator!=(const Tmcc& o) const { return !(*this == o); }
    int slotMod(int slot) const;           // Mod of a slot, kUnused when not allocated
    int slotsOf(int rel) const;            // number of slots carrying relative stream `rel`
    int modOf(int rel) const;              // modulation of the first slot of the stream (-1 none)
};
constexpr int kTmccSliceBits = 48;         // per frame: 5 change + 43 payload
constexpr int kTmccPayloadBits = 43 * kFramesPerSuper;
// The 384 bits of a super-frame; slice k (the 48 bits of frame k) is bits [48 k, 48 k + 48)
std::array<uint8_t, kFramesPerSuper * kTmccSliceBits> tmccPack(const Tmcc& t);
// Reads a super-frame's bits back. Returns false when the fields are not consistent (a mode code that does not exist, slot counts beyond 48).
bool tmccUnpack(const uint8_t* bits384, Tmcc& t);
// 64 transmitted bits for the 48 bits of a slice (16 bit CRC), and the check on the way back
void tmccSliceEncode(const uint8_t slice48[48], uint8_t out64[64]);
bool tmccSliceCheck(const uint8_t bits64[64]);

// ---- the fixed parts of the frame
const std::array<float, kSyncSyms>& syncWord();                            // BPSK +-1
const std::array<float, kFrameIdSyms>& frameIdWord(int frame);             // Walsh function, frame 0..7
// The burst symbols of a frame (768, BPSK +-1) and the filler of an unused slot
const std::vector<float>& burstSequence();
std::vector<float> fillerSequence(int slot, size_t n);

// ---- trellis coded 8PSK (rate 2/3, 8 states): two information bits (u, v) a symbol
struct Tc8State { uint8_t a = 0, b = 0, c = 0; };
// Advances the encoder; returns the symbol label 0..7 (the point exp(j 2 pi label / 8))
int tc8Step(Tc8State& s, int u, int v);
cf32 psk8Point(int label);

// ---- test signal
struct StreamCfg {
    int mod = kTc8psk23;                   // all slots of one stream share one modulation and code
    int slots = 8;
    uint16_t tsId = 0x4010;
    std::function<void(uint8_t*)> src;     // fills one 188 byte packet; null: null packets
};
struct GenConfig {
    double sampleRate = 57.72e6;
    double symbolRate = kSymbolRate;
    double snrDb = 200;                    // Es/N0, 200 = none
    double cfoHz = 0;
    double clockPpm = 0;                   // the symbol clock runs this much fast
    double level = 0.2;                    // rms of the clean signal
    uint32_t seed = 1;
    std::vector<StreamCfg> streams;        // at most 8 streams in at most 4 groups of different modulation, together at most 48 slots; the rest is filler
    int changeAfterSuper = -1;             // >= 0: step the TMCC change counter and swap in `streams2` at this super-frame
    std::vector<StreamCfg> streams2;
};
class Generator {
public:
    explicit Generator(const GenConfig& cfg);
    ~Generator();
    bool ok() const;                       // false: the configuration does not fit (see error())
    const std::string& error() const;
    void generate(cf32* out, size_t n);    // the next n samples
    const Tmcc& tmcc() const;
    uint64_t packetsSent() const;
    uint64_t framesSent() const;
    double sampleRate() const;
private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};
// The Tmcc for a list of streams (empty when it does not fit)
bool tmccFromStreams(const std::vector<StreamCfg>& s, int change, Tmcc& t, std::string* err = nullptr);

}  // namespace isdbs
}  // namespace dect2
