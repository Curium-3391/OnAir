// APCO Project 25 Phase 2 receiver (TIA-102.BBAB / BBAC): TDMA, two time slots, 6000 symbols/s differential QPSK in 12.5 kHz. Receive only.
//
// The channel is mixed to 0 Hz, decimated to 48 kHz (8 samples per symbol), root raised cosine filtered, and every sample phase is differentially
// demodulated in parallel; the 40-bit frame sync (also giving the carrier offset and the SNR) picks the sample phase and the slot clock. Per slot:
// ISCH (slot number), DUID, then 4V / 2V voice bursts (AMBE+2 half-rate frames) or SACCH / FACCH MAC PDUs (Reed-Solomon + CRC): PTT, END_PTT, ACTIVE, IDLE and
// the TSBK-equivalent messages (voice grants, unit-to-unit grants, voice channel users, RFSS / network / adjacent status, identifier updates).
// Encrypted calls are reported as such (from the algorithm ID of the PTT) and their voice is never decoded; the encryption sync areas are never read.
// Voice: AMBE+2 through the user-installed codec plug-in "ambe2" or mbelib (the NXDN / DMR path, nxdn_ambe.h); without one the voice frames are counted, no sound.
// ALL FIELD POSITIONS ARE THIS PROGRAM'S READING OF PUBLIC DESCRIPTIONS: see p25p2_proto.h. Not tried on a real signal.
//
// Written to the engine's contract: configure() once the sample rate is known, feed() from the analysis thread (never blocks),
// telemetry() and the setters from the interface thread.
#pragma once
#include "dect2/engine/mode_tuning.h"
#include "dect2/common/ring.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace dect2 {

struct P25p2Telemetry;   // p25p2_tel.h

class P25p2Receiver {
public:
    P25p2Receiver();
    ~P25p2Receiver();

    void configure(double inputRateHz);
    void setSignalOffset(double hz);
    bool ready() const;
    void reset();                                        // after a retune (the logs and the site stay: clearResults)
    void feed(const cf32* x, size_t n);
    bool telemetry(P25p2Telemetry& out, uint64_t lastSeq);
    void clearResults();                                 // the call log, talkgroups, messages and the site
    void setLogCallback(std::function<void(const std::string&)> cb);

    void setVolume(float v);
    void setMuted(bool m);
    void setSilent(bool s);              // decode but never open the sound device
    void setListenSlot(int slot);        // 0 the first slot heard (default), 1 or 2: only that logical channel is decoded and played
    // tests: the 8 kHz voice that was played
    void setAudioTap(std::function<void(const float* x, size_t n)> cb);
    // tests and recording: every voice frame as received (logical channel 1 or 2, the 72 voice bits, whether the call is known to be clear)
    void setVoiceTap(std::function<void(int slot, const uint8_t bits72[72], bool clear)> cb);

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

ModeTuning p25p2Tuning();

} // namespace dect2
