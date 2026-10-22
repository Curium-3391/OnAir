// KAAL voice codec 4 ("KV4"): the owner's own low-rate speech codec for the KAAL voice slots (docs/KAAL-CODEC.md).
//
// 8 kHz core: LPC analysis-by-synthesis coder (LSF prediction, fractional pitch, pyramid-VQ pulse codebook, closed-loop gains,
// adaptive postfilter). At 16 kHz the band above ~3.7 kHz is split off and sent as a parametric noise envelope (high band).
// Speech frames have a fixed size per profile; silence (VAD/DTX) sends a small SID frame or nothing, so the slot can carry data.
// The payload bits are ordered by importance: class A (envelope, pitch, gains: protect most), class B, class C (pulses).
//
// Containers (the KAAL PHY) carry the codec id, profile and frame length out of band; a frame's type follows from its length:
// sidBytes -> SID, 0 -> no data (DTX), payloadBytes -> speech. A frame that did not arrive is an erasure (concealed).
//
// Typical use:
//   kaalvoice::Config c; c.profile = kaalvoice::Profile::K6_0;
//   kaalvoice::Encoder enc(c); kaalvoice::Decoder dec(c);
//   std::vector<uint8_t> f; kaalvoice::FrameType t = enc.encode(pcm, f);   // pcm: info().frameSamples samples
//   dec.decode(t, f.data(), f.size(), out);                                // or dec.decode(..., nullptr, 0, out) for a lost frame
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dect2 {
namespace kaalvoice {

enum class CodecId : uint8_t {
    LegacyV1 = 1,     // KaalCodec (spec v0.1): 16 kHz, 20 ms, 320 bits
    LegacyV3 = 3,     // KaalCodec3: 16 kHz, 20 ms, 240 / 320 / 480 / 640 bits (old KAAL signals)
    KV4 = 4,          // this codec
};

// Speech payload rates per slot (bits per 20 ms in brackets). The four-slot KAAL PHY uses K4_8 / K6_0 / K8_0; HQ12 / HQ16 are the
// single-slot high-quality profiles.
enum class Profile : uint8_t { K4_8 = 0, K6_0 = 1, K8_0 = 2, HQ12 = 3, HQ16 = 4, Count = 5 };
int profileBitsPer20ms(Profile p);     // 96, 120, 160, 240, 320
const char* profileName(Profile p);    // "4.8", "6.0", "8.0", "12", "16"

enum class FrameType : uint8_t { Speech = 0, Sid = 1, NoData = 2 };

// Damaged importance classes, from the container's per-class checks (decode()).
enum : uint32_t { kBadA = 1u, kBadB = 2u, kBadC = 4u };

struct Config {
    int sampleRate = 16000;         // 8000 or 16000 (encoder input and decoder output)
    int frameMs = 20;               // 20 or 40
    Profile profile = Profile::K6_0;
    int payloadBits = 0;            // 0: the profile's budget; otherwise an exact speech-frame budget (custom slot sizes), see valid()
    bool dtx = true;                // encoder: voice activity detection + discontinuous transmission
    bool noiseSuppress = false;     // encoder: input noise suppression (adds 8 ms of delay)
    bool postfilter = true;         // decoder: adaptive formant / pitch postfilter
};

struct Info {
    int frameSamples = 0;           // samples per frame at sampleRate
    int payloadBits = 0;            // bits of a speech frame (the last byte is zero-padded)
    int payloadBytes = 0;
    int sidBits = 40, sidBytes = 5;
    int classBits[3] = {0, 0, 0};   // A, B, C; A + B + C + padBits = payloadBits; class A occupies the first classBits[0] bits
    int padBits = 0;
    int delaySamples = 0;           // decoder output lags the encoder input by this many samples (noise suppressor included)
};

// Checks a configuration; payloadBits (custom) must lie in [72 * frameMs / 20, 400 * frameMs / 20].
bool valid(const Config& c, std::string* why = nullptr);
Info info(const Config& c);
// The frame type a received payload length stands for (out-of-band typing): 0 -> NoData, sidBytes -> Sid, payloadBytes -> Speech.
// Any other length returns false (treat as an erasure).
bool typeOfLength(const Config& c, size_t nbytes, FrameType* t);

class Encoder {
public:
    explicit Encoder(const Config& c);
    ~Encoder();
    Encoder(const Encoder&) = delete;
    Encoder& operator=(const Encoder&) = delete;
    const Info& info() const;
    // pcm: info().frameSamples samples. out receives payloadBytes (Speech), sidBytes (Sid) or nothing (NoData).
    FrameType encode(const int16_t* pcm, std::vector<uint8_t>& out);
    void reset();
    // Statistics for the caller (rate control, UI): fraction of frames sent as speech so far.
    double speechFraction() const;
private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

class Decoder {
public:
    explicit Decoder(const Config& c);
    ~Decoder();
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;
    const Info& info() const;
    // data == nullptr (or kBadA in badClasses) is a lost frame: concealed from the past. kBadB / kBadC: those bits are replaced by
    // neutral values (B) or a noise excitation (C). pcm receives info().frameSamples samples.
    void decode(FrameType t, const uint8_t* data, size_t nbytes, int16_t* pcm, uint32_t badClasses = 0);
    void erasure(int16_t* pcm) { decode(FrameType::Speech, nullptr, 0, pcm); }
    void reset();
private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

// One interface for the current codec and the old KAAL codecs, so a receiver can follow whichever codec id the container names.
// Legacy codecs take 16 kHz / 20 ms only; their payloadBits is the frame size in bits (KaalCodec3: 240, 320, 480 or 640; v1: 320),
// packed MSB first into bytes; they have no SID / DTX (every frame is Speech).
class FrameCodec {
public:
    virtual ~FrameCodec() = default;
    virtual CodecId id() const = 0;
    virtual const Info& info() const = 0;
    virtual FrameType encode(const int16_t* pcm, std::vector<uint8_t>& out) = 0;
    virtual void decode(FrameType t, const uint8_t* data, size_t nbytes, int16_t* pcm, uint32_t badClasses = 0) = 0;
};
// nullptr when the combination is not supported.
std::unique_ptr<FrameCodec> makeFrameCodec(CodecId id, const Config& c);

}  // namespace kaalvoice
}  // namespace dect2
