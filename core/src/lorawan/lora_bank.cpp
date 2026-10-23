// Bank of LoRa channels (see lora_bank.h).
#include "dect2/lorawan/lora_bank.h"
#include <algorithm>
#include <cmath>

namespace dect2 {
namespace lorabank {

namespace {
constexpr size_t kBlock = 4096;
}

struct Bank::Impl {
    struct Dec {
        std::unique_ptr<lora::Demod> dm;
        size_t stat = 0;
        int sf = 0, sync = 0;
        bool inverted = false;
        lora::DemodStats seen;
    };
    struct Chan {
        std::unique_ptr<lora::Channelizer> ch;
        lora::ChanBuf buf, bufInv;           // bufInv: the conjugated samples (inverted IQ), only filled when a decoder wants them
        bool wantInv = false;
        std::vector<Dec> decs;
        size_t index = 0;
        double offsetHz = 0;
    };
    struct Recent { std::vector<uint8_t> payload; double sec; size_t chan; int sf; bool inv; };

    double rate = 0;
    std::vector<Channel> cfg;
    std::vector<Chan> chans;
    std::vector<DecoderStat> stats;
    std::vector<cf32> pend;
    std::vector<lora::RxFrame> frames;
    std::vector<Recent> recent;
    int64_t nProc = 0;
    size_t running = 0, inBandChans = 0;

    void block(const cf32* x, size_t n, std::vector<Decoded>& out) {
        for (Chan& c : chans) {
            const int64_t before = c.buf.end();
            c.ch->process(x, n, c.buf);
            if (c.wantInv) {
                std::vector<cf32> tmp((size_t)(c.buf.end() - before));
                for (size_t i = 0; i < tmp.size(); i++) tmp[i] = std::conj(c.buf.at(before + (int64_t)i)[0]);
                c.bufInv.append(tmp.data(), tmp.size());
            }
            int64_t keep = c.buf.end(), keepInv = c.bufInv.end();
            for (Dec& d : c.decs) {
                frames.clear();
                const lora::ChanBuf& b = d.inverted ? c.bufInv : c.buf;
                d.dm->process(b, frames);
                DecoderStat& st = stats[d.stat];
                const lora::DemodStats& ds = d.dm->stats();
                st.preambles = ds.preambles; st.frames = ds.frames; st.crcBad = ds.crcBad; st.headerBad = ds.headerBad; st.syncBad = ds.syncBad;
                d.seen = ds;
                for (auto& f : frames) {
                    const double t = (f.startSample * c.ch->decim() - c.ch->delayIn()) / rate;   // channels start with the bank: sample 0
                    bool dup = false;
                    for (const auto& r : recent)
                        if (r.chan == c.index && (r.sf != d.sf || r.inv != d.inverted) && r.payload == f.payload && std::fabs(r.sec - t) < 0.25) dup = true;   // another setting locked the same frame
                    if (dup) continue;
                    recent.push_back({f.payload, t, c.index, d.sf, d.inverted});
                    if (recent.size() > 16) recent.erase(recent.begin());
                    Decoded o;
                    o.chan = c.index; o.sf = d.sf; o.sync = d.sync; o.bwHz = cfg[c.index].bwHz; o.offsetHz = c.offsetHz; o.inverted = d.inverted;
                    o.sec = t; o.f = std::move(f);
                    out.push_back(std::move(o));
                }
                (d.inverted ? keepInv : keep) = std::min(d.inverted ? keepInv : keep, d.dm->oldestNeeded());
            }
            c.buf.trim(keep);
            if (c.wantInv) c.bufInv.trim(keepInv);
        }
        nProc += (int64_t)n;
    }
};

Bank::Bank() : p_(std::make_unique<Impl>()) {}
Bank::~Bank() = default;

bool Bank::fits(double rate, const Channel& c) {
    const double fb = std::max(c.bwHz, c.filterBwHz);
    return rate >= 2.2 * fb && std::fabs(c.offsetHz) + 0.5 * fb + c.marginHz < 0.45 * rate;
}

void Bank::configure(double inputRate, const std::vector<Channel>& chans) {
    Impl& m = *p_;
    m.rate = inputRate;
    m.cfg = chans;
    m.chans.clear(); m.stats.clear(); m.pend.clear(); m.recent.clear();
    m.nProc = 0; m.running = 0; m.inBandChans = 0;
    if (inputRate <= 0) return;
    for (size_t ci = 0; ci < chans.size(); ci++) {
        const Channel& c = chans[ci];
        const bool inBand = fits(inputRate, c);
        const bool active = inBand && !c.sfs.empty() && !c.syncs.empty() && (c.normalIq || c.invertedIq);
        Impl::Chan* ch = nullptr;
        if (active) {
            m.chans.emplace_back();
            ch = &m.chans.back();
            ch->ch = std::make_unique<lora::Channelizer>(inputRate, c.offsetHz, std::max(c.bwHz, c.filterBwHz));
            ch->index = ci; ch->offsetHz = c.offsetHz;
            ch->buf.clear(0); ch->bufInv.clear(0);
            ch->wantInv = c.invertedIq;
            m.inBandChans++;
        }
        for (int inv = 0; inv < 2; inv++) {
            if (inv ? !c.invertedIq : !c.normalIq) continue;
            for (int sf : c.sfs) for (int sw : c.syncs) {
                DecoderStat st;
                st.chan = ci; st.sf = sf; st.sync = sw; st.inverted = inv != 0; st.inBand = inBand;
                m.stats.push_back(st);
                if (!active) continue;
                lora::Params p;
                p.sf = sf; p.bwHz = c.bwHz; p.syncWord = (uint8_t)sw; p.preamble = c.preamble;
                p.ldro = lora::autoLdro(sf, c.bwHz);
                p.cfoTrack = c.cfoTrack; p.carrierHz = c.carrierHz;
                Impl::Dec d;
                d.dm = std::make_unique<lora::Demod>(p, ch->ch->outRate());
                d.dm->reset(0);
                d.stat = m.stats.size() - 1; d.sf = sf; d.sync = sw; d.inverted = inv != 0;
                ch->decs.push_back(std::move(d));
                m.running++;
            }
        }
    }
}

void Bank::feed(const cf32* x, size_t n, std::vector<Decoded>& out) {
    Impl& m = *p_;
    if (m.rate <= 0 || n == 0) return;
    if (m.chans.empty()) { m.nProc += (int64_t)n; return; }
    size_t i = 0;
    if (!m.pend.empty()) {
        const size_t take = std::min(n, kBlock - m.pend.size());
        m.pend.insert(m.pend.end(), x, x + take);
        i = take;
        if (m.pend.size() == kBlock) { m.block(m.pend.data(), kBlock, out); m.pend.clear(); }
    }
    for (; i + kBlock <= n; i += kBlock) m.block(x + i, kBlock, out);
    if (i < n) m.pend.insert(m.pend.end(), x + i, x + n);
}

double Bank::timeSec() const { return p_->rate > 0 ? (double)(p_->nProc + (int64_t)p_->pend.size()) / p_->rate : 0; }
int64_t Bank::samples() const { return p_->nProc + (int64_t)p_->pend.size(); }
const std::vector<DecoderStat>& Bank::stats() const { return p_->stats; }
size_t Bank::decoders() const { return p_->running; }
size_t Bank::channelsInBand() const { return p_->inBandChans; }

} // namespace lorabank
} // namespace dect2
