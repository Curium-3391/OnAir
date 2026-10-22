// NFC / RFID physical-layer demodulation (see nfc_rx.h). Clean-room; receive only.
//
// Reader (PCD) frames: the field envelope |x| is tracked; a "pause" is a sharp dip. Once a start pause is seen the etu grid is
// locked (samples per etu come from fs) and each etu is classified by where its pause sits (none / 2nd half / 1st half), giving the
// modified-Miller symbols that millerSeqToBytes turns into bytes. ISO/IEC 15693 reader frames are told apart by their pulse-position
// pattern (a single narrow pause per symbol at one of four slots, 75.5 us apart) and decoded with the 1-of-4 rule.
// Tag (PICC) frames: the signal is mixed down by the subcarrier (847.5 kHz for 14443, 423.75 kHz for 15693) and low-pass filtered;
// the subcarrier envelope is Manchester-decoded (first half vs second half energy) into bits.
#include "dect2/nfc/nfc_rx.h"
#include "dect2/nfc/nfc_proto.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace dect2::nfc {

// ---------------------------------------------------------------- bit / byte framing (LSB first, odd parity)
std::vector<uint8_t> bytesToBits(const std::vector<uint8_t>& bytes, int bits) {
    std::vector<uint8_t> out;
    if (bits == 7 && bytes.size() == 1) {   // short frame, no parity
        for (int i = 0; i < 7; i++) out.push_back((bytes[0] >> i) & 1);
        return out;
    }
    for (uint8_t by : bytes) {
        int ones = 0;
        for (int i = 0; i < 8; i++) { const uint8_t b = (by >> i) & 1; out.push_back(b); ones += b; }
        out.push_back((ones & 1) ? 0 : 1);   // odd parity: total number of 1s (data + parity) is odd
    }
    return out;
}

std::vector<uint8_t> bitsToBytes(const std::vector<uint8_t>& bits, int& outBits, bool& parityOk) {
    std::vector<uint8_t> bytes;
    parityOk = true;
    if (bits.size() == 7) { uint8_t b = 0; for (int i = 0; i < 7; i++) b |= (uint8_t)(bits[i] << i); bytes.push_back(b); outBits = 7; return bytes; }
    const size_t groups = bits.size() / 9;
    for (size_t g = 0; g < groups; g++) {
        uint8_t b = 0; int ones = 0;
        for (int i = 0; i < 8; i++) { const uint8_t bit = bits[g * 9 + i]; b |= (uint8_t)(bit << i); ones += bit; }
        const uint8_t par = bits[g * 9 + 8];
        if (((ones + par) & 1) == 0) parityOk = false;   // odd parity expected
        bytes.push_back(b);
    }
    outBits = (int)groups * 8;
    return bytes;
}

// ---------------------------------------------------------------- modified-Miller symbol codec (0 = Y, 1 = X, 2 = Z)
std::vector<uint8_t> bytesToMillerSeq(const std::vector<uint8_t>& bytes, int bits) {
    const std::vector<uint8_t> b = bytesToBits(bytes, bits);
    std::vector<uint8_t> seq;
    seq.push_back(2);   // SOF = Z
    int prev = 0;       // SOF behaves as a preceding logic 0, so a leading 0 is coded Z
    for (uint8_t bit : b) {
        if (bit) { seq.push_back(1); prev = 1; }           // X
        else { seq.push_back(prev == 1 ? 0 : 2); prev = 0; } // Y after a 1, else Z
    }
    seq.push_back(0);   // EOF = logic 0 (Y), followed by no modulation
    return seq;
}

std::vector<uint8_t> millerSeqToBytes(const std::vector<uint8_t>& seq, int& bits, bool& parityOk) {
    bits = 0; parityOk = true;
    if (seq.size() < 2) return {};
    std::vector<uint8_t> all;   // bits after the SOF, one per symbol (X->1, Y/Z->0)
    for (size_t i = 1; i < seq.size(); i++) all.push_back(seq[i] == 1 ? 1 : 0);
    if (all.empty()) return {};
    all.pop_back();   // drop the EOF logic-0
    if (all.size() == 7) return bitsToBytes(all, bits, parityOk);
    // trim to a whole number of (8 + parity) groups
    const size_t groups = all.size() / 9;
    all.resize(groups * 9);
    return bitsToBytes(all, bits, parityOk);
}

// ---------------------------------------------------------------- the demodulator
namespace {
constexpr double kIdleEtu = 6.0;        // a gap of this many etu ends a burst (must exceed an ISO 15693 Manchester gap: a bit is
                                        // 4 etu, so a "1" then "0" leaves up to 4 etu of no subcarrier within one frame)
constexpr float kPauseFrac = 0.4f;      // envelope below this fraction of the field = a pause (a 100% ASK reader pause goes to ~0;
                                        // a tag's weaker load modulation never dips this far, so the two are told apart)
}

struct Demod::State {
    double fs = 0;
    // carrier / field envelope smoothing
    double fieldLevel = 0;              // slow running maximum of |x| (the un-paused field)
    // subcarrier mixers (Type A/B at 847.5 kHz, 15693 at 423.75 kHz), each with a 3-pole low-pass after the down-mix so the carrier
    // leakage at the (negative) subcarrier image is well below the detection threshold while the data band passes.
    double phA = 0, phV = 0;
    std::complex<float> lpA[3]{}, lpV[3]{};
    double alphaA = 0.2, alphaV = 0.1;
    // burst capture
    std::vector<float> env;             // field envelope, |x| smoothed
    std::vector<float> subA;            // 847.5 kHz subcarrier magnitude
    std::vector<float> subV;            // 423.75 kHz subcarrier magnitude
    bool inBurst = false;
    size_t idleRun = 0;
    double sampleTime = 0;              // absolute time of env[0..]
    uint64_t sampleIdx = 0;
};

Demod::Demod(double fs) : st_(std::make_shared<State>()) { setFs(fs); }

void Demod::setFs(double fs) {
    st_->fs = fs;
    // Type A/B path passes the ~106 kbit/s data (cutoff ~160 kHz); ISO 15693 path passes the ~26 kbit/s data (cutoff ~55 kHz). Both
    // reject their subcarrier-frequency carrier leakage by about -45 dB across three poles.
    st_->alphaA = fs > 0 ? 1.0 - std::exp(-2.0 * 3.14159265358979 * 160e3 / fs) : 0.2;
    st_->alphaV = fs > 0 ? 1.0 - std::exp(-2.0 * 3.14159265358979 * 55e3 / fs) : 0.1;
    reset();
}

void Demod::reset() {
    State& s = *st_;
    s.fieldLevel = 0; s.phA = s.phV = 0;
    for (int i = 0; i < 3; i++) { s.lpA[i] = {0,0}; s.lpV[i] = {0,0}; }
    s.env.clear(); s.subA.clear(); s.subV.clear();
    s.inBurst = false; s.idleRun = 0; s.sampleTime = 0; s.sampleIdx = 0;
    peakSubDb_ = -200; carrierDb_ = -200;
}

// Convert a subcarrier-magnitude envelope burst into bits via Manchester (first vs second half of each etu). The tag SOF is one
// full-on etu (both halves high), which aligns the bit grid and is skipped.
static std::vector<uint8_t> manchesterBits(const std::vector<float>& e, double spEtu) {
    float peak = 0; for (float v : e) peak = std::max(peak, v);
    if (peak <= 0 || spEtu < 2) return {};
    const float th = peak * 0.3f;
    size_t start = 0; while (start < e.size() && e[start] < th) start++;
    // skip a full-on SOF etu if present (both halves high)
    {
        double first = 0, second = 0; const size_t h = (size_t)(start + spEtu / 2);
        for (size_t i = start; i < h && i < e.size(); i++) first += e[i];
        for (size_t i = h; i < (size_t)(start + spEtu) && i < e.size(); i++) second += e[i];
        const double on = th * (spEtu / 2) * 0.5;
        if (first > on && second > on) start += (size_t)spEtu;   // SOF marker
    }
    std::vector<uint8_t> bits;
    for (double c = start; c + spEtu <= e.size(); c += spEtu) {
        double first = 0, second = 0;
        const size_t h = (size_t)(c + spEtu / 2);
        for (size_t i = (size_t)c; i < h && i < e.size(); i++) first += e[i];
        for (size_t i = h; i < (size_t)(c + spEtu) && i < e.size(); i++) second += e[i];
        if (first < th * spEtu * 0.25 && second < th * spEtu * 0.25) break;   // EOF: no subcarrier in either half
        bits.push_back(first > second ? 1 : 0);   // subcarrier in the first half = logic 1
    }
    return bits;
}

void Demod::processTagSubcarrier(const std::complex<float>*, size_t, double) {}
void Demod::processReaderEnvelope(const float*, size_t, double) {}

// decode one captured burst (field envelope + the two subcarrier envelopes), emit any frames found
static void decodeBurst(const std::function<void(const RawFrame&)>& onFrame, double fs,
                        const std::vector<float>& env, const std::vector<float>& subA, const std::vector<float>& subV,
                        double t0, float fieldLevel) {
    const double spEtu = fs * kEtu;   // samples per etu
    if (spEtu < 4) return;

    float subAPk = 0, subVPk = 0, envMin = fieldLevel;
    for (float v : subA) subAPk = std::max(subAPk, v);
    for (float v : subV) subVPk = std::max(subVPk, v);
    for (float v : env) envMin = std::min(envMin, v);
    // A reader (PCD) frame drives the field to ~0 during its pauses; a tag (PICC) answer keeps the field on and adds a subcarrier.
    const bool hasPause = fieldLevel > 0 && envMin < fieldLevel * kPauseFrac;

    auto db = [](float lin) { return lin > 0 ? 20.0f * std::log10(lin) : -200.0f; };

    if (!hasPause) {   // tag answer
        if (subAPk > fieldLevel * 0.05f && subAPk >= subVPk) {   // Type A/B on 847.5 kHz
            const std::vector<uint8_t> bits = manchesterBits(subA, spEtu);
            int b = 0; bool par = true;
            std::vector<uint8_t> bytes = bitsToBytes(bits, b, par);
            if (!bytes.empty()) {
                RawFrame f; f.proto = Proto::TypeA; f.fromReader = false; f.bits = b; f.parityOk = par;
                f.bytes = bytes; f.timeSec = t0; f.levelDb = db(subAPk);
                onFrame(f);
            }
        } else if (subVPk > fieldLevel * 0.05f) {   // ISO 15693 on 423.75 kHz (high data rate fc/512 = 4 etu/bit, no per-byte parity)
            const std::vector<uint8_t> bits = manchesterBits(subV, spEtu * 4.0);
            std::vector<uint8_t> bytes;
            for (size_t i = 0; i + 8 <= bits.size(); i += 8) { uint8_t by = 0; for (int k = 0; k < 8; k++) by |= (uint8_t)(bits[i+k] << k); bytes.push_back(by); }
            if (!bytes.empty()) {
                RawFrame f; f.proto = Proto::V15693; f.fromReader = false; f.bits = (int)bytes.size()*8; f.parityOk = true;
                f.bytes = bytes; f.timeSec = t0; f.levelDb = db(subVPk);
                onFrame(f);
            }
        }
        return;
    }

    // --- reader burst: find the pauses ---
    const float th = fieldLevel * kPauseFrac;
    std::vector<size_t> pauseCentre;
    bool below = false; size_t startIdx = 0;
    for (size_t i = 0; i < env.size(); i++) {
        if (env[i] < th && !below) { below = true; startIdx = i; }
        else if (env[i] >= th && below) { below = false; pauseCentre.push_back((startIdx + i) / 2); }
    }
    if (pauseCentre.empty()) return;

    // ISO 15693 1-of-4: pauses are ~spEtu wide spaced ~8*spEtu (75.5 us). Heuristic: the gap between the first two pauses.
    bool looksVicinity = false;
    if (pauseCentre.size() >= 2) {
        double meanGap = 0; int ng = 0;
        for (size_t i = 1; i < pauseCentre.size(); i++) { meanGap += pauseCentre[i] - pauseCentre[i-1]; ng++; }
        meanGap /= std::max(1, ng);
        if (meanGap > spEtu * 4.0) looksVicinity = true;   // sparse pauses = vicinity PPM
    }

    if (looksVicinity) {
        // 1-of-4: a symbol is 8 etu long (256/fc = 2 etu pause slot grid of 4). The pause position in the symbol encodes 2 bits.
        const double sym = spEtu * 8.0;
        const size_t sof = pauseCentre.front();
        std::vector<uint8_t> bitsV;
        for (double c = sof; c + sym <= env.size(); c += sym) {
            // find the pause nearest this symbol window
            double best = -1; for (size_t p : pauseCentre) if (p >= c - spEtu && p < c + sym) { best = p - c; }
            if (best < 0) break;
            const int slot = std::min(3, std::max(0, (int)std::round((best - spEtu) / (spEtu * 2.0))));
            bitsV.push_back(slot & 1); bitsV.push_back((slot >> 1) & 1);
        }
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i + 8 <= bitsV.size(); i += 8) { uint8_t by = 0; for (int k = 0; k < 8; k++) by |= (uint8_t)(bitsV[i+k] << k); bytes.push_back(by); }
        if (!bytes.empty()) {
            RawFrame f; f.proto = Proto::V15693; f.fromReader = true; f.bits = (int)bytes.size()*8; f.parityOk = true;
            f.bytes = bytes; f.timeSec = t0; f.levelDb = db(fieldLevel);
            onFrame(f);
        }
        return;
    }

    // ISO 14443 Type A reader: lock the etu grid at the first pause and read symbols.
    const double sof = (double)pauseCentre.front() - spEtu * 0.25;   // SOF pause sits in the first half of the start Z
    std::vector<uint8_t> seq;
    seq.push_back(2);   // SOF Z
    const size_t lastPause = pauseCentre.back();
    for (double c = sof + spEtu; c <= (double)lastPause + spEtu; c += spEtu) {
        // any pause in [c, c+spEtu)? which half?
        int sym = 0;   // Y (none)
        for (size_t p : pauseCentre) if (p >= (size_t)c && p < (size_t)(c + spEtu)) {
            sym = (p - (size_t)c) < (size_t)(spEtu / 2) ? 2 /*Z 1st half*/ : 1 /*X 2nd half*/;
            break;
        }
        seq.push_back((uint8_t)sym);
    }
    seq.push_back(0);   // EOF Y

    // The grid may leave the frame one pause-free symbol short or long. Try removing / keeping / adding a trailing Y and score each
    // candidate by its parity and, when it carries a CRC_A, the CRC. Keep the best.
    RawFrame best; int bestScore = -1;
    for (int delta = -1; delta <= 1; delta++) {
        std::vector<uint8_t> s = seq;
        if (delta < 0) { if (s.size() < 3) continue; s.erase(s.end() - 2); }       // drop one symbol before the EOF
        else for (int e = 0; e < delta; e++) s.insert(s.end() - 1, 0);             // add trailing Y symbols
        int b = 0; bool par = false;
        std::vector<uint8_t> bytes = millerSeqToBytes(s, b, par);
        if (bytes.empty()) continue;
        int score = 0;
        if (par) score += 2;
        if (bytes.size() >= 3 && crcAOk(bytes)) score += 4;
        if (score > bestScore) {
            bestScore = score;
            best = RawFrame{}; best.proto = Proto::TypeA; best.fromReader = true; best.bits = b; best.parityOk = par;
            best.bytes = bytes; best.timeSec = t0; best.levelDb = db(fieldLevel);
        }
    }
    if (bestScore >= 0) onFrame(best);
}

void Demod::feed(const std::complex<float>* x, size_t n) {
    State& s = *st_;
    if (s.fs <= 0 || !onFrame_) { return; }
    const double stepA = -2.0 * 3.14159265358979 * kSubA / s.fs;
    const double stepV = -2.0 * 3.14159265358979 * kSubV / s.fs;
    for (size_t i = 0; i < n; i++) {
        const std::complex<float> xi = x[i];
        const float mag = std::abs(xi);
        // slow field level (attack fast on high, decay slow)
        if (mag > s.fieldLevel) s.fieldLevel += (mag - (float)s.fieldLevel) * 0.05f;
        else s.fieldLevel += (mag - (float)s.fieldLevel) * 0.0005f;
        // subcarrier down-mix + one-pole LP
        const std::complex<float> rotA((float)std::cos(s.phA), (float)std::sin(s.phA));
        const std::complex<float> rotV((float)std::cos(s.phV), (float)std::sin(s.phV));
        s.phA += stepA; if (s.phA < -3.14159265) s.phA += 6.2831853;
        s.phV += stepV; if (s.phV < -3.14159265) s.phV += 6.2831853;
        std::complex<float> ya = xi * rotA; for (int k = 0; k < 3; k++) { s.lpA[k] += (ya - s.lpA[k]) * (float)s.alphaA; ya = s.lpA[k]; }
        std::complex<float> yv = xi * rotV; for (int k = 0; k < 3; k++) { s.lpV[k] += (yv - s.lpV[k]) * (float)s.alphaV; yv = s.lpV[k]; }
        const float sa = std::abs(s.lpA[2]), sv = std::abs(s.lpV[2]);
        if (sa > 0 && 20.f*std::log10(sa) > peakSubDb_) peakSubDb_ = 20.f*std::log10(sa);
        if ((float)s.fieldLevel > 0 && 20.f*std::log10((float)s.fieldLevel) > carrierDb_) carrierDb_ = 20.f*std::log10((float)s.fieldLevel);

        // activity: a pause (reader) or subcarrier (tag)
        const bool pause = s.fieldLevel > 1e-6 && mag < (float)s.fieldLevel * kPauseFrac;
        const bool subc = sa > (float)s.fieldLevel * 0.05f || sv > (float)s.fieldLevel * 0.05f;
        const bool active = pause || subc;

        if (active) { if (!s.inBurst) { s.inBurst = true; s.env.clear(); s.subA.clear(); s.subV.clear(); s.sampleTime = (double)s.sampleIdx / s.fs; } s.idleRun = 0; }
        if (s.inBurst) {
            s.env.push_back(mag);
            s.subA.push_back(sa);
            s.subV.push_back(sv);
            if (!active && ++s.idleRun > (size_t)(kIdleEtu * s.fs * kEtu)) {
                decodeBurst(onFrame_, s.fs, s.env, s.subA, s.subV, s.sampleTime, (float)s.fieldLevel);
                s.inBurst = false; s.idleRun = 0;
            } else if (s.env.size() > (size_t)(s.fs * 0.02)) {   // safety cap ~20 ms
                decodeBurst(onFrame_, s.fs, s.env, s.subA, s.subV, s.sampleTime, (float)s.fieldLevel);
                s.inBurst = false; s.idleRun = 0;
            }
        }
        s.sampleIdx++;
    }
}

}  // namespace dect2::nfc
