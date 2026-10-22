// HamSlotTx: one ham burst through a radio. Order on every path: gate (hamTxAllowed), burst length, duty-cycle limit, the radio's prepare() (it
// tunes to the frequency that was checked), audio start, PTT on, audio, drain, PTT off. Any error stops the burst and releases PTT; every attempt,
// refused or sent, goes to the TX audit log (ham_safety.h).
#include "dect2/ham/ham.h"
#include "dect2/ham/ham_safety.h"
#include "dect2/hfdig/hfdig_ftx.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

namespace dect2::ham {

namespace {
using Steady = std::chrono::steady_clock;
double sinceSec(Steady::time_point t) { return std::chrono::duration<double>(Steady::now() - t).count(); }
constexpr double kStallSec = 3.0;   // no audio taken for this long: the output is gone, the burst stops
}  // namespace

bool HamSlotTx::busy() const { return busy_; }

double HamSlotTx::now() const {
    if (clock_) return clock_();
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}

double HamSlotTx::onAirSec(int mode) const {
    std::lock_guard<std::mutex> l(dutyMu_);
    return hamDutyUsed(dutyLog_[mode < 0 ? 1 : 0], now(), hamDutyLimit(mode).windowSec);
}

bool HamSlotTx::pttSet(bool on, std::string& err) {
    std::lock_guard<std::mutex> l(mu_);
    if (pttOn_ == on) return true;
    if (!io_.ptt(on, err)) {
        if (!on) pttOn_ = false;
        else { std::string e; io_.ptt(false, e); }   // a half-done key-up is undone
        return false;
    }
    pttOn_ = on;
    return true;
}

void HamSlotTx::abort() {
    abort_ = true;
    io_.flushAudio();
    std::string e;
    pttSet(false, e);   // released at once, from the caller's thread
}

bool HamSlotTx::waitForSlot(double startUtc, std::string& err, double lateSec) {
    err.clear();
    abort_ = false;
    const double target = startUtc - std::max(0.0, io_.latencySec());
    for (;;) {
        if (abort_) { err = "halted"; return false; }
        const double t = now();
        if (t > startUtc + lateSec) { err = "too late for this slot"; return false; }
        if (t >= target) return true;
        std::this_thread::sleep_for(std::chrono::duration<double>(std::min(0.02, target - t)));
    }
}

bool HamSlotTx::send(const HamTxCheck& gateIn, int mode, const std::string& msg, double audioHz, double level, std::string& err, int sub) {
    err.clear();
    if (busy_.exchange(true)) { err = "a transmission is already running"; return false; }
    struct Done { std::atomic<bool>& b; ~Done() { b = false; } } done{busy_};
    abort_ = false;
    HamTxCheck gate = gateIn;
    gate.audioHz = audioHz;
    gate.bwHz = std::max(gate.bwHz, hamModeBwHz(mode, sub));
    TxAuditEntry au;
    au.utc = now(); au.path = io_.kind(); au.mode = hamModeName(mode); au.message = msg; au.call = gate.call;
    au.dialHz = gate.dialHz; au.audioHz = audioHz; au.bwHz = gate.bwHz;
    auto refuse = [&](const std::string& why) { err = why; au.result = "refused"; au.reason = why; txAuditAdd(au); return false; };
    std::string why;
    if (!hamTxAllowed(gate, why)) return refuse(why);
    if (!(mode >= 0 && mode <= 5) && mode != 10) return refuse("not a mode the ham transmitter sends");
    const int rate = 48000;
    std::vector<float> a((size_t)(slotPeriod(mode) * rate) + rate, 0.f);
    const double amp = level < 0.05 ? 0.05 : level > 0.9 ? 0.9 : level;
    if (!ftxAddTransmissionEx(mode, msg, audioHz, amp, rate, 0.0, false, sub, 0, a)) return refuse("the message cannot be sent: " + msg);
    size_t n = a.size();
    while (n > 0 && a[n - 1] == 0.f) n--;
    if (n == 0) return refuse("empty audio");
    return run(gate, mode, au.mode, msg, a, n, err);
}

bool HamSlotTx::sendAudio(const HamTxCheck& gateIn, std::vector<float> a, double bwHz, double level, std::string& err, const std::string& label) {
    err.clear();
    if (busy_.exchange(true)) { err = "a transmission is already running"; return false; }
    struct Done { std::atomic<bool>& b; ~Done() { b = false; } } done{busy_};
    abort_ = false;
    HamTxCheck gate = gateIn;
    gate.bwHz = std::max(gate.bwHz, bwHz);
    TxAuditEntry au;
    au.utc = now(); au.path = io_.kind(); au.mode = label; au.call = gate.call;
    au.dialHz = gate.dialHz; au.audioHz = gate.audioHz; au.bwHz = gate.bwHz;
    auto refuse = [&](const std::string& why) { err = why; au.result = "refused"; au.reason = why; txAuditAdd(au); return false; };
    std::string why;
    if (!hamTxAllowed(gate, why)) return refuse(why);
    float pk = 0;
    for (float v : a) if (std::isfinite(v)) pk = std::max(pk, std::fabs(v));
    if (a.empty() || pk <= 0) return refuse("empty audio");
    const float amp = (float)(level < 0.05 ? 0.05 : level > 0.9 ? 0.9 : level);
    const float g = amp / pk;
    for (auto& v : a) v = std::isfinite(v) ? v * g : 0.f;
    return run(gate, -1, label, std::string(), a, a.size(), err);
}

bool HamSlotTx::run(const HamTxCheck& gate, int dutyMode, const std::string& label, const std::string& msg, const std::vector<float>& a, size_t n,
                    std::string& err) {
    const int rate = 48000;
    const double dur = (double)n / rate, t0 = now();
    TxAuditEntry au;
    au.utc = t0; au.path = io_.kind(); au.mode = label; au.message = msg; au.call = gate.call;
    au.dialHz = gate.dialHz; au.audioHz = gate.audioHz; au.bwHz = gate.bwHz;
    auto refuse = [&](const std::string& why) { err = why; au.result = "refused"; au.reason = why; txAuditAdd(au); return false; };
    const HamDutyLimit lim = hamDutyLimit(dutyMode);
    if (dur > lim.maxBurstSec + 0.5) {
        char b[120];
        snprintf(b, sizeof b, "the burst is %.0f s long, the limit for this mode is %.0f s", dur, lim.maxBurstSec);
        return refuse(b);
    }
    {
        std::lock_guard<std::mutex> l(dutyMu_);
        auto& log = dutyLog_[dutyMode < 0 ? 1 : 0];
        log.erase(std::remove_if(log.begin(), log.end(), [&](const std::pair<double, double>& b) { return b.first + b.second < t0 - 3600; }), log.end());
        const double used = hamDutyUsed(log, t0, lim.windowSec);
        if (used + dur > lim.maxFraction * lim.windowSec) {
            char b[160];
            snprintf(b, sizeof b, "duty cycle limit: %.0f s on air in the last %.0f minutes (at most %.0f %%), wait before sending more", used,
                     lim.windowSec / 60, lim.maxFraction * 100);
            return refuse(b);
        }
    }
    if (abort_) return refuse("halted");
    std::string why;
    if (!io_.prepare(gate, why)) return refuse(why.empty() ? "the radio refused the transmission" : why);
    size_t played = 0;
    const bool ok = play(a, n, rate, err, played);
    const double onAir = (double)played / rate;
    {
        std::lock_guard<std::mutex> l(dutyMu_);
        if (played > 0) dutyLog_[dutyMode < 0 ? 1 : 0].push_back({t0, onAir});
    }
    au.seconds = onAir;
    au.result = ok ? "sent" : err == "halted" ? "halted" : "failed";
    au.reason = ok ? std::string() : err;
    txAuditAdd(au);
    return ok;
}

bool HamSlotTx::play(const std::vector<float>& a, size_t n, int rate, std::string& err, size_t& played) {
    played = 0;
    if (!io_.beginAudio(rate, err)) return false;
    if (abort_) { io_.endAudio(); err = "halted"; return false; }
    if (!pttSet(true, err)) { io_.endAudio(); return false; }
    size_t pos = 0;
    bool ok = true;
    auto lastProgress = Steady::now();
    while (pos < n && !abort_) {
        const int w = io_.write(a.data() + pos, (int)std::min<size_t>(n - pos, 4800));
        if (w < 0) { ok = false; err = io_.lastError(); if (err.empty()) err = "the audio output failed"; break; }
        if (w > 0) { pos += (size_t)w; lastProgress = Steady::now(); continue; }
        if (sinceSec(lastProgress) > kStallSec) { ok = false; err = "the audio output stalled"; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (ok && !abort_) {
        const auto drain0 = Steady::now();
        const double limit = (double)std::max(0, io_.pending()) / rate + kStallSec;
        while (!abort_ && io_.pending() > 0) {
            if (sinceSec(drain0) > limit) { ok = false; err = "the audio output did not finish playing"; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    if (!ok || abort_) io_.flushAudio();
    if (abort_ && ok) { ok = false; err = "halted"; }
    std::string e2;
    pttSet(false, e2);
    io_.endAudio();
    if (!e2.empty() && ok) { err = e2; ok = false; }
    played = std::min(pos, n);
    return ok;
}

}  // namespace dect2::ham
