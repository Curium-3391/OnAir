// LoRa receiver benchmark: packet success against SNR (90 % thresholds, with carrier / clock offsets) and CPU cost of the channel bank.
//   lora_bench sweep <sf> <bwKhz> <cr> [cfoFrac of bw/4... as Hz] [sroPpm] [len] [frames] [floorOffsetDb]
//   lora_bench cpu [seconds]
#include "../data/mesh/testutil.h"
#include "dect2/lorawan/lora_bank.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
using namespace dect2;
using namespace meshtest;

static double cpuSec() { timespec t; clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t); return t.tv_sec + 1e-9 * t.tv_nsec; }

static double gRateX = 4;
static double floorFor(int sf) { return -7.5 - 2.5 * (sf - 7); }

static int sweep(int argc, char** argv) {
    const int sf = atoi(argv[2]);
    const double bw = atof(argv[3]) * 1e3;
    const int cr = atoi(argv[4]);
    const double cfo = argc > 5 ? atof(argv[5]) : 0;
    const double sro = argc > 6 ? atof(argv[6]) : 0;
    const size_t len = argc > 7 ? (size_t)atoi(argv[7]) : 20;
    const int frames = argc > 8 ? atoi(argv[8]) : 30;
    const double fo = argc > 9 ? atof(argv[9]) : 0;
    lora::Params p; p.sf = sf; p.bwHz = bw; p.cr = cr; p.preamble = 8; p.ldro = lora::autoLdro(sf, bw);
    double prevSnr = 0, prevFrac = 1, thr = 99;
    const double start = floorFor(sf) + 4 + fo;
    int lowRuns = 0;
    printf("SF%d BW%.0f CR4/%d cfo %.0f sro %.0f len %zu:", sf, bw / 1e3, cr, cfo, sro, len);
    for (double snr = start; snr > start - 14; snr -= 0.5) {
        Trial t;
        t.p = p; t.rate = gRateX * bw; t.off = 0; t.cfoHz = cfo; t.sroPpm = sro; t.snrDb = snr; t.frames = frames; t.len = len;
        t.seed = (uint32_t)(sf * 1000 + (int)((snr + 40) * 2));
        int dec = 0; lora::DemodStats stt;
        const int ok = runTrial(t, nullptr, &dec, &stt);
        const double fr = (double)ok / frames;
        if (getenv("LORA_DIAG")) printf(" %.1f:%d[p%llu s%llu h%llu c%llu]", snr, ok, (unsigned long long)stt.preambles, (unsigned long long)stt.syncBad, (unsigned long long)stt.headerBad, (unsigned long long)stt.crcBad);
        else printf(" %.1f:%d", snr, ok);
        fflush(stdout);
        if (fr < 0.9 && prevFrac >= 0.9 && thr > 90) thr = prevSnr + (prevFrac - 0.9) / (prevFrac - fr + 1e-9) * (snr - prevSnr);
        prevSnr = snr; prevFrac = fr;
        if (fr < 0.2 && ++lowRuns >= 2) break;
    }
    printf("\nTHRESH SF%d BW%.0f CR%d cfo%.0f sro%.0f: %.2f dB (datasheet floor %.1f)\n", sf, bw / 1e3, cr, cfo, sro, thr, floorFor(sf));
    return 0;
}

// noise (plus the unused channels' emptiness) through a bank: CPU seconds per second of signal
static double bankCpu(double rate, const std::vector<lorabank::Channel>& chans, double secs, size_t* decs) {
    std::vector<cf32> x((size_t)(secs * rate));
    genutil::NoiseSource ns(5);
    ns.add(x.data(), x.size(), 0.05f);
    lorabank::Bank b;
    b.configure(rate, chans);
    if (decs) *decs = b.decoders();
    std::vector<lorabank::Decoded> out;
    const double c0 = cpuSec();
    for (size_t i = 0; i < x.size(); i += 8192) b.feed(x.data() + i, std::min<size_t>(8192, x.size() - i), out);
    const double r = (cpuSec() - c0) / secs;
    if (getenv("LORA_DIAG")) { uint64_t pre = 0, sb = 0, hb = 0, fr = 0; for (auto& s : b.stats()) { pre += s.preambles; sb += s.syncBad; hb += s.headerBad; fr += s.frames; } printf("   [noise: preambles %llu syncBad %llu headerBad %llu frames %llu per s]\n", (unsigned long long)(pre / secs), (unsigned long long)(sb / secs), (unsigned long long)(hb / secs), (unsigned long long)(fr / secs)); }
    return r;
}

static int cpu(int argc, char** argv) {
    const double secs = argc > 2 ? atof(argv[2]) : 10;
    // raw LoRa auto mode: 3 bandwidths x SF7..12 x 3 sync words, one channel each at the centre
    {
        std::vector<lorabank::Channel> ch;
        for (double bw : {125e3, 250e3, 500e3}) {
            lorabank::Channel c; c.bwHz = bw; c.sfs = {7, 8, 9, 10, 11, 12}; c.syncs = {0x12, 0x34, 0x2B}; c.preamble = 8; ch.push_back(c);
        }
        size_t d; const double v = bankCpu(2e6, ch, secs, &d);
        printf("CPU raw-auto 2 Msps 3bw x 6sf x 3sync (%zu dec): %.4f\n", d, v);
    }
    for (int sf : {7, 9, 12}) {
        lorabank::Channel c; c.bwHz = 125e3; c.sfs = {sf}; c.syncs = {0x34}; c.preamble = 8;
        size_t d; const double v = bankCpu(1e6, {c}, secs, &d);
        printf("CPU 1 channel SF%d 125k 1 Msps: %.4f\n", sf, v);
    }
    // LoRaWAN-like: 8 channels x SF7..12, one sync
    {
        std::vector<lorabank::Channel> ch;
        for (int i = 0; i < 8; i++) {
            lorabank::Channel c; c.bwHz = 125e3; c.offsetHz = (i - 3.5) * 200e3; c.sfs = {7, 8, 9, 10, 11, 12}; c.syncs = {0x34}; c.preamble = 8; ch.push_back(c);
        }
        size_t d; const double v = bankCpu(2e6, ch, secs, &d);
        printf("CPU 8ch x 6sf 2 Msps (%zu dec): %.4f\n", d, v);
        std::vector<lorabank::Channel> c2(ch.begin(), ch.begin() + 2);
        const double v2 = bankCpu(2e6, c2, secs, &d);
        printf("CPU 2ch x 6sf 2 Msps (%zu dec): %.4f\n", d, v2);
    }
    return 0;
}

int main(int argc, char** argv) {
    if (const char* e = getenv("LORA_RATIO")) lora::Channelizer::kMinRatio = atof(e);
    if (const char* e = getenv("LORA_RATE_X")) gRateX = atof(e);
    if (argc >= 5 && !strcmp(argv[1], "sweep")) return sweep(argc, argv);
    if (argc >= 2 && !strcmp(argv[1], "cpu")) return cpu(argc, argv);
    printf("usage: lora_bench sweep sf bwKhz cr [cfoHz sroPpm len frames floorOffsetDb] | cpu [secs]\n");
    return 1;
}
