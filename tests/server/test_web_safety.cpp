// OnAir Web safety rules, checked three ways:
//  1. compile time: the web classes have no transmit / arm / key / record member (detection idiom), and the transmit headers refuse to be
//     included in code built with ONAIR_WEB_SESSION (the web sources are; CMakeLists.txt is checked for it below);
//  2. the protocol: every transmit / key / decrypt / record message type is refused with "forbidden", before and after hello;
//  3. a scan of server/web: no transmit header or symbol, no engine (it has the recorder), no voice key store, no file writing, and the
//     restricted decoder classes are off in a fresh policy.
#include "../../server/web/web_config.h"
#include "../../server/web/web_hub.h"
#include "../../server/web/web_server.h"
#include "server_config.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <type_traits>
using namespace onair_web;
#include "../common/test_util.h"

#ifndef ONAIR_SOURCE_DIR
#define ONAIR_SOURCE_DIR "."
#endif

// ---- 1. no such member can be called on the web classes
#define HAS_MEMBER(name)                                                                                   \
    template <class T, class = void> struct has_##name : std::false_type {};                               \
    template <class T> struct has_##name<T, std::void_t<decltype(&T::name)>> : std::true_type {};
HAS_MEMBER(transmit)
HAS_MEMBER(startTx)
HAS_MEMBER(stopTx)
HAS_MEMBER(setTx)
HAS_MEMBER(arm)
HAS_MEMBER(ptt)
HAS_MEMBER(txSink)
HAS_MEMBER(importKey)
HAS_MEMBER(setKey)
HAS_MEMBER(decrypt)
HAS_MEMBER(startRecording)
HAS_MEMBER(engine)
template <class T> constexpr bool noTxSurface() {
    return !has_transmit<T>::value && !has_startTx<T>::value && !has_stopTx<T>::value && !has_setTx<T>::value && !has_arm<T>::value && !has_ptt<T>::value &&
           !has_txSink<T>::value && !has_importKey<T>::value && !has_setKey<T>::value && !has_decrypt<T>::value && !has_startRecording<T>::value && !has_engine<T>::value;
}
static_assert(noTxSurface<WebHub>(), "WebHub must not offer transmit, keys, recording or the engine");
static_assert(noTxSurface<WebServer>(), "WebServer must not offer transmit, keys, recording or the engine");
static_assert(noTxSurface<VirtualRx>(), "VirtualRx must not offer transmit");
static_assert(noTxSurface<SliceIq>(), "SliceIq must not offer transmit");
static_assert(noTxSurface<WebDecoder>(), "decoders must not offer transmit or keys");

static std::string readFile(const std::filesystem::path& p) { std::ifstream f(p, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }

static void testProtocol() {
    WebHub h;
    std::string c, w;
    AdmitRequest a; a.ip = "10.0.0.1";
    const int id = h.admit(a, c, w);
    for (const char* t : {"tx", "transmit", "ptt", "arm", "tx_arm", "carrier", "key", "keys", "import_key", "key_import", "voicekeys", "decrypt", "record", "recording", "save", "upload", "exec", "shell", "config", "admin"}) {
        ClientMsg m; std::string code, why;
        CHECK(!parseClientMsg(std::string("{\"type\":\"") + t + "\"}", m, code, why) && code == err::Forbidden, "%s must be forbidden (%s)", t, code.c_str());
    }
    // what a session can send at all: none of it reaches a transmitter, a key or the disk
    for (const auto& t : clientTypes()) CHECK(!isForbiddenType(t), "%s", t.c_str());
    CHECK(clientTypes().size() == 9, "the protocol has exactly the nine documented client types");
    h.leave(id);
}

static void testSourceScan() {
    namespace fs = std::filesystem;
    const fs::path root = fs::path(ONAIR_SOURCE_DIR);
    const fs::path dir = root / "server" / "web";
    int files = 0;
    const char* banned[] = {"dect2/tx/", "TxSink", "makeTxSink", "TxTune", "voicekey", "VoiceKey", "voice_keys", "dect2/engine/engine.h", "startRecording",
                            "std::ofstream", "ofstream(", "fopen(", "fwrite(", "std::filesystem::rename", "fs::rename", "fs::remove", "std::remove(\"", "system(", "popen("};
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        const std::string ext = e.path().extension().string();
        if (ext != ".cpp" && ext != ".h") continue;
        files++;
        const std::string raw = readFile(e.path());
        // the code without comments and string literals (the forbidden message names are strings), plus every #include line as it is
        std::string code, includes;
        for (size_t i = 0; i < raw.size(); i++) {
            if (raw.compare(i, 2, "//") == 0) { while (i < raw.size() && raw[i] != '\n') i++; code += '\n'; continue; }
            if (raw.compare(i, 2, "/*") == 0) { const size_t e2 = raw.find("*/", i + 2); i = e2 == std::string::npos ? raw.size() : e2 + 1; continue; }
            if (raw[i] == '"') { i++; while (i < raw.size() && raw[i] != '"') { if (raw[i] == '\\') i++; i++; } code += "\"\""; continue; }
            if (raw[i] == '\'') { i++; while (i < raw.size() && raw[i] != '\'') { if (raw[i] == '\\') i++; i++; } code += "''"; continue; }
            code += raw[i];
        }
        std::istringstream ls(raw);
        std::string line;
        while (std::getline(ls, line)) if (line.compare(0, 8, "#include") == 0) includes += line + "\n";
        const std::string name = e.path().filename().string();
        for (const char* b : banned) {
            CHECK(code.find(b) == std::string::npos, "%s uses \"%s\"", name.c_str(), b);
            CHECK(includes.find(b) == std::string::npos, "%s includes \"%s\"", name.c_str(), b);
        }
    }
    CHECK(files >= 12, "found the web sources (%d files in %s)", files, dir.string().c_str());
    // the transmit headers refuse the web build
    for (const char* h : {"core/include/dect2/tx/tx.h", "core/include/dect2/tx/tx_iqfile.h"}) {
        const std::string s = readFile(root / h);
        CHECK(s.find("#ifdef ONAIR_WEB_SESSION\n#error") != std::string::npos, "%s has the web guard", h);
    }
    CHECK(readFile(root / "core/include/dect2/source/source.h").find("#ifndef ONAIR_WEB_SESSION") != std::string::npos, "source.h hides TxSink from web code");
    // the build compiles every web source with the guard
    const std::string cm = readFile(root / "CMakeLists.txt");
    CHECK(cm.find("ONAIR_WEB_SOURCES") != std::string::npos && cm.find("COMPILE_DEFINITIONS ONAIR_WEB_SESSION") != std::string::npos, "CMakeLists.txt builds server/web with ONAIR_WEB_SESSION");
}

static void testDefaults() {
    WebPolicy p;
    for (const auto& d : decoderCatalogue()) {
        if (d.cls == DecoderClass::Data) CHECK(p.decoderAllowed(d.kind), "%s on", d.kind);
        else CHECK(!p.decoderAllowed(d.kind), "%s must be off by default", d.kind);
    }
    CHECK(!p.legalAccepted(), "the notice is not accepted by default");
    onair_server::ConfigFile cf;
    cf.parse("web=1\n");
    bool made;
    WebSettings s = loadWebSettings(cf, made);
    CHECK(s.server.bind == "127.0.0.1" && !s.logIp && !s.directory.on && !s.server.adminLan && !s.server.plainInternet, "safe defaults: loopback, no IP logging, no directory, no admin from the network");
    CHECK(makeDirectoryHeartbeat() == nullptr, "no directory module in this build");
}

int main() {
    testProtocol();
    testSourceScan();
    testDefaults();
    return testSummary("web_safety");
}
