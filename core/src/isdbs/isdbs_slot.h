// ISDB-S: the coding chain of one slot. A slot is its own continuous stream: 188 byte packets -> energy dispersal (groups of 8, as DVB-S) ->
// Reed-Solomon (204,188) -> convolutional byte interleaver (I = 12, as DVB-S) -> inner code -> symbols, 812 of them in each frame.
// The inner code is the K = 7 (171,133) code with the DVB-S puncturing for BPSK and QPSK, and an 8 state trellis for 8PSK. Internal.
#pragma once
#include "dect2/isdbs/isdbs.h"
#include "dect2/dvbt/dvbt.h"
#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

namespace dect2 {
namespace isdbs {

class SlotEncoder {
public:
    SlotEncoder(int mod, std::function<void(uint8_t*)> src);
    void symbols(cf32* out, int n);        // the next n symbols of the slot
    uint64_t packets() const { return packets_; }
private:
    void nextPacket();
    int mod_;
    std::function<void(uint8_t*)> src_;
    dvbt::InnerEncoder enc_;
    dvbt::ConvInterleaver il_{false};
    Tc8State tc_;
    int grp_ = 0;
    uint64_t packets_ = 0;
    std::vector<uint8_t> q_;               // bits waiting to become symbols
    size_t qh_ = 0;
};

struct SlotStats {
    uint64_t packets = 0, rsClean = 0, rsCorrected = 0, rsFailed = 0, bytesCorrected = 0;
    bool synced = false;
};

class SlotDecoder {
public:
    void configure(int mod);               // also a reset
    int mod() const { return mod_; }
    // One frame's symbols of the slot (carrier recovered, unit mean power). `snrHint` is the receiver's Es/N0 estimate (linear) for the BPSK / QPSK soft values.
    void push(const cf32* sym, int n);
    // Appends the packets decoded so far (188 bytes each, transport_error_indicator set when Reed-Solomon failed)
    // `keys` (when given) gets, for every packet, where in the slot's decoded bit stream it starts: packets of different slots of one stream are
    // put in order by it (the generator took them from the source frame by frame, slot by slot)
    void take(std::vector<uint8_t>& out, std::vector<uint64_t>* keys = nullptr);
    uint64_t watermark() const { return st_.synced ? originKey_ + 1632ull * blockIdx_ : 0; }   // packets that come later start at or after this
    const SlotStats& stats() const { return st_; }
private:
    void processConv(bool flush);
    void processTc8();
    void bitsToPackets();
    int mod_ = kUnused;
    SlotStats st_;
    // convolutional chain
    std::vector<float> llr_;               // serial soft values (> 0 is bit 0) waiting for a block
    std::vector<int8_t> carry_;            // depunctured history of the previous block
    bool phaseLocked_ = false;
    int dropLeft_ = 0;                     // serial values to drop at the start so that the first one begins a puncturing period
    int searches_ = 0;
    // 8PSK chain
    std::vector<cf32> sym_;
    std::vector<cf32> carrySym_;
    // bits and bytes
    std::vector<uint8_t> bits_;            // decoded bits, one per byte
    int bitOffset_ = -1;
    std::vector<uint8_t> after_;
    dvbt::ConvInterleaver deint_{true};
    int warm_ = 0, groupIdx_ = 0, syncMisses_ = 0;
    bool haveGroup_ = false;
    std::vector<uint8_t> out_;
    std::vector<uint64_t> outKeys_;
    uint64_t bitBase_ = 0, originKey_ = 0, blockIdx_ = 0;   // decoded bit number of bits_[0]; of the first packet after sync; blocks looked at since
};

}  // namespace isdbs
}  // namespace dect2
