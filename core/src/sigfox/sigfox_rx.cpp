// Sigfox uplink mode (see sigfox_rx.h): the engine's wrapper around SigfoxRx (ism_sigfox.cpp) with the zone of the dial and a lock.
#include "dect2/sigfox/sigfox_rx.h"
#include "dect2/sigfox/sigfox_tel.h"
#include "dect2/sigfox/sigfox_proto.h"
#include "dect2/ism/ism_sigfox.h"
#include <cmath>
#include <ctime>
#include <mutex>

namespace dect2 {

ModeTuning sigfoxTuning() {
    ModeTuning t;
    t.stdMode = 151;
    t.id = "sigfox";
    t.name = "Sigfox";
    t.minMhz = 862.0;
    t.maxMhz = 928.0;
    t.defMhz = 868.130;
    t.sampleRate = 1024000;
    t.bandwidthMhz = 0.2;
    t.minSampleRate = 400000;
    t.tuneOffsetHz = 250000;   // the 200 kHz macro channel well clear of the radio's DC spike
    return t;
}

struct SigfoxReceiver::Impl {
    std::mutex mu;
    SigfoxRx rx;
    SigfoxModeTelemetry tel;
    std::function<void(const std::string&)> log;
    double fs = 0, off = 0, dial = 0;
    bool rateSet = false;
    uint64_t nIn = 0, lastFrames = 0, lastCarriers = 0;
    double lastPublish = 0, lastDecodeSec = -1e9, pw = 0;
    size_t pwN = 0;

    void apply() {
        rx.configure(fs, off, dial, true);
        tel.zone.clear(); tel.zoneBaud = 0;
        int nz = 0;
        const sigfox::Zone* z = sigfox::zones(nz);
        for (int i = 0; i < nz && dial > 0; i++)
            if (std::fabs(dial - z[i].ulMhz * 1e6) < 0.15e6) { tel.zone = z[i].name; tel.zoneBaud = z[i].baud; }
        tel.dialHz = dial;
        lastFrames = lastCarriers = 0;
    }
    double timeSec() const { return fs > 0 ? (double)nIn / fs : 0; }
    void publish() {
        const SigfoxTelemetry& s = rx.telemetry();
        if (s.frames > lastFrames) lastDecodeSec = timeSec();
        if (log && s.frames > lastFrames && !s.messages.empty()) {
            const SigfoxMessage& m = s.messages.front();
            char line[200];
            snprintf(line, sizeof line, "Sigfox %s #%u %s (%d bit/s)", m.id.c_str(), m.seq, m.payload.c_str(), m.baud);
            log(line);
        }
        lastFrames = s.frames; lastCarriers = s.carriers;
        tel.sf = s;
        tel.timeSec = timeSec();
        tel.state = rateSet ? (timeSec() - lastDecodeSec < 300 ? 2 : 1) : 0;
        tel.blocksOk = s.frames;
        tel.blocksBad = s.carriers > s.frames ? s.carriers - s.frames : 0;
        tel.dataValid = !s.messages.empty();
        if (!s.messages.empty() && !s.messages.front().freqs.empty()) tel.cfoHz = s.messages.front().freqs.front() - (dial > 0 ? dial : 0);
        if (pwN) tel.levelDb = (float)(10 * std::log10(pw / (double)pwN + 1e-20));
        pw = 0; pwN = 0;
        tel.seq++;
    }
};

SigfoxReceiver::SigfoxReceiver() : p_(std::make_unique<Impl>()) {}
SigfoxReceiver::~SigfoxReceiver() = default;

void SigfoxReceiver::configure(double r) {
    std::lock_guard<std::mutex> lk(p_->mu);
    p_->fs = r;
    p_->rateSet = r >= sigfoxTuning().minSampleRate - 1;
    p_->nIn = 0; p_->lastPublish = 0; p_->lastDecodeSec = -1e9;
    p_->apply();
    p_->tel.inputRate = r;
    p_->tel.state = p_->rateSet ? 1 : 0;
    p_->tel.seq++;
}
void SigfoxReceiver::setSignalOffset(double hz) {
    std::lock_guard<std::mutex> lk(p_->mu);
    if (hz == p_->off) return;
    p_->off = hz;
    if (p_->fs > 0) p_->apply();
}
void SigfoxReceiver::setDialHz(double hz) {
    std::lock_guard<std::mutex> lk(p_->mu);
    if (hz == p_->dial) return;
    p_->dial = hz;
    if (p_->fs > 0) p_->apply();
}
bool SigfoxReceiver::ready() const {
    std::lock_guard<std::mutex> lk(p_->mu);
    return p_->rateSet;
}
void SigfoxReceiver::reset() {
    std::lock_guard<std::mutex> lk(p_->mu);
    const uint64_t seq = p_->tel.seq;
    p_->tel = SigfoxModeTelemetry();
    p_->tel.seq = seq + 1;
    p_->tel.inputRate = p_->fs;
    p_->tel.state = p_->rateSet ? 1 : 0;
    p_->nIn = 0; p_->lastDecodeSec = -1e9; p_->lastPublish = 0;
    if (p_->fs > 0) p_->apply();
}
void SigfoxReceiver::clearResults() {
    std::lock_guard<std::mutex> lk(p_->mu);
    p_->rx.clear();
    p_->lastFrames = p_->lastCarriers = 0;
    p_->publish();
}
void SigfoxReceiver::setLogCallback(std::function<void(const std::string&)> cb) {
    std::lock_guard<std::mutex> lk(p_->mu);
    p_->log = std::move(cb);
}
bool SigfoxReceiver::telemetry(SigfoxModeTelemetry& out, uint64_t lastSeq) {
    std::lock_guard<std::mutex> lk(p_->mu);
    if (p_->tel.seq <= lastSeq) return false;
    out = p_->tel;
    return true;
}
void SigfoxReceiver::feed(const cf32* x, size_t n) {
    if (!x || !n) return;
    std::lock_guard<std::mutex> lk(p_->mu);
    Impl& m = *p_;
    if (!m.rateSet) { m.tel.state = 0; return; }
    for (size_t i = 0; i < n; i += 64) { const cf32 v = x[i]; if (std::isfinite(v.real()) && std::isfinite(v.imag())) { m.pw += (double)std::norm(v); m.pwN++; } }
    m.rx.feed(x, n);
    m.nIn += n;
    if (m.timeSec() - m.lastPublish >= 0.25 || m.rx.telemetry().frames != m.lastFrames) { m.lastPublish = m.timeSec(); m.publish(); }
}

}  // namespace dect2
