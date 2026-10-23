// The directory heartbeat: a thread that POSTs a signed JSON body over HTTPS (certificate and host name checked) every few minutes.
// The decisions about what is sent live in web_directory_proto.cpp; this file is the key file, the HTTPS client and the loop.
#include "web_directory.h"
#include "../../core/src/net/netcompat.h"
#include "../../core/src/net/tcp_client.h"
#include "../../core/src/net/tls_conn.h"
#include "dect2/mesh/mesh_crypto.h"
#include "server_config.h"
#include "web_directory_proto.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <mutex>
#include <random>
#include <thread>

namespace onair_web {

namespace {

namespace mc = dect2::meshcrypto;

void randomSeed(uint8_t seed[32]) {
#ifndef _WIN32
    std::ifstream u("/dev/urandom", std::ios::binary);
    if (u.read((char*)seed, 32) && u.gcount() == 32) return;
#endif
    std::random_device rd;
    for (int i = 0; i < 32; i++) seed[i] = (uint8_t)(rd() & 0xFF);
}

struct Target { std::string host, path; int port = 443; };

bool splitUrl(const std::string& dirUrl, Target& t, std::string& err) {
    std::string u, e;
    if (!dirNormalizeUrl(dirUrl, u, e)) { err = "web_directory_url: " + e; return false; }
    std::string rest = u.substr(8);
    const size_t slash = rest.find('/');
    std::string hp = rest.substr(0, slash);
    t.path = rest.substr(slash);
    if (hp.find('[') != std::string::npos) { err = "web_directory_url: IPv6 literals are not supported, use a host name"; return false; }
    const size_t colon = hp.find(':');
    if (colon != std::string::npos) { t.port = atoi(hp.c_str() + colon + 1); hp.resize(colon); }
    if (t.port <= 0 || t.port > 65535) { err = "web_directory_url: bad port"; return false; }
    t.host = hp;
    return true;
}

class Impl : public DirectoryHeartbeat {
public:
    ~Impl() override { stop(); }

    bool start(const DirectoryConfig& c, std::function<std::string()> info, std::string& err) override {
        if (!prepare(c, std::move(info), err)) return false;
        DirRecord r;
        if (!dirBuildRecord(c_, info_(), (long long)time(nullptr), id_, pub_, r, err)) return false;
        const std::string code = dirAck(r, c_.url);
        if (c_.ack != code) {
            printf("onair-server: %s", dirPreview(r, c_.url).c_str());
            err = "web_directory is on, but nothing is sent yet: to confirm exactly this, add  web_directory_ack=" + code + "  to the settings file";
            return false;
        }
        if (!dect2::tls::available()) { err = "web_directory needs HTTPS, and this build has no TLS support (built without OpenSSL): nothing is sent"; return false; }
        th_ = std::thread([this] { loop(); });
        return true;
    }

    void stop() override {
        { std::lock_guard<std::mutex> g(m_); stop_ = true; }
        cv_.notify_all();
        if (th_.joinable()) th_.join();
    }

    bool removeNow(const DirectoryConfig& c, std::string& err) override {
        if (!prepare(c, nullptr, err)) return false;
        if (!dect2::tls::available()) { err = "this build has no TLS support"; return false; }
        DirRecord r;
        r.action = "delete";
        r.id = id_;
        r.pub = pub_;
        r.ts = (long long)time(nullptr);
        int status = 0;
        if (!post(dirBodyJson(r, dirSign(seed_, r)), status, err)) return false;
        if (status != 200) { err = "the directory answered " + std::to_string(status); return false; }
        return true;
    }

private:
    // key file and target; also used by removeNow() without a confirmation
    bool prepare(const DirectoryConfig& c, std::function<std::string()> info, std::string& err) {
        c_ = c;
        info_ = std::move(info);
        if (c_.url.empty()) { err = "web_directory_url is empty"; return false; }
        if (!splitUrl(c_.url, target_, err)) return false;
        if (info_ && c_.publicUrl.empty()) { err = "web_public_url is empty: it is the address listeners open (https://...)"; return false; }
        if (c_.keyFile.empty()) { err = "no place for the directory key"; return false; }
        onair_server::ConfigFile kf;
        kf.load(c_.keyFile);
        std::vector<uint8_t> s;
        if (!mc::hexDecode(kf.get("seed"), s) || s.size() != 32) {
            randomSeed(seed_);
            kf.set("seed", mc::hexEncode(seed_, 32));
            kf.set("public_key", dirPubHex(seed_));
            if (!kf.save(c_.keyFile, err)) { err = "cannot write the directory key file: " + err; return false; }
            printf("onair-server: made a new directory key in %s (keep it: it proves this server is the one listed)\n", c_.keyFile.c_str());
        } else {
            memcpy(seed_, s.data(), 32);
        }
        pub_ = dirPubHex(seed_);
        id_ = dirIdFromPub(pub_);
        return true;
    }

    // one HTTPS POST; the answer is read up to 4 KB, only the status is used
    bool post(const std::string& body, int& status, std::string& err) {
        using namespace dect2;
        sock_t s = tcpConnect(target_.host, target_.port, 10000, err);
        if (s == kBadSock) return false;
        sockRecvTimeout(s, 10000);
        tls::Ctx* ctx = tls::makeClientCtx(err);
        if (!ctx) { sockClose(s); return false; }
        tls::Conn* tc = tls::connect(ctx, s, target_.host, err);
        bool ok = false;
        if (tc) {
            const std::string req = "POST " + target_.path + "api/v1/heartbeat HTTP/1.1\r\nHost: " + target_.host + "\r\nUser-Agent: OnAir-Server/" + c_.version +
                                    "\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
            if (tls::write(tc, req.data(), req.size()) == (long)req.size()) {
                char buf[512];
                const long n = tls::read(tc, buf, sizeof buf - 1);
                if (n >= 12 && memcmp(buf, "HTTP/1.", 7) == 0) { status = atoi(buf + 9); ok = true; }
                else err = "no valid answer from the directory";
            } else err = "could not send to the directory";
            tls::close(tc);
        }
        tls::freeCtx(ctx);
        sockClose(s);
        return ok;
    }

    void loop() {
        int fails = 0;
        int lastStatus = -1;
        for (;;) {
            int status = 0;
            std::string err;
            DirRecord r;
            bool ok = false;
            if (dirBuildRecord(c_, info_(), (long long)time(nullptr), id_, pub_, r, err)) {
                ok = post(dirBodyJson(r, dirSign(seed_, r)), status, err) && status == 200;
                if (!ok && err.empty()) err = "the directory answered " + std::to_string(status);
            }
            if (ok) { if (lastStatus != 200) printf("onair-server: listed in the directory %s (id %s)\n", c_.url.c_str(), id_.c_str()); fails = 0; }
            else if (status != lastStatus || fails == 0) { fprintf(stderr, "onair-server: directory heartbeat failed: %s\n", err.c_str()); fails++; }
            else fails++;
            lastStatus = ok ? 200 : (status ? status : -2);
            const int wait = ok ? c_.intervalSec : std::min(c_.intervalSec, 30 << std::min(fails, 4));
            std::unique_lock<std::mutex> g(m_);
            if (cv_.wait_for(g, std::chrono::seconds(std::max(30, wait)), [this] { return stop_; })) return;
        }
    }

    DirectoryConfig c_;
    std::function<std::string()> info_;
    Target target_;
    uint8_t seed_[32] = {};
    std::string pub_, id_;
    std::thread th_;
    std::mutex m_;
    std::condition_variable cv_;
    bool stop_ = false;
};

} // namespace

DirectoryHeartbeat* makeDirectoryHeartbeat() { return new Impl; }

} // namespace onair_web
