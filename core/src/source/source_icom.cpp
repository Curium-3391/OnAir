// Native driver for the Icom IC-R8600 [I/Q OUT] port (Cypress FX2 over USB). RECEIVE ONLY. NOT tested on a real radio (no R8600 here): the plain
// logic (marker decoder, CI-V, firmware conversion) is unit-tested in tests/source/test_icom_r8600.cpp, the radio itself needs a user test.
//
// Written again in C++ from the MIT-licensed work of Pawel "Topol" Topolski ("Topol in collaboration with AI": the SoapySDR module and the macOS
// loader) and Pieter Ibelings' ibelinp/ic-r8600-usb-iq (MIT), see THIRD_PARTY.md. What it does, and only this: libusb to the radio, CI-V commands
// over the radio's own bulk endpoints, and reading the firmware file the user imported. It starts no program and opens no serial port.
//
// - The FX2 has no firmware of its own: it enumerates as 0c26:0022 until Icom's firmware (icom_r8600.h) is written into its RAM with vendor
//   requests 0xA0 (the first holds the CPU, the last releases it), then as 0c26:0023. OnAir never ships the firmware; without it the radio is
//   listed as "needs Icom firmware, see ?".
// - Samples: 16-bit I/Q on bulk IN 0x86 with Icom's marker word (IqDecoder). CI-V on bulk OUT 2 / IN 0x88.
// - macOS and Linux: libusb. Windows: libusb needs the WinUSB driver for 0c26:0022 and 0c26:0023 (Zadig); Icom's own driver does not work with it.
#include "dect2/source/icom_r8600.h"
#include "native_common.h"
#include <libusb.h>
#include <map>

namespace dect2 {
namespace icom {

std::vector<UsbSeen> findUsb() {
    std::vector<UsbSeen> out;
    libusb_context* ctx = nullptr;
    if (libusb_init(&ctx) != 0) return out;
    libusb_device** list = nullptr;
    const ssize_t n = libusb_get_device_list(ctx, &list);
    for (ssize_t i = 0; i < n; i++) {
        libusb_device_descriptor d{};
        if (libusb_get_device_descriptor(list[i], &d) != 0 || !isIcomIq(d.idVendor, d.idProduct)) continue;
        UsbSeen u;
        u.vid = d.idVendor;
        u.pid = d.idProduct;
        u.bus = libusb_get_bus_number(list[i]);
        u.address = libusb_get_device_address(list[i]);
        uint8_t ports[8];
        const int np = libusb_get_port_numbers(list[i], ports, sizeof ports);
        if (np > 0) for (int k = 0; k < np; k++) u.ports += (k ? "-" : "") + std::to_string(ports[k]);
        else u.ports = "a" + std::to_string(u.address);   // no route known (some Windows set-ups): the address, valid until the next re-enumeration
        out.push_back(u);
    }
    if (list) libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    return out;
}

} // namespace icom

namespace native {

using namespace icom;

namespace {

constexpr int kCtlTimeoutMs = 2000;
constexpr size_t kMaxTransfer = 256 * 1024;
constexpr int kQueued = 12;

std::vector<RadioSetting> icomSettings() {
    std::vector<RadioSetting> v;
    RadioSetting rf;
    rf.type = RadioSetting::Number;
    rf.key = "rfgain";
    rf.label = "Icom RF gain (radio units, not dB)";
    rf.help = "The radio's own RF gain, 0 to 255 in Icom units (NOT dB; 255 is the maximum). Lowering it can make the I/Q output weak. The Gain slider "
              "in the toolbar is a separate digital gain inside OnAir. Sent when the radio starts and when changed.";
    rf.minV = 0; rf.maxV = 255; rf.step = 1; rf.def = "255";
    v.push_back(rf);
    const char* note = " \"Leave as is\" stops OnAir changing it; it does not undo an earlier change. Stopping does not restore the radio's earlier values.";
    v.push_back(choiceSetting("att", "Icom attenuator", std::string("The radio's attenuator ahead of the receiver. Use it on strong signals or overload: software gain cannot repair a clipped converter.") + note,
                              {"keep", "0", "10", "20", "30"}, {"Leave as is", "Off (0 dB)", "10 dB", "20 dB", "30 dB"}, "keep"));
    v.push_back(choiceSetting("preamp", "Icom preamplifier", std::string("The radio's preamplifier. On can help weak signals and can worsen overload from strong ones.") + note,
                              {"keep", "0", "1"}, {"Leave as is", "Off", "On"}, "keep"));
    v.push_back(choiceSetting("ipplus", "Icom IP+", std::string("The radio's IP+ (intercept point) setting; what it does depends on the band.") + note,
                              {"keep", "0", "1"}, {"Leave as is", "Off", "On"}, "keep"));
    v.push_back(choiceSetting("hfant", "Icom antenna below 30 MHz", std::string("Which antenna socket is used below 30 MHz. Above that the radio uses ANT1.") + note,
                              {"keep", "1", "2", "3"}, {"Leave as is", "ANT1 (N)", "ANT2 (SO-239)", "ANT3 (RCA)"}, "keep"));
    v.push_back(choiceSetting("hfbpf", "Icom HF band-pass filter", std::string("The HF band-pass filter for the I/Q output. On is how the first working module started the radio. Matters for HF.") + note,
                              {"keep", "0", "1"}, {"Leave as is", "Off", "On"}, "1"));
    return v;
}

const char* const kKeys[] = {"rfgain", "att", "hfant", "preamp", "ipplus", "hfbpf"};   // the order the radio is set up in

class IcomSource : public NativeSource {
public:
    IcomSource(std::string identity) : identity_(std::move(identity)) {}
    ~IcomSource() override { stop(); }

protected:
    bool openDevice(const TuneSettings& s, std::string& err) override {
        std::lock_guard<std::mutex> lk(api_);
        if (h_) return true;
        streamState_ = 0;
        const std::vector<UsbSeen> all = findUsb();
        const UsbSeen* hit = nullptr;
        for (const auto& u : all) if (usbIdentity(u) == identity_) { hit = &u; break; }
        if (!hit && all.size() == 1) hit = &all[0];   // (the route can change when the radio is moved)
        if (!hit) { err = "Icom IC-R8600 I/Q port not found (unplugged? connect the I/Q OUT USB port)"; return false; }
        std::string ports = hit->ports;
        if (isLoader(*hit)) {
            if (!uploadFirmware(*hit, ports, err)) return false;
        }
        if (!openStreaming(ports, err)) return false;
        return configure0(s, err);
    }
    bool configure(const TuneSettings& s, std::string& err, bool live) override {
        std::lock_guard<std::mutex> lk(api_);
        if (!live) streamState_ = 0;
        return configure0(s, err, live);
    }
    void closeDevice() override {
        std::lock_guard<std::mutex> lk(api_);
        if (!h_) return;
        if (entered_) leave();
        libusb_release_interface(h_, 0);
        libusb_close(h_);
        h_ = nullptr;
        if (ctx_) libusb_exit(ctx_);
        ctx_ = nullptr;
    }
    void interrupt() override { if (ctx_) libusb_interrupt_event_handler(ctx_); }
    bool waitForStreamStart(std::string& err) override {
        for (int i = 0; i < 100; i++) {   // up to 5 s for the CI-V start-up and the first transfers
            const int st = streamState_.load();
            if (st > 0) return true;
            if (st < 0) { std::lock_guard<std::mutex> lk(api_); err = failMsg_; return false; }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        err = "Icom IC-R8600: the radio did not start streaming within 5 s";
        return false;
    }

    void streamLoop() override {
        libusb_context* ctx;
        libusb_device_handle* h;
        double rate;
        {
            std::lock_guard<std::mutex> lk(api_);
            if (!h_ || !run_) return;
            ctx = ctx_;
            h = h_;
            rate = rate_;
            std::string e;
            if (!enter(e)) { fail(e); return; }
        }
        decoder_.reset();
        struct State {
            IcomSource* self;
            std::atomic<int> inflight{0};
            std::atomic<int> failCode{0};
            std::vector<int16_t> iq;
            std::vector<cf32> cf;
        } st;
        st.self = this;
        auto cb = [](libusb_transfer* t) {
            auto* s = static_cast<State*>(t->user_data);
            const bool ok = t->status == LIBUSB_TRANSFER_COMPLETED || t->status == LIBUSB_TRANSFER_TIMED_OUT;   // (a quiet moment is not an error)
            if (ok && s->self->run_.load() && !s->failCode) {
                if (t->actual_length > 0) s->self->consume(t->buffer, (size_t)t->actual_length, s->iq, s->cf);
                if (libusb_submit_transfer(t) == 0) return;
                s->failCode = LIBUSB_ERROR_IO;
            } else if (!ok && t->status != LIBUSB_TRANSFER_CANCELLED && !s->failCode) {
                s->failCode = t->status == LIBUSB_TRANSFER_NO_DEVICE ? LIBUSB_ERROR_NO_DEVICE : LIBUSB_ERROR_IO;
            }
            s->inflight--;
        };
        size_t size = std::max<size_t>(4096, std::min<size_t>(kMaxTransfer, (size_t)(rate * 4 * 0.010)));
        size = (size + 511) & ~(size_t)511;
        std::vector<std::vector<uint8_t>> bufs(kQueued, std::vector<uint8_t>(size));
        std::vector<libusb_transfer*> xfers(kQueued, nullptr);
        for (int i = 0; i < kQueued; i++) {
            xfers[(size_t)i] = libusb_alloc_transfer(0);
            libusb_fill_bulk_transfer(xfers[(size_t)i], h, 0x86, bufs[(size_t)i].data(), (int)size, cb, &st, 250);
        }
        for (int i = 0; i < kQueued && !st.failCode; i++) {
            st.inflight++;
            const int r = libusb_submit_transfer(xfers[(size_t)i]);
            if (r != 0) { st.inflight--; st.failCode = r; }
        }
        if (!st.failCode) {
            streamState_ = 1;
            fprintf(stderr, "Icom IC-R8600: streaming %.3f Msps, %.6f MHz\n", rate / 1e6, freq_ / 1e6);
            fflush(stderr);
        } else {
            std::lock_guard<std::mutex> lk(api_);
            fail(std::string("Icom IC-R8600: the I/Q transfers could not be started (") + usbErrorText(st.failCode) + ")");
        }
        bool cancelled = false;
        while (st.inflight > 0) {
            if (!cancelled && (st.failCode || !run_.load())) {
                for (auto* t : xfers) libusb_cancel_transfer(t);
                cancelled = true;
            }
            timeval tv{0, 50000};
            const int r = libusb_handle_events_timeout_completed(ctx, &tv, nullptr);
            if (r != 0 && r != LIBUSB_ERROR_TIMEOUT && r != LIBUSB_ERROR_INTERRUPTED && !st.failCode) st.failCode = r;
        }
        for (auto* t : xfers) libusb_free_transfer(t);
        {
            std::lock_guard<std::mutex> lk(api_);
            if (entered_ && !st.failCode) leave();   // (after a lost radio the commands could not be sent anyway)
            entered_ = false;
        }
        if (run_ && st.failCode) { fprintf(stderr, "Icom IC-R8600: the stream ended (%s)\n", usbErrorName(st.failCode)); fflush(stderr); }
        else { fprintf(stderr, "Icom IC-R8600: I/Q stopped; markers=%llu phase corrections=%llu\n", (unsigned long long)decoder_.markers, (unsigned long long)decoder_.realignments); fflush(stderr); }
        streamState_ = 0;
    }

private:
    void fail(const std::string& m) {   // (api_ held)
        failMsg_ = m;
        streamState_ = -1;
        fprintf(stderr, "%s\n", m.c_str());
        fflush(stderr);
    }

    // ---- the samples
    void consume(const uint8_t* p, size_t n, std::vector<int16_t>& iq, std::vector<cf32>& cf) {
        iq.clear();
        decoder_.feed(p, n, iq);
        const size_t count = iq.size() / 2;
        if (!count) return;
        const float g = scale_.load() / 32768.f;
        cf.resize(count);
        for (size_t k = 0; k < count; k++) cf[k] = cf32(iq[2 * k] * g, iq[2 * k + 1] * g);
        push(cf.data(), count);
    }

    // ---- CI-V
    bool civ(const std::vector<unsigned char>& payload, std::string& err) {
        std::lock_guard<std::mutex> l(civMu_);
        std::vector<unsigned char> f = civFrame(payload);
        int n = 0;
        int e = libusb_bulk_transfer(h_, 2, f.data(), (int)f.size(), &n, kCtlTimeoutMs);
        if (e || n != (int)f.size()) { err = std::string("Icom IC-R8600: a CI-V command was not sent (") + usbErrorName(e) + ")"; return false; }
        std::vector<unsigned char> all;
        for (int tries = 0; tries < 4; tries++) {
            unsigned char ack[512];
            n = 0;
            e = libusb_bulk_transfer(h_, 0x88, ack, sizeof ack, &n, 1000);
            if (e && e != LIBUSB_ERROR_TIMEOUT) { err = std::string("Icom IC-R8600: no CI-V answer (") + usbErrorName(e) + ")"; return false; }
            if (n > 0) all.insert(all.end(), ack, ack + n);
            const CivReply r = civParseReply(all);
            if (r == CivReply::Ack) return true;
            if (r == CivReply::Nak) { err = "Icom IC-R8600: the radio refused a CI-V command"; return false; }
        }
        err = "Icom IC-R8600: the radio did not acknowledge a CI-V command";
        return false;
    }
    bool applySetting(const std::string& key, const std::string& value, std::string& err) {
        std::vector<unsigned char> p;
        if (!settingPayload(key, value, freq_, p, err)) return false;
        if (!p.empty() && !civ(p, err)) { err = "setting " + key + "=" + value + ": " + err; return false; }
        applied_[key] = value;
        return true;
    }
    // the radio starts streaming (api_ held)
    bool enter(std::string& err) {
        for (const auto& p : civEnterStream()) if (!civ(p, err)) return false;
        entered_ = true;
        if (!civ(civTune(freq_), err)) return false;
        applied_.clear();
        for (const char* k : kKeys) {
            if (!applySetting(k, want_.at(k), err)) return false;
        }
        if (!civ(civSetRate(rate_), err)) return false;
        enteredRate_ = rate_;
        tunedHz_ = freq_;
        return true;
    }
    void leave() {   // (api_ held)
        for (const auto& p : civLeaveStream()) { std::string e; if (!civ(p, e)) { fprintf(stderr, "%s\n", e.c_str()); fflush(stderr); } }
        entered_ = false;
    }

    // what the settings ask for; applied to the radio at once when it is streaming (api_ held)
    bool configure0(const TuneSettings& s, std::string& err, bool live = false) {
        rate_ = chooseRate(s.sampleRate);
        requested_ = s.sampleRate;
        if (s.sampleRate > 0) checkRate(s.sampleRate, err);
        freq_ = s.centerHz;
        if (!(freq_ >= 1) || !std::isfinite(freq_)) { err = "Icom IC-R8600: no valid frequency"; return false; }
        const double dg = std::clamp(s.gainDb, 0.0, 48.0);
        scale_ = std::pow(10.f, (float)dg / 20.f);
        for (const char* k : kKeys) {
            std::string v = radioOption(s, k, defaultSetting(k));
            std::vector<unsigned char> p;
            std::string e;
            if (!settingPayload(k, v, freq_, p, e)) { err = e; return false; }   // refused before anything is sent
            want_[k] = v;
        }
        if (!live || !entered_) return true;
        std::string e;
        if (std::fabs(freq_ - tunedHz_) >= 0.5) {
            const bool crossed = (freq_ >= 30e6) != (tunedHz_ >= 30e6);
            if (!civ(civTune(freq_), e)) { err = e; return false; }
            tunedHz_ = freq_;
            if (crossed || freq_ < 30e6) {   // the HF antenna and filter are set per band
                if (want_.at("hfant") != "keep" && !applySetting("hfant", want_.at("hfant"), e)) { err = e; return false; }
                if (!applySetting("hfbpf", want_.at("hfbpf"), e)) { err = e; return false; }
            }
        }
        for (const char* k : kKeys) {
            auto it = applied_.find(k);
            if (it != applied_.end() && it->second == want_.at(k)) continue;
            if (!applySetting(k, want_.at(k), e)) { err = e; return false; }
        }
        return true;
    }

    // ---- USB
    // The FX2 is in its loader (0c26:0022): write the firmware into its RAM, wait for it to come back as 0c26:0023
    bool uploadFirmware(const UsbSeen& dev, std::string& ports, std::string& err) {
        std::vector<FwRecord> recs;
        std::string path, e;
        if (!loadFirmware(recs, path, e)) {
            err = std::string("Icom IC-R8600: ") + kNeedFirmwareText + (e.find("not found") == std::string::npos ? " (" + e + ")" : std::string());
            openFailure_ = OpenFailure::Final;
            return false;
        }
        {
            libusb_context* ctx = nullptr;
            if (libusb_init(&ctx) != 0) { err = "Icom IC-R8600: libusb could not start"; return false; }
            libusb_device_handle* h = openByRoute(ctx, kPidLoader, dev.ports, err);
            if (!h) { libusb_exit(ctx); return false; }
#ifndef _WIN32
            if (libusb_kernel_driver_active(h, 0) == 1) libusb_detach_kernel_driver(h, 0);
#endif
            bool ok = true;
            const auto t0 = std::chrono::steady_clock::now();
            for (size_t i = 0; i < recs.size() && ok; i++) {
                if (std::chrono::steady_clock::now() - t0 > std::chrono::seconds(7)) { err = "Icom IC-R8600: loading the firmware took too long; unplug and replug the I/Q USB cable"; ok = false; break; }
                std::vector<uint8_t> d = recs[i].data;
                const int r = libusb_control_transfer(h, 0x40, 0xA0, recs[i].wValue, 0, d.data(), (uint16_t)d.size(), 500);
                const bool release = i + 1 == recs.size();   // the release makes the FX2 leave the bus mid-transfer: an error there is expected
                if (r != (int)d.size() && !release) {
                    err = std::string("Icom IC-R8600: the firmware write ") + std::to_string(i) + " failed (" + usbErrorText(r) + ")";
                    if (r == LIBUSB_ERROR_ACCESS || r == LIBUSB_ERROR_NOT_SUPPORTED) openFailure_ = OpenFailure::Final;
                    ok = false;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(3));
            }
            libusb_close(h);
            libusb_exit(ctx);
            if (!ok) return false;
        }
        const int waitMs = kOnWindows ? 15000 : 8000;
        for (int waited = 0; waited < waitMs; waited += 250) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            const std::vector<UsbSeen> now = findUsb();
            for (const auto& u : now) {
                if (u.pid == kPidStream && (u.ports == dev.ports || now.size() == 1)) { ports = u.ports; fprintf(stderr, "Icom IC-R8600: firmware loaded from %s\n", path.c_str()); fflush(stderr); return true; }
            }
        }
        err = "Icom IC-R8600: the firmware was loaded but the radio (0c26:0023) did not come back in time. Unplug the I/Q USB cable, plug it in again and retry."
              + std::string(kOnWindows ? " On Windows both 0c26:0022 and 0c26:0023 need the WinUSB driver (Zadig)." : "");
        return false;
    }
    static libusb_device_handle* openByRoute(libusb_context* ctx, uint16_t pid, const std::string& ports, std::string& err) {
        libusb_device** list = nullptr;
        const ssize_t n = libusb_get_device_list(ctx, &list);
        libusb_device_handle* h = nullptr;
        int lastErr = LIBUSB_ERROR_NO_DEVICE;
        for (ssize_t i = 0; i < n && !h; i++) {
            libusb_device_descriptor d{};
            if (libusb_get_device_descriptor(list[i], &d) != 0 || d.idVendor != kVid || d.idProduct != pid) continue;
            uint8_t pn[8];
            const int np = libusb_get_port_numbers(list[i], pn, sizeof pn);
            std::string route;
            if (np > 0) for (int k = 0; k < np; k++) route += (k ? "-" : "") + std::to_string(pn[k]);
            else route = "a" + std::to_string(libusb_get_device_address(list[i]));
            if (route != ports) continue;
            const int r = libusb_open(list[i], &h);
            if (r != 0) { lastErr = r; h = nullptr; }
        }
        if (list) libusb_free_device_list(list, 1);
        if (!h) {
            char id[16];
            snprintf(id, sizeof id, "0c26:%04x", (unsigned)pid);
            err = std::string("Icom IC-R8600: cannot open the I/Q port ") + id + ": " + usbErrorText(lastErr);
        }
        return h;
    }
    bool openStreaming(const std::string& ports, std::string& err) {
        if (libusb_init(&ctx_) != 0) { ctx_ = nullptr; err = "Icom IC-R8600: libusb could not start"; return false; }
        h_ = openByRoute(ctx_, kPidStream, ports, err);
        if (!h_) {
            libusb_exit(ctx_);
            ctx_ = nullptr;
            openFailure_ = OpenFailure::Unknown;
            return false;
        }
        auto bail = [&](const std::string& m, int code) {
            err = "Icom IC-R8600: " + m + " (" + usbErrorText(code) + ")";
            libusb_close(h_);
            h_ = nullptr;
            libusb_exit(ctx_);
            ctx_ = nullptr;
            if (code == LIBUSB_ERROR_ACCESS || code == LIBUSB_ERROR_NOT_SUPPORTED) openFailure_ = OpenFailure::Final;
            return false;
        };
        int cfg = 0;
        int r = libusb_get_configuration(h_, &cfg);
        if (r != 0) return bail("cannot read the USB configuration", r);
        if (cfg != 1 && (r = libusb_set_configuration(h_, 1)) != 0) return bail("cannot select the USB configuration", r);
#ifndef _WIN32
        if (libusb_kernel_driver_active(h_, 0) == 1) libusb_detach_kernel_driver(h_, 0);
#endif
        if ((r = libusb_claim_interface(h_, 0)) != 0) return bail("the I/Q interface is busy or access was denied", r);
        entered_ = false;
        return true;
    }

    std::string identity_;
    std::mutex api_, civMu_;
    libusb_context* ctx_ = nullptr;
    libusb_device_handle* h_ = nullptr;
    bool entered_ = false;
    double freq_ = 100e6, tunedHz_ = 0, enteredRate_ = 0;
    std::atomic<float> scale_{1.0f};
    std::map<std::string, std::string> want_, applied_;
    IqDecoder decoder_;
    std::atomic<int> streamState_{0};   // 0: not yet, 1: streaming, -1: failed (failMsg_)
    std::string failMsg_;
};

} // namespace

// the radios for the list, from what libusb saw (a pure function, tested without a radio)
void icomDevicesFrom(const std::vector<UsbSeen>& seen, bool haveFirmware, std::vector<DeviceInfo>& out) {
    for (const auto& u : seen) {
        if (!isIcomIq(u.vid, u.pid)) continue;
        DeviceInfo d;
        d.kind = DeviceInfo::Native;
        d.board = "icr8600";
        d.serial = usbIdentity(u);
        d.nativeArgs = d.serial;
        d.name = "Icom IC-R8600 (I/Q)";
        if (isLoader(u) && !haveFirmware) d.name += std::string(" - ") + kNeedFirmwareText;
        else d.name += " (experimental, needs a user test)";
        d.minRateHz = kRates[0];
        d.maxRateHz = kRates[kRateCount - 1];
        for (double r : kRates) d.rateRanges.push_back({r, r});
        d.gainMinDb = 0;
        d.gainMaxDb = 48;
        d.gainNote = "This gain is DIGITAL, inside OnAir (0 to 48 dB). The radio's own RF gain, attenuator and preamp are under Radio settings.";
        d.minFreqHz = kMinHz;
        d.maxFreqHz = kMaxHz;
        d.canTransmit = false;   // a receiver: there is no transmit path
        d.settings = icomSettings();
        out.push_back(d);
    }
}

void listIcom(std::vector<DeviceInfo>& out) {
    if (nativeDisabled()) return;
    const std::vector<UsbSeen> seen = findUsb();
    if (seen.empty()) return;
    icomDevicesFrom(seen, firmwareInstalled(), out);
}

std::unique_ptr<IqSource> makeIcom(const DeviceInfo& d) { return std::make_unique<IcomSource>(d.nativeArgs); }

} // namespace native
} // namespace dect2
