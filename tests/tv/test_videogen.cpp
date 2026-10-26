// Digital video generator (Tools > Digital video generator), the parts without a window:
//  1. the bitrate estimate: the suggestion follows resolution, frame rate, codec and frame types; the TS budget adds up; the mux rate covers it
//  2. the encoder settings: the encoders found really open, MPEG-2 is the fallback, the capabilities that grey out the controls
//  3. the saved settings and overlay layout come back the same; snapping; the fit plans (letterbox, crop)
//  4. the overlay is drawn and laid on the picture
//  5. a .ts file from a still picture (MPEG-2 + MP2, two audio tracks, set service ID and PIDs): PAT, PMT, SDT name, length and mux rate
//  6. the same with H.264 when this FFmpeg has an H.264 encoder
//  7. UDP and RTP streaming to a local port: whole TS datagrams, RTP header with payload type 33
// Run from the source root (the overlay font is third_party/fonts/Inter-Medium.ttf).
#include "dect2/tv/videogen.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <thread>
extern "C" {
#include <libavformat/avformat.h>
}
#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace dect2::vgen;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { failures++; std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); } } while (0)

static std::vector<uint8_t> readAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// a 400x300 colour picture as PPM (FFmpeg reads it as a one-frame image)
static void writePicture(const fs::path& p) {
    std::ofstream f(p, std::ios::binary);
    f << "P6\n400 300\n255\n";
    for (int y = 0; y < 300; y++)
        for (int x = 0; x < 400; x++) {
            const unsigned char px[3] = {(unsigned char)(x * 255 / 399), (unsigned char)(y * 255 / 299), (unsigned char)((x / 50 + y / 50) % 2 ? 220 : 30)};
            f.write((const char*)px, 3);
        }
}

static bool hasEncoder(const char* name) {
    for (const EncoderInfo& e : videoEncoders()) if (e.name == name) return true;
    return false;
}

static void testEstimate() {
    Settings s;   // the spec's defaults: H.264, 720x576, 15 fps, I/P/B, AAC 96 kbit/s mono
    const int base = suggestVideoKbps(s);
    CHECK(base >= 900 && base <= 1100, "default suggestion %d kbit/s, expected about 1000", base);
    Settings lo = s; lo.width = 352; lo.height = 288;
    CHECK(suggestVideoKbps(lo) < base / 3, "CIF needs far less than 720x576 (%d vs %d)", suggestVideoKbps(lo), base);
    Settings f25 = s; f25.fps = 25;
    CHECK(suggestVideoKbps(f25) > base, "25 fps needs more than 15 fps");
    Settings hevc = s; hevc.videoEncoder = "libx265";
    Settings mp2v = s; mp2v.videoEncoder = "mpeg2video";
    CHECK(suggestVideoKbps(hevc) < base && base < suggestVideoKbps(mp2v), "HEVC < H.264 < MPEG-2 (%d, %d, %d)", suggestVideoKbps(hevc), base, suggestVideoKbps(mp2v));
    Settings intra = s; intra.frames = FrameTypes::IOnly;
    CHECK(suggestVideoKbps(intra) > 2 * base, "I-only needs much more");
    Settings manual = s; manual.autoBitrate = false; manual.videoKbps = 2500;
    CHECK(effectiveVideoKbps(manual) == 2500, "a set bitrate is kept");

    const Budget b = estimate(s);
    CHECK(std::fabs(b.total - (b.video + b.audio + b.pesTs + b.psiSi)) < 1e-6, "total is the sum of its parts");
    CHECK(b.audio == 96, "one AAC track at 96 kbit/s, got %.0f", b.audio);
    CHECK(b.pesTs > b.video * 0.02 && b.pesTs < b.video * 0.15, "TS/PES overhead is a few per cent (%.1f)", b.pesTs);
    CHECK(b.suggestedMux >= b.peak && b.mux == b.suggestedMux, "the mux rate covers the peak");
    Settings vbr = s; vbr.rc = RateControl::ConstrainedVbr;
    CHECK(estimate(vbr).suggestedMux > b.suggestedMux, "constrained VBR needs peak headroom");
    Settings vo = s; vo.content = TsContent::VideoOnly;
    CHECK(estimate(vo).audio == 0 && audioTrackCount(vo) == 0, "video only has no audio");
    Settings multi = s; multi.content = TsContent::VideoMultiAudio; multi.audioTracks = 3;
    CHECK(estimate(multi).audio == 3 * 96, "three tracks");
    Settings fixedMux = s; fixedMux.autoMux = false; fixedMux.muxKbps = 500;
    const Budget fb = estimate(fixedMux);
    CHECK(fb.mux == 500, "a set mux rate is used");
    bool warned = false;
    for (const std::string& w : checkSettings(fixedMux, fb)) if (w.find("mux rate") != std::string::npos) warned = true;
    CHECK(warned, "a mux rate below the streams is warned about");
    CHECK(paramValue(Param::Resolution, s) == "720x576", "resolution text");
    CHECK(paramValue(Param::FrameRate, s) == "15 fps", "frame rate text");
    CHECK(paramValue(Param::Gop, s) == "2.0 s (30 frames)", "GOP text: %s", paramValue(Param::Gop, s).c_str());
}

static void testEncoders() {
    const auto& v = videoEncoders();
    CHECK(hasEncoder("mpeg2video"), "MPEG-2 video is always there");
    for (const EncoderInfo& e : v) CHECK(codecOf(e.name) == e.codec, "codec of %s", e.name.c_str());
    std::vector<EncoderInfo> only2;
    EncoderInfo m; m.name = "mpeg2video"; only2.push_back(m);
    CHECK(pickVideoEncoder("libx264", only2) == "mpeg2video", "falls back to MPEG-2");
    EncoderInfo nv; nv.name = "h264_nvenc"; nv.codec = Codec::H264; only2.push_back(nv);
    CHECK(pickVideoEncoder("libx264", only2) == "h264_nvenc", "another H.264 encoder before MPEG-2");
    const EncoderCaps x = capsOf("libx264"), m2 = capsOf("mpeg2video"), hw = capsOf("h264_nvenc");
    CHECK(x.preset && x.zeroLatency && x.profile && x.crf && x.bFrames, "x264 has every option");
    CHECK(!m2.preset && !m2.crf && !m2.profile && m2.bFrames, "MPEG-2: B-frames only");
    CHECK(!hw.preset && !hw.crf && hw.profile, "hardware H.264: profile, no x264 preset or CRF");
    for (const EncoderInfo& a : audioEncoders()) CHECK(audioCodecOf(a.name) == a.audio, "audio codec of %s", a.name.c_str());
    std::printf("video encoders:");
    for (const EncoderInfo& e : v) std::printf(" %s", e.name.c_str());
    std::printf("\naudio encoders:");
    for (const EncoderInfo& e : audioEncoders()) std::printf(" %s", e.name.c_str());
    std::printf("\n");
}

static void testSaveAndGeometry() {
    Settings s;
    s.videoEncoder = "mpeg2video"; s.fps = 12.5; s.width = 640; s.height = 360; s.serviceName = "Test"; s.autoPids = false; s.videoPid = 0x321;
    Settings r;
    loadSettings(saveSettings(s), r);
    CHECK(r.videoEncoder == s.videoEncoder && r.fps == s.fps && r.width == 640 && r.serviceName == "Test" && !r.autoPids && r.videoPid == 0x321, "settings round trip");

    Layout l;
    Block t; t.kind = BlockKind::Text; t.text = "two\nlines \\ here"; t.x = 0.25f; t.fontPx = 30;
    Block p; p.kind = BlockKind::Param; p.param = Param::MuxRate; p.text = "Mux";
    Block rl; rl.kind = BlockKind::Rolling; rl.text = "News"; rl.direction = 1; rl.charsPerSec = 2.5f;
    l.blocks = {t, p, rl};
    const Layout b = loadLayout(saveLayout(l));
    CHECK(b.blocks.size() == 3, "three blocks back");
    if (b.blocks.size() == 3) {
        CHECK(b.blocks[0].text == t.text && b.blocks[0].x == 0.25f && b.blocks[0].fontPx == 30, "text block round trip");
        CHECK(b.blocks[1].kind == BlockKind::Param && b.blocks[1].param == Param::MuxRate, "parameter block round trip");
        CHECK(b.blocks[2].direction == 1 && b.blocks[2].charsPerSec == 2.5f, "rolling block round trip");
    }

    // snapping: within the distance it lands on the other block's edge or the picture's, outside it stays
    const std::vector<Rect> others = {{100, 100, 50, 20}};
    Rect r1 = snapRect({153, 104, 40, 20}, others, 720, 576, 6);
    CHECK(r1.x == 150 && r1.y == 100, "snaps to the right side and the top (%.0f, %.0f)", r1.x, r1.y);
    Rect r2 = snapRect({300, 300, 40, 20}, others, 720, 576, 6);
    CHECK(r2.x == 300 && r2.y == 300, "no snap when far");
    Rect r3 = snapRect({3, 553, 40, 20}, {}, 720, 576, 6);
    CHECK(r3.x == 0 && r3.y == 556, "snaps to the picture's corner (%.0f, %.0f)", r3.x, r3.y);

    const FitPlan lb = fitPlan(640, 480, 1280, 720, Fit::Letterbox);
    CHECK(lb.dw == 960 && lb.dh == 720 && lb.dx == 160 && lb.dy == 0 && lb.sw == 640, "4:3 in 16:9 letterbox");
    const FitPlan cr = fitPlan(640, 480, 1280, 720, Fit::Crop);
    CHECK(cr.sw == 640 && cr.sh == 360 && cr.sy == 60 && cr.dw == 1280, "4:3 in 16:9 crop");
    const FitPlan st = fitPlan(640, 480, 1280, 720, Fit::Stretch);
    CHECK(st.sw == 640 && st.sh == 480 && st.dw == 1280 && st.dh == 720, "stretch uses all");

    CHECK(streamUrl(Output::Rtp, "192.168.1.20", 5000) == "rtp://192.168.1.20:5000", "RTP address");
    CHECK(streamUrl(Output::Udp, "192.168.1.20", 5000) == "udp://192.168.1.20:5000", "UDP address");
    CHECK(vlcOpenUrl(Output::Rtp, "192.168.1.20", 5000) == "rtp://@:5000", "VLC unicast");
    CHECK(vlcOpenUrl(Output::Udp, "239.1.1.1", 5000) == "udp://@239.1.1.1:5000", "VLC multicast");
}

static std::shared_ptr<const std::vector<uint8_t>> font() {
    auto f = std::make_shared<std::vector<uint8_t>>(readAll("third_party/fonts/Inter-Medium.ttf"));
    return f->empty() ? nullptr : f;
}

static void testOverlay() {
    OverlayRenderer r;
    const auto f = font();
    if (!f) { std::printf("overlay: font not found (run from the source root), text checks skipped\n"); return; }
    CHECK(r.setFont(f), "the font loads");
    Settings s;
    Layout l;
    Block t; t.kind = BlockKind::Text; t.text = "OnAir"; t.x = 0.1f; t.y = 0.1f; t.fontPx = 40; t.background = false;
    Block rl; rl.kind = BlockKind::Rolling; rl.text = "rolling text"; rl.x = 0; rl.y = 0.85f; rl.w = 1; rl.background = false;
    l.blocks = {t, rl};
    const Rect tr = r.blockRect(t, s, 720, 576);
    CHECK(tr.w > 80 && tr.h > 40, "text block size %.0fx%.0f", tr.w, tr.h);
    std::vector<uint8_t> a, b;
    r.render(l, s, 0.5, 720, 576, a);
    r.render(l, s, 3.0, 720, 576, b);
    size_t lit = 0, diff = 0;
    for (size_t i = 3; i < a.size(); i += 4) { lit += a[i] > 0; diff += a[i] != b[i]; }
    CHECK(lit > 200, "text is drawn (%zu pixels)", lit);
    CHECK(diff > 50, "the rolling text moves (%zu pixels changed)", diff);
    // the rolling text starts at the left edge going right: at t = 0.5 its right end is just inside the band
    std::vector<uint8_t> y(720 * 576, 16), u(360 * 288, 128), v(360 * 288, 128);
    OverlayRenderer::blendYuv420(a.data(), 720, 576, y.data(), 720, u.data(), 360, v.data(), 360);
    size_t bright = 0;
    for (uint8_t px : y) bright += px > 200;
    CHECK(bright > 100, "white text shows in the picture (%zu)", bright);
    // a PNG logo with transparency: drawn inside its box, the corners stay clear
    Block lg; lg.kind = BlockKind::Logo; lg.text = "assets/branding/OnAir-glyph-transparent-2048.png"; lg.x = 0.5f; lg.y = 0.5f; lg.w = 0.25f; lg.h = 0.25f;
    Layout ll; ll.blocks = {lg};
    std::vector<uint8_t> c;
    r.render(ll, s, 0, 720, 576, c);
    size_t in = 0, out = 0, partial = 0;
    for (int py = 0; py < 576; py++)
        for (int px = 0; px < 720; px++) {
            const uint8_t al = c[((size_t)py * 720 + px) * 4 + 3];
            const bool inside = px >= 360 && px < 540 && py >= 288 && py < 432;
            (inside ? in : out) += al > 0;
            partial += al > 0 && al < 255;
        }
    CHECK(in > 2000, "the PNG logo is drawn (%zu pixels)", in);
    CHECK(out == 0, "the logo stays in its box (%zu pixels outside)", out);
    CHECK(partial > 0 && c[((size_t)288 * 720 + 360) * 4 + 3] == 0, "transparency is kept");
    lg.text = "no/such/logo.png"; ll.blocks = {lg};
    r.render(ll, s, 0, 720, 576, c);
    size_t any = 0;
    for (size_t i = 3; i < c.size(); i += 4) any += c[i] > 0;
    CHECK(any == 0, "a missing logo file draws nothing");
}

// PIDs and names from a transport stream file
struct TsInfo { std::set<int> pids; int program = -1, pmtPid = -1; std::vector<std::pair<int, int>> streams; size_t packets = 0; bool sync = true; };

static TsInfo parseTs(const std::vector<uint8_t>& d) {
    TsInfo t;
    if (d.size() % 188) t.sync = false;
    for (size_t o = 0; o + 188 <= d.size(); o += 188) {
        const uint8_t* p = &d[o];
        if (p[0] != 0x47) { t.sync = false; break; }
        t.packets++;
        const int pid = ((p[1] & 0x1F) << 8) | p[2];
        t.pids.insert(pid);
        const bool start = p[1] & 0x40;
        int off = 4;
        if (p[3] & 0x20) off += 1 + p[4];
        if (!(p[3] & 0x10) || !start || off >= 188) continue;
        off += 1 + p[off];   // pointer field
        if (pid == 0 && t.program < 0 && p[off] == 0x00) {
            t.program = (p[off + 8] << 8) | p[off + 9];
            t.pmtPid = ((p[off + 10] & 0x1F) << 8) | p[off + 11];
        } else if (pid == t.pmtPid && t.streams.empty() && p[off] == 0x02) {
            const int len = ((p[off + 1] & 0x0F) << 8) | p[off + 2];
            const int pil = ((p[off + 10] & 0x0F) << 8) | p[off + 11];
            int i = off + 12 + pil;
            const int end = off + 3 + len - 4;
            while (i + 5 <= end && i + 5 < 188) {
                t.streams.push_back({p[i], ((p[i + 1] & 0x1F) << 8) | p[i + 2]});
                i += 5 + (((p[i + 3] & 0x0F) << 8) | p[i + 4]);
            }
        }
    }
    return t;
}

static std::string serviceNameOf(const fs::path& p) {
    AVFormatContext* fc = nullptr;
    std::string n;
    if (avformat_open_input(&fc, p.string().c_str(), nullptr, nullptr) < 0) return n;
    avformat_find_stream_info(fc, nullptr);
    if (fc->nb_programs > 0) {
        const AVDictionaryEntry* e = av_dict_get(fc->programs[0]->metadata, "service_name", nullptr, 0);
        if (e) n = e->value;
    }
    avformat_close_input(&fc);
    return n;
}

static GenConfig pictureConfig(const fs::path& pic) {
    GenConfig c;
    c.source = Source::Picture;
    c.path = pic.string();
    c.audio = AudioSource::Tone;
    c.fit = Fit::Letterbox;
    c.enc.width = 352; c.enc.height = 288; c.enc.fps = 10;
    c.enc.audioEncoder = "mp2";
    c.font = font();
    Block t; t.kind = BlockKind::Param; t.param = Param::VideoBitrate; t.text = "Video";
    Block rl; rl.kind = BlockKind::Rolling; rl.text = "Test transmission"; rl.x = 0; rl.y = 0.85f; rl.w = 1; rl.fontPx = 20;
    c.overlay.blocks = {t, rl};
    return c;
}

static void testTsFile(const fs::path& dir, const fs::path& pic) {
    GenConfig c = pictureConfig(pic);
    c.output = Output::File;
    c.tsPath = (dir / "mpeg2.ts").string();
    c.seconds = 3;
    c.enc.videoEncoder = "mpeg2video";
    c.enc.content = TsContent::VideoMultiAudio; c.enc.audioTracks = 2;
    c.enc.serviceId = 0x1234; c.enc.serviceName = "Generator test";
    c.enc.autoPids = false; c.enc.pmtPid = 0x150; c.enc.videoPid = 0x200; c.enc.audioPid = 0x300;
    VideoGenerator g;
    std::string err;
    CHECK(g.start(c, err), "start: %s", err.c_str());
    CHECK(g.wait(60), "the file is finished in time");
    const GenStats st = g.stats();
    CHECK(st.error.empty() && st.finished, "no error: %s", st.error.c_str());
    CHECK(st.frames == 30, "30 frames (%llu)", (unsigned long long)st.frames);
    CHECK(st.videoEncoder == "mpeg2video" && st.audioEncoder == "mp2", "encoders used: %s %s", st.videoEncoder.c_str(), st.audioEncoder.c_str());
    const std::vector<uint8_t> d = readAll(c.tsPath);
    const TsInfo ti = parseTs(d);
    CHECK(ti.sync && ti.packets > 100, "whole TS packets (%zu)", ti.packets);
    CHECK(ti.program == 0x1234 && ti.pmtPid == 0x150, "PAT: program 0x%x on PMT PID 0x%x", ti.program, ti.pmtPid);
    bool video = false;
    int audio = 0;
    for (auto& s : ti.streams) {
        if (s.first == 0x02 && s.second == 0x200) video = true;
        if ((s.first == 0x03 || s.first == 0x04) && (s.second == 0x300 || s.second == 0x301)) audio++;
    }
    CHECK(video, "PMT: MPEG-2 video on PID 0x200");
    CHECK(audio == 2, "PMT: two MPEG audio tracks on 0x300 / 0x301 (%d)", audio);
    CHECK(ti.pids.count(0x11) == 1, "SDT present");
    CHECK(serviceNameOf(c.tsPath) == "Generator test", "service name in the SDT");
    // the mux rate holds: size / length within 10 % of the set rate
    const double kbps = (double)d.size() * 8 / 3.0 / 1000.0;
    const Budget b = estimate(c.enc);
    CHECK(std::fabs(kbps - b.mux) < b.mux * 0.1, "file rate %.0f kbit/s, mux rate %d", kbps, b.mux);
    CHECK(ti.pids.count(0x1FFF) == 1, "null packets fill the mux rate");
    std::printf("mpeg2.ts: %zu packets, %.0f kbit/s (mux %d)\n", ti.packets, kbps, b.mux);
}

static void testH264(const fs::path& dir, const fs::path& pic) {
    std::string enc;
    for (const EncoderInfo& e : videoEncoders()) if (e.codec == Codec::H264) { enc = e.name; break; }
    if (enc.empty()) { std::printf("H.264: no encoder in this FFmpeg, skipped\n"); return; }
    GenConfig c = pictureConfig(pic);
    c.output = Output::File;
    c.tsPath = (dir / "h264.ts").string();
    c.seconds = 2;
    c.enc.videoEncoder = enc;
    c.enc.audioEncoder = "aac";
    VideoGenerator g;
    std::string err;
    CHECK(g.start(c, err), "start H.264: %s", err.c_str());
    CHECK(g.wait(60), "the H.264 file is finished in time");
    const GenStats st = g.stats();
    CHECK(st.error.empty() && st.finished, "H.264 (%s): %s", enc.c_str(), st.error.c_str());
    const TsInfo ti = parseTs(readAll(c.tsPath));
    bool h264 = false, aac = false;
    for (auto& s : ti.streams) { h264 |= s.first == 0x1B; aac |= s.first == 0x0F; }
    CHECK(ti.sync && h264 && aac, "PMT: H.264 (0x1B) and AAC (0x0F) with %s", enc.c_str());
}

#ifndef _WIN32
static void testStream(const fs::path& pic, bool rtp) {
    int rx = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(rx, (sockaddr*)&a, sizeof a) != 0) { CHECK(false, "bind"); close(rx); return; }
    socklen_t al = sizeof a;
    getsockname(rx, (sockaddr*)&a, &al);
    timeval tv{0, 200000};
    setsockopt(rx, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    GenConfig c = pictureConfig(pic);
    c.output = rtp ? Output::Rtp : Output::Udp;
    c.host = "127.0.0.1";
    c.port = ntohs(a.sin_port);
    c.enc.videoEncoder = "mpeg2video";
    VideoGenerator g;
    std::string err;
    CHECK(g.start(c, err), "start stream: %s", err.c_str());
    size_t got = 0, good = 0;
    uint8_t buf[2048];
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
    while (std::chrono::steady_clock::now() < end) {
        const ssize_t n = recv(rx, buf, sizeof buf, 0);
        if (n <= 0) continue;
        got++;
        const int hdr = rtp ? 12 : 0;
        if (n == hdr + 7 * 188 && buf[hdr] == 0x47 && buf[hdr + 188] == 0x47 && (!rtp || (buf[0] == 0x80 && (buf[1] & 0x7F) == 33))) good++;
    }
    CHECK(g.running(), "streaming keeps going until stopped");
    g.stop();
    close(rx);
    const GenStats st = g.stats();
    CHECK(st.error.empty(), "stream error: %s", st.error.c_str());
    CHECK(got > 20 && good == got, "%s: %zu datagrams, %zu whole", rtp ? "RTP" : "UDP", got, good);
    // about real time: 1.5 s at 10 fps
    CHECK(st.frames >= 10 && st.frames <= 20, "paced in real time (%llu frames in 1.5 s)", (unsigned long long)st.frames);
}
#endif

int main() {
    const fs::path dir = fs::temp_directory_path() / ("onair_videogen_" + std::to_string((long long)std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    const fs::path pic = dir / "picture.ppm";
    writePicture(pic);
    testEstimate();
    testEncoders();
    testSaveAndGeometry();
    testOverlay();
    testTsFile(dir, pic);
    testH264(dir, pic);
#ifndef _WIN32
    testStream(pic, false);
    testStream(pic, true);
#else
    std::printf("streaming: skipped on Windows\n");
#endif
    std::error_code ec;
    fs::remove_all(dir, ec);
    if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
    std::printf("videogen: all passed\n");
    return 0;
}
