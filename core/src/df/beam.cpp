#include "dect2/df/beam.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

namespace dect2::df {

static constexpr double D2R = kPi / 180.0;

std::vector<cd> steeringAE(const Array& arr, double lambda, double az, double el) {
    double a = az * D2R, e = el * D2R, ce = std::cos(e), k = 2.0 * kPi / lambda;
    double ux = std::sin(a) * ce, uy = std::cos(a) * ce, uz = std::sin(e);
    std::vector<cd> v(arr.n());
    for (int i = 0; i < arr.n(); i++) {
        double z = i < (int)arr.z.size() ? arr.z[i] : 0.0;
        v[i] = std::polar(1.0, k * (arr.pos[i][0] * ux + arr.pos[i][1] * uy + z * uz));
    }
    return v;
}

std::vector<cd> delaySumWeights(const Array& arr, double lambda, Direction look) {
    auto a = steeringAE(arr, lambda, look.azDeg, look.elDeg);
    for (auto& z : a) z /= (double)arr.n();       // w^H a = 1
    return a;
}

// w = R^-1 C (C^H R^-1 C)^-1 f, constraints in the columns of C, f = [1 0 0 ...]
static std::vector<cd> constrained(const CMat& r0, const std::vector<std::vector<cd>>& c, double loading, const std::vector<cd>& fallback) {
    int n = r0.n, m = (int)c.size();
    CMat r = r0;
    double tr = 0;
    for (int i = 0; i < n; i++) tr += r(i, i).real();
    double dl = std::max(loading, 1e-9) * tr / std::max(1, n);
    if (!(dl > 0)) dl = 1e-9;
    for (int i = 0; i < n; i++) r(i, i) += dl;
    std::vector<std::vector<cd>> z(m);
    for (int j = 0; j < m; j++) if (!solve(r, c[j], z[j])) return fallback;
    CMat g(m);
    for (int i = 0; i < m; i++) for (int j = 0; j < m; j++) { cd s = 0; for (int k = 0; k < n; k++) s += std::conj(c[i][k]) * z[j][k]; g(i, j) = s; }
    std::vector<cd> f(m, 0), y;
    f[0] = 1;
    if (!solve(g, f, y)) return fallback;
    std::vector<cd> w(n, 0);
    for (int k = 0; k < n; k++) for (int j = 0; j < m; j++) w[k] += z[j][k] * y[j];
    return w;
}

std::vector<cd> mvdrWeights(const CMat& r, const Array& arr, double lambda, Direction look, double loading) {
    if (r.n != arr.n()) return delaySumWeights(arr, lambda, look);
    return constrained(r, {steeringAE(arr, lambda, look.azDeg, look.elDeg)}, loading, delaySumWeights(arr, lambda, look));
}

std::vector<cd> lcmvWeights(const CMat& r, const Array& arr, double lambda, Direction look, const std::vector<Direction>& nulls, double loading) {
    if (r.n != arr.n()) return delaySumWeights(arr, lambda, look);
    std::vector<std::vector<cd>> c{steeringAE(arr, lambda, look.azDeg, look.elDeg)};
    for (size_t i = 0; i < nulls.size() && (int)c.size() < arr.n(); i++) c.push_back(steeringAE(arr, lambda, nulls[i].azDeg, nulls[i].elDeg));
    return constrained(r, c, loading, delaySumWeights(arr, lambda, look));
}

std::vector<cd> makeWeights(const BeamSpec& s, const CMat& r, const Array& arr, double lambda) {
    switch (s.type) {
    case BeamType::DelaySum: return delaySumWeights(arr, lambda, s.look);
    case BeamType::Mvdr: return mvdrWeights(r, arr, lambda, s.look, s.loading);
    case BeamType::Lcmv: return lcmvWeights(r, arr, lambda, s.look, s.nulls, s.loading);
    }
    return delaySumWeights(arr, lambda, s.look);
}

double beamResponse(const std::vector<cd>& w, const Array& arr, double lambda, Direction d) {
    auto a = steeringAE(arr, lambda, d.azDeg, d.elDeg);
    cd s = 0;
    for (size_t i = 0; i < w.size(); i++) s += std::conj(w[i]) * a[i];
    return std::norm(s);
}

std::vector<double> beamPattern(const std::vector<cd>& w, const Array& arr, double lambda, double el, double step) {
    std::vector<double> p;
    for (double az = 0; az < 360.0 - 1e-9; az += step) p.push_back(10 * std::log10(std::max(beamResponse(w, arr, lambda, {az, el}), 1e-30)));
    return p;
}

double whiteNoiseGainDb(const std::vector<cd>& w) {
    double s = 0;
    for (auto& z : w) s += std::norm(z);
    return s > 0 ? -10 * std::log10(s) : 0;
}

double nullDepthDb(const std::vector<cd>& w, const Array& arr, double lambda, Direction look, Direction d) {
    double l = beamResponse(w, arr, lambda, look), n = beamResponse(w, arr, lambda, d);
    return 10 * std::log10(std::max(n, 1e-30) / std::max(l, 1e-30));
}

double beamwidthDeg(const std::vector<cd>& w, const Array& arr, double lambda, Direction look) {
    double l = beamResponse(w, arr, lambda, look), lim = l * 0.5, up = 360, dn = 360;
    for (double d = 0.25; d <= 180; d += 0.25) {
        if (up == 360 && beamResponse(w, arr, lambda, {look.azDeg + d, look.elDeg}) < lim) up = d;
        if (dn == 360 && beamResponse(w, arr, lambda, {look.azDeg - d, look.elDeg}) < lim) dn = d;
        if (up != 360 && dn != 360) break;
    }
    return (up == 360 || dn == 360) ? 360 : up + dn;
}

bool strongestPeak(const CMat& r, const Array& arr, double lambda, const Options& opt, double& az) {
    Options o = opt;
    o.compassOffsetDeg = 0;
    Result res = estimate(r, arr, lambda, o);
    if (res.peaks.empty()) return false;
    az = wrap360(res.bearingDeg);
    return true;
}

BeamBank::BeamBank(BeamBankConfig c) : cfg_(std::move(c)), r_((int)cfg_.array.pos.size()) {
    if ((int)cfg_.beams.size() > kMaxBeams) cfg_.beams.resize(kMaxBeams);
    if (cfg_.beams.empty()) cfg_.beams.push_back(BeamSpec{});
    recompute();
}

void BeamBank::setBeam(int i, const BeamSpec& s) {
    if (i < 0 || i >= kMaxBeams) return;
    if (i >= (int)cfg_.beams.size()) cfg_.beams.resize(i + 1);
    cfg_.beams[i] = s;
    recompute();
}

void BeamBank::recompute() {
    const int n = cfg_.array.n();
    CMat r = r_;
    if (!haveR_) { r = CMat(n); for (int i = 0; i < n; i++) r(i, i) = 1; }
    w_.resize(cfg_.beams.size());
    for (size_t b = 0; b < cfg_.beams.size(); b++) {
        auto& s = cfg_.beams[b];
        if (s.autoSteer && haveR_) { double az; if (strongestPeak(r, cfg_.array, cfg_.wavelength, cfg_.df, az)) s.look.azDeg = az; }
        w_[b] = makeWeights(s, r, cfg_.array, cfg_.wavelength);
    }
}

std::vector<std::vector<cd>> BeamBank::process(const Frame& x) {
    std::vector<std::vector<cd>> out(cfg_.beams.size());
    if ((int)x.size() != cfg_.array.n() || x[0].empty()) return out;
    Frame y = x;
    if ((int)cfg_.calibration.size() == cfg_.array.n()) applyCalibration(y, cfg_.calibration);
    CMat r = dect2::df::covariance(y);
    if (haveR_ && cfg_.covForget > 0) {
        double f = cfg_.covForget;
        for (size_t i = 0; i < r.a.size(); i++) r.a[i] = f * r_.a[i] + (1 - f) * r.a[i];
    }
    r_ = r;
    haveR_ = true;
    recompute();
    for (size_t b = 0; b < out.size(); b++) out[b] = beamform(y, w_[b]);
    return out;
}

BeamIqSource::BeamIqSource(std::shared_ptr<BeamBank> bank, std::shared_ptr<MultiChannelSource> src, int beam)
    : bank_(std::move(bank)), src_(std::move(src)), beam_(beam) {}

bool BeamIqSource::start(const TuneSettings&, IqRing& ring, std::string& err) {
    if (!bank_ || !src_ || beam_ < 0 || beam_ >= bank_->beams()) { err = "no such beam"; return false; }
    if (src_->channels() != bank_->config().array.n()) { err = "the array has a different number of channels than the source"; return false; }
    stop();
    run_ = true;
    th_ = std::thread([this, &ring] {
        Frame x;
        std::vector<cf32> buf;
        while (run_) {
            if (!src_->read(x, 4096)) break;
            auto out = bank_->process(x);
            if (beam_ >= (int)out.size() || out[beam_].empty()) continue;
            buf.resize(out[beam_].size());
            for (size_t i = 0; i < buf.size(); i++) buf[i] = cf32((float)out[beam_][i].real(), (float)out[beam_][i].imag());
            while (run_ && ring.capacity() - ring.available() < buf.size()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (!run_) break;
            ring.write(buf.data(), buf.size());
        }
        run_ = false;
    });
    return true;
}

void BeamIqSource::stop() {
    run_ = false;
    if (th_.joinable()) th_.join();
}

}   // namespace dect2::df
