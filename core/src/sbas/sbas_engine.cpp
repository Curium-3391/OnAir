// SBAS L1 receiver: the GNSS receiver on the SBAS PRNs plus the message content decoder. See sbas_engine.h.
#include "dect2/sbas/sbas_engine.h"
#include "dect2/gnss/gnss_gen.h"
#include "dect2/gnss/gnss_rx.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>

namespace dect2 {

struct SbasReceiver::Impl {
    GnssReceiver gnss;
    mutable std::mutex mu;                 // the decoded state; never held while calling into gnss (its feed() calls back into add())
    sbas::Network net;
    double rate = 0, centerHz = 0;
    double signalSecs = 0;
    Impl() {
        gnss.setSbasCallback([this](int prn, const uint8_t* bits, double t) {
            std::lock_guard<std::mutex> lk(mu);
            net.add(prn, bits, t);
        });
    }
    void applyCenter() { gnss.setCenterMhz(centerHz > 0 ? centerHz / 1e6 : kGnssTuneMhz[0]); }
};

SbasReceiver::SbasReceiver() : p_(std::make_unique<Impl>()) {}
SbasReceiver::~SbasReceiver() = default;

void SbasReceiver::configure(double inputRateHz) {
    p_->rate = inputRateHz;
    p_->gnss.configure(inputRateHz);
    p_->gnss.setSystems(gnssSystemBit(GnssSbas));
    p_->gnss.setClockCompare(false);
    p_->applyCenter();
    clearMessages();
}
void SbasReceiver::setSignalOffset(double) {}
void SbasReceiver::setCenterHz(double hz) { p_->centerHz = hz; p_->applyCenter(); }
bool SbasReceiver::ready() const { return p_->gnss.ready(); }
void SbasReceiver::reset() {
    p_->gnss.reset();
    clearMessages();
}
void SbasReceiver::clearMessages() {
    std::lock_guard<std::mutex> lk(p_->mu);
    p_->net.clear();
}
void SbasReceiver::feed(const cf32* x, size_t n) { p_->gnss.feed(x, n); }
void SbasReceiver::setLogCallback(std::function<void(const std::string&)> cb) { p_->gnss.setLogCallback(std::move(cb)); }

bool SbasReceiver::telemetry(SbasTelemetry& out, uint64_t lastSeq) {
    GnssTelemetry g;
    if (!p_->gnss.telemetry(g, lastSeq)) return false;
    SbasTelemetry t;
    t.seq = g.seq;
    t.cfoHz = g.cfoHz;
    t.snrDb = 0;
    t.blocksBad = g.blocksBad;
    t.inputRate = g.inputRate; t.centerMhz = g.centerMhz; t.signalSecs = g.signalSecs;
    t.levelDbfs = g.levelDbfs;
    t.searching = g.searching; t.searchPrn = g.searchPrn; t.searchProgress = g.searchProgress; t.searchRounds = g.searchRounds;
    t.status = g.status;
    {
        std::lock_guard<std::mutex> lk(p_->mu);
        for (const auto& kv : p_->net.geos()) t.geos.push_back(kv.second);
        const auto& lg = p_->net.log();
        t.log.assign(lg.size() > 100 ? lg.end() - 100 : lg.begin(), lg.end());
        for (int i = 0; i < 64; i++) t.typeCount[i] = p_->net.typeCount(i);
        t.total = p_->net.total();
        t.blocksOk = t.total;
    }
    for (const auto& c : g.channels) {
        if (c.sys != GnssSbas) continue;
        SbasSat s;
        s.prn = c.prn; s.state = c.state; s.cn0 = c.cn0; s.dopplerHz = c.dopplerHz; s.lockSecs = c.lockSecs;
        s.framesBad = c.framesBad; s.cn0Hist = c.cn0Hist;
        for (const auto& gs : t.geos) if (gs.prn == c.prn) { s.messages = gs.messages; s.lastType = gs.lastType; }
        t.snrDb = std::max(t.snrDb, c.cn0);
        t.sats.push_back(std::move(s));
    }
    std::sort(t.sats.begin(), t.sats.end(), [](const SbasSat& a, const SbasSat& b) { return a.cn0 > b.cn0; });
    t.state = t.total > 0 ? 2 : (!t.sats.empty() ? 1 : 0);
    for (const auto& gs : t.geos) {
        if (!gs.maskValid) continue;
        for (const auto& f : gs.fast) if (f.valid) { t.dataValid = true; break; }
    }
    out = std::move(t);
    return true;
}

ModeTuning sbasTuning() {
    ModeTuning t;
    t.stdMode = 152; t.id = "sbas"; t.name = "SBAS";
    t.minMhz = 1550; t.maxMhz = 1610; t.defMhz = 1575.42;
    t.sampleRate = 4e6;
    t.basebandHz = 2.5e6;
    t.bandwidthMhz = 2.046;
    t.minSampleRate = 2.046e6;
    return t;
}

const char* sbasServiceOfPrn(int prn) {
    switch (prn) {
    case 123: case 126: case 136: return "EGNOS";
    case 131: case 133: case 135: case 138: return "WAAS";
    case 129: case 137: return "MSAS";
    case 127: case 128: case 132: return "GAGAN";
    case 125: case 140: case 141: return "SDCM";
    case 130: return "BDSBAS";
    case 134: return "KASS";
    default: return "";
    }
}

std::string sbasSummary(const SbasTelemetry& t) {
    char b[200];
    if (t.state == 2) {
        int masks = 0;
        for (const auto& g : t.geos) masks += g.maskValid;
        snprintf(b, sizeof b, "SBAS: %zu satellites tracked, %llu messages, %d with a PRN mask, best %.0f dB-Hz", t.sats.size(), (unsigned long long)t.total, masks, t.snrDb);
    } else if (t.state == 1) {
        snprintf(b, sizeof b, "SBAS: tracking %zu satellites, best %.0f dB-Hz, no message yet", t.sats.size(), t.snrDb);
    } else if (t.searching) {
        snprintf(b, sizeof b, "SBAS: searching (PRN %d)", t.searchPrn);
    } else {
        snprintf(b, sizeof b, "SBAS: searching");
    }
    return b;
}

std::unique_ptr<ModeSynth> makeSbasSynth(const SynthConfig& cfg, double sampleRate) {
    SynthConfig c = cfg;
    c.modeOpt[0] = (int)gnssSystemBit(GnssSbas);
    c.modeOpt[3] |= 4;                              // structured message content
    return makeGnssSynth(c, sampleRate);
}

}  // namespace dect2
