// kaalvoice_bench: objective comparison of the KAAL voice codecs (KV4 profiles against the old KaalCodec3 / KaalCodec) on the repo's
// speech sample and on synthetic speech, plus the real-time factor. No listening test: the numbers are SNR / spectral distances only.
//
//   kaalvoice_bench [--wav file.wav]... [--out dir] [--quick] [--lsfstats]
// --out writes every decoded signal as a WAV next to the reference, --lsfstats prints the LSF statistics the quantiser is built on.
#include "dect2/kaalvoice/kaal_voice.h"
#include "dect2/kaalvoice/kaal_voice_eval.h"
#include "../core/src/kaalvoice/kv_internal.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace dect2::kaalvoice;

namespace {
struct Sig { std::string name; std::vector<int16_t> pcm; int fs; };
struct Codec { std::string name; CodecId id; Config cfg; };

std::vector<int16_t> run(FrameCodec& c, const std::vector<int16_t>& x, double* cpuSec) {
    const Info& inf = c.info();
    const int n = inf.frameSamples;
    std::vector<int16_t> y;
    std::vector<int16_t> frame((size_t)n), out((size_t)n);
    std::vector<uint8_t> bits;
    const auto t0 = std::chrono::steady_clock::now();
    const size_t total = x.size() + (size_t)inf.delaySamples + (size_t)n;
    for (size_t pos = 0; pos < total; pos += (size_t)n) {
        for (int i = 0; i < n; i++) frame[(size_t)i] = pos + i < x.size() ? x[pos + i] : 0;
        const FrameType t = c.encode(frame.data(), bits);
        c.decode(t, bits.empty() ? nullptr : bits.data(), bits.size(), out.data());
        y.insert(y.end(), out.begin(), out.end());
    }
    *cpuSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return y;
}

void lsfStats(const std::vector<Sig>& sigs) {
    using namespace dect2::kaalvoice::detail;
    double sum[kP] = {}, sum2[kP] = {}, rs[kP] = {}, rs2[kP] = {};
    int n = 0;
    static float win[280];
    for (int i = 0; i < 240; i++) win[i] = (float)(0.54 - 0.46 * std::cos(kPi * i / 239));
    for (int i = 240; i < 280; i++) win[i] = (float)std::cos(0.5 * kPi * (i - 239) / 41.0);
    std::vector<std::vector<double>> all;
    for (const Sig& s : sigs) {
        const std::vector<int16_t> x = s.fs == 8000 ? s.pcm : eval::downsample2(s.pcm);
        std::vector<double> prev;
        for (size_t p = 0; p + 280 <= x.size(); p += 160) {
            float f[280];
            double e = 0;
            for (int i = 0; i < 280; i++) { f[i] = x[p + i]; e += (double)f[i] * f[i]; }
            if (e / 280 < 1e4) { prev.clear(); continue; }   // skip silence
            double r[kP + 1], lsf[kP];
            float a[kP + 1];
            autocorr(f, win, 280, kP, r);
            lagWindow(r, kP);
            if (!levinson(r, kP, a) || !lpcToLsf(a, lsf)) continue;
            all.push_back(std::vector<double>(lsf, lsf + kP));
            for (int i = 0; i < kP; i++) { sum[i] += lsf[i]; sum2[i] += lsf[i] * lsf[i]; }
            n++;
            prev.assign(lsf, lsf + kP);
        }
    }
    double mu[kP];
    printf("LSF statistics over %d active frames\nmean :", n);
    for (int i = 0; i < kP; i++) { mu[i] = sum[i] / n; printf(" %.3f", mu[i]); }
    printf("\nstd  :");
    for (int i = 0; i < kP; i++) printf(" %.3f", std::sqrt(sum2[i] / n - mu[i] * mu[i]));
    for (double rho : {0.45, 0.6}) {
        int m = 0;
        for (int i = 0; i < kP; i++) rs[i] = rs2[i] = 0;
        for (size_t k = 1; k < all.size(); k++)
            for (int i = 0; i < kP; i++) {
                const double v = all[k][i] - mu[i] - rho * (all[k - 1][i] - mu[i]);
                rs[i] += v; rs2[i] += v * v; if (i == 0) m++;
            }
        printf("\nres std (rho %.2f):", rho);
        for (int i = 0; i < kP; i++) printf(" %.3f", std::sqrt(rs2[i] / m));
    }
    printf("\n");
}
}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> wavs;
    std::string outDir;
    bool quick = false, stats = false, synth = true;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--wav") && i + 1 < argc) wavs.push_back(argv[++i]);
        else if (!std::strcmp(argv[i], "--out") && i + 1 < argc) outDir = argv[++i];
        else if (!std::strcmp(argv[i], "--quick")) quick = true;
        else if (!std::strcmp(argv[i], "--lsfstats")) stats = true;
        else if (!std::strcmp(argv[i], "--nosynth")) synth = false;
    }
    std::vector<Sig> sigs;
    for (const std::string& w : wavs) {
        Sig s;
        if (!eval::readWav(w, s.pcm, s.fs)) { fprintf(stderr, "cannot read %s\n", w.c_str()); return 1; }
        if (s.fs == 8000) { s.pcm = eval::upsample2(s.pcm); s.fs = 16000; }
        if (s.fs != 16000) { fprintf(stderr, "%s: need 8 or 16 kHz\n", w.c_str()); return 1; }
        const size_t sl = w.find_last_of('/');
        s.name = sl == std::string::npos ? w : w.substr(sl + 1);
        sigs.push_back(s);
    }
    const double secs = quick ? 2.0 : 4.0;
    const char* vn[4] = {"synth-male", "synth-female", "synth-child", "synth-lowmale"};
    for (int v = 0; synth && v < 4; v++) sigs.push_back({vn[v], eval::synthSpeech(16000, v, secs, 11 + v), 16000});
    if (synth) sigs.push_back({"synth-male+noise15dB", eval::synthSpeech(16000, 0, secs, 21, -15.0), 16000});
    if (stats) { lsfStats(sigs); return 0; }

    std::vector<Codec> codecs;
    auto kv = [&](Profile p, int ms, int fs = 16000) {
        Config c; c.profile = p; c.frameMs = ms; c.sampleRate = fs; c.dtx = false;
        codecs.push_back({std::string("KV4 ") + profileName(p) + "k/" + std::to_string(ms) + "ms" + (fs == 8000 ? "/8k" : ""), CodecId::KV4, c});
    };
    kv(Profile::K3_2, 40); kv(Profile::K4_8, 40); kv(Profile::K6_0, 40); kv(Profile::K8_0, 40);
    kv(Profile::K4_8, 20); kv(Profile::K6_0, 20); kv(Profile::K8_0, 20); kv(Profile::HQ12, 20); kv(Profile::HQ16, 20);
    kv(Profile::K6_0, 40, 8000);
    { Config c; c.payloadBits = 240; codecs.push_back({"old v3 12k", CodecId::LegacyV3, c}); }
    { Config c; c.payloadBits = 320; codecs.push_back({"old v3 16k", CodecId::LegacyV3, c}); }
    { Config c; codecs.push_back({"old v1 16k", CodecId::LegacyV1, c}); }

    printf("%-22s %-22s %7s %7s %7s %7s %6s %6s %6s %6s %7s\n", "codec", "signal", "SNR", "segSNR", "SNRlo", "segSNRlo", "LSD", "LSDlo", "LSDhi", "BLD", "RTF");
    for (const Codec& cd : codecs) {
        double aSnr = 0, aSeg = 0, aLsd = 0, aBld = 0, aRtf = 0, aSl = 0, aSsl = 0, aLl = 0, aLh = 0;
        for (const Sig& s : sigs) {
            std::unique_ptr<FrameCodec> c = makeFrameCodec(cd.id, cd.cfg);
            if (!c) { printf("%-22s unsupported\n", cd.name.c_str()); break; }
            const int fs = cd.cfg.sampleRate;
            const std::vector<int16_t> ref = fs == 8000 ? eval::downsample2(s.pcm) : s.pcm;
            double cpu = 0;
            const std::vector<int16_t> y = run(*c, ref, &cpu);
            const eval::Metrics m = eval::compare(ref, y, fs, c->info().delaySamples);
            const double rtf = cpu / ((double)ref.size() / fs);
            printf("%-22s %-22s %7.2f %7.2f %7.2f %7.2f %6.2f %6.2f %6.2f %6.3f %7.4f\n", cd.name.c_str(), s.name.c_str(), m.snr, m.segSnr, m.snrLow,
                   m.segSnrLow, m.lsd, m.lsdLow, m.lsdHigh, m.bld, rtf);
            aSnr += m.snr; aSeg += m.segSnr; aLsd += m.lsd; aBld += m.bld; aRtf += rtf; aSl += m.snrLow; aSsl += m.segSnrLow; aLl += m.lsdLow; aLh += m.lsdHigh;
            if (!outDir.empty()) {
                std::string fn = outDir + "/" + cd.name + "_" + s.name + ".wav";
                for (char& ch : fn) if (ch == ' ') ch = '_';
                std::vector<int16_t> al(y.begin() + std::min(y.size(), (size_t)c->info().delaySamples), y.end());
                eval::writeWav(fn, al, fs);
            }
        }
        const double k = (double)sigs.size();
        printf("%-22s %-22s %7.2f %7.2f %7.2f %7.2f %6.2f %6.2f %6.2f %6.3f %7.4f\n\n", cd.name.c_str(), "MEAN", aSnr / k, aSeg / k, aSl / k, aSsl / k,
               aLsd / k, aLl / k, aLh / k, aBld / k, aRtf / k);
    }
    return 0;
}
