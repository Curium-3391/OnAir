#include "web_directory_proto.h"
#include "dect2/mesh/mesh_crypto.h"
#include "web_json.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace onair_web {

namespace mc = dect2::meshcrypto;

std::string dirFreqText(const std::vector<std::pair<long long, long long>>& f) {
    std::string s;
    for (size_t i = 0; i < f.size(); i++) s += (i ? ";" : "") + std::to_string(f[i].first) + "-" + std::to_string(f[i].second);
    return s;
}

std::string dirCanonical(const DirRecord& r) {
    std::string s = "OnAir-Directory-v1\n";
    auto kv = [&](const char* k, const std::string& v) { s += std::string(k) + "=" + v + "\n"; };
    kv("action", r.action);
    kv("id", r.id);
    kv("pub", r.pub);
    kv("ts", std::to_string(r.ts));
    if (r.action == "delete") return s;
    kv("name", r.name);
    kv("url", r.url);
    kv("loc", r.loc);
    kv("freq", dirFreqText(r.freq));
    kv("radio", r.radio);
    kv("cap", std::to_string(r.cap));
    kv("users", std::to_string(r.users));
    kv("interval", std::to_string(r.interval));
    kv("ver", r.ver);
    kv("contact", r.contact);
    kv("desc", r.desc);
    kv("reach", std::to_string(r.reach));
    return s;
}

std::string dirBodyJson(const DirRecord& r, const std::string& sigHex) {
    auto q = [](const std::string& v) { return "\"" + jsonEsc(v) + "\""; };
    std::string s = "{\"v\":1,\"action\":" + q(r.action) + ",\"id\":" + q(r.id) + ",\"pub\":" + q(r.pub) + ",\"ts\":" + std::to_string(r.ts);
    if (r.action != "delete") {
        s += ",\"name\":" + q(r.name) + ",\"url\":" + q(r.url) + ",\"loc\":" + q(r.loc) + ",\"freq\":[";
        for (size_t i = 0; i < r.freq.size(); i++) s += (i ? "," : "") + std::string("[") + std::to_string(r.freq[i].first) + "," + std::to_string(r.freq[i].second) + "]";
        s += "],\"radio\":" + q(r.radio) + ",\"cap\":" + std::to_string(r.cap) + ",\"users\":" + std::to_string(r.users) + ",\"interval\":" + std::to_string(r.interval) +
             ",\"ver\":" + q(r.ver) + ",\"contact\":" + q(r.contact) + ",\"desc\":" + q(r.desc) + ",\"reach\":" + std::to_string(r.reach);
    }
    return s + ",\"sig\":" + q(sigHex) + "}";
}

std::string dirPubHex(const uint8_t seed[32]) {
    uint8_t pub[32];
    mc::ed25519PublicKey(seed, pub);
    return mc::hexEncode(pub, 32);
}

std::string dirSign(const uint8_t seed[32], const DirRecord& r) {
    const std::string t = dirCanonical(r);
    uint8_t sig[64];
    mc::ed25519Sign(seed, (const uint8_t*)t.data(), t.size(), sig);
    return mc::hexEncode(sig, 64);
}

bool dirVerify(const std::string& pubHex, const std::string& sigHex, const DirRecord& r) {
    std::vector<uint8_t> pub, sig;
    if (!mc::hexDecode(pubHex, pub) || pub.size() != 32 || !mc::hexDecode(sigHex, sig) || sig.size() != 64) return false;
    const std::string t = dirCanonical(r);
    return mc::ed25519Verify(sig.data(), (const uint8_t*)t.data(), t.size(), pub.data());
}

std::string dirIdFromPub(const std::string& pubHex) {
    std::vector<uint8_t> pub;
    if (!mc::hexDecode(pubHex, pub) || pub.size() != 32) return "";
    uint8_t h[32];
    mc::sha256(pub.data(), 32, h);
    return "onair-" + mc::hexEncode(h, 8);
}

std::string dirClean(const std::string& s, size_t maxBytes) {
    std::string t;
    for (size_t i = 0; i < s.size(); i++) {
        const unsigned char c = (unsigned char)s[i];
        if (c < 0x20 || c == 0x7f) continue;
        if (c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] >= 0x80 && (unsigned char)s[i + 1] <= 0x9F) { i++; continue; }   // C1 controls U+0080..U+009F
        if (c == 0xE2 && i + 2 < s.size() && (unsigned char)s[i + 1] == 0x80 && ((unsigned char)s[i + 2] == 0xA8 || (unsigned char)s[i + 2] == 0xA9)) { i += 2; continue; }   // U+2028 / U+2029
        t += (char)c;
    }
    size_t a = 0, b = t.size();
    while (a < b && t[a] == ' ') a++;
    while (b > a && t[b - 1] == ' ') b--;
    t = t.substr(a, b - a);
    if (t.size() > maxBytes) {
        size_t n = maxBytes;
        while (n > 0 && ((unsigned char)t[n] & 0xC0) == 0x80) n--;   // do not cut inside a multi-byte character
        t = t.substr(0, n);
        while (!t.empty() && t.back() == ' ') t.pop_back();
    }
    return t;
}

static bool hostChars(const std::string& h) {
    if (h.empty()) return false;
    for (char c : h) if (!(isalnum((unsigned char)c) || c == '.' || c == '-' || c == ':' || c == '[' || c == ']')) return false;
    return true;
}

bool dirNormalizeUrl(const std::string& in, std::string& out, std::string& err) {
    if (in.size() > 200) { err = "the address is longer than 200 characters"; return false; }
    for (unsigned char c : in) if (c <= 0x20 || c == 0x7f) { err = "the address has spaces or control characters"; return false; }
    std::string s = in;
    std::string scheme;
    if (s.rfind("https://", 0) == 0 || s.rfind("HTTPS://", 0) == 0) scheme = "https";
    else if (s.rfind("http://", 0) == 0 || s.rfind("HTTP://", 0) == 0) { err = "the address must start with https:// (the directory lists HTTPS servers)"; return false; }
    else { err = "the address must start with https://"; return false; }
    s = s.substr(8);
    if (s.find_first_of("?#@\\") != std::string::npos) { err = "the address must not have a user name, query or fragment"; return false; }
    const size_t slash = s.find('/');
    std::string host = s.substr(0, slash), path = slash == std::string::npos ? "/" : s.substr(slash);
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return (char)tolower(c); });
    if (!hostChars(host)) { err = "the host name has odd characters"; return false; }
    if (host.size() > 4 && host.compare(host.size() - 4, 4, ":443") == 0 && host.find(']') == std::string::npos) host.resize(host.size() - 4);
    for (char c : path) if (!(isalnum((unsigned char)c) || c == '.' || c == '_' || c == '~' || c == '-' || c == '/')) { err = "the path has odd characters"; return false; }
    if (path.back() != '/') path += '/';
    out = scheme + "://" + host + path;
    return true;
}

bool dirNormalizeLoc(const std::string& in, int decimals, std::string& out, std::string& err) {
    std::string s = dirClean(in, 64);
    out.clear();
    if (s.empty()) return true;
    decimals = std::clamp(decimals, 0, 5);
    const size_t comma = s.find(',');
    if (comma != std::string::npos) {
        char *e1 = nullptr, *e2 = nullptr;
        const std::string a = s.substr(0, comma), b = s.substr(comma + 1);
        const double lat = strtod(a.c_str(), &e1), lon = strtod(b.c_str(), &e2);
        if (a.empty() || b.empty() || *e1 || *e2 || !std::isfinite(lat) || !std::isfinite(lon) || lat < -90 || lat > 90 || lon < -180 || lon > 180) {
            err = "the location must be \"latitude,longitude\" in decimal degrees or a Maidenhead locator";
            return false;
        }
        char buf[64];
        snprintf(buf, sizeof buf, "%.*f,%.*f", decimals, lat, decimals, lon);
        out = buf;
        return true;
    }
    // Maidenhead: AA00, AA00aa or AA00aa00
    if (s.size() != 4 && s.size() != 6 && s.size() != 8) { err = "a locator has 4, 6 or 8 characters (e.g. KO24 or KO24pq)"; return false; }
    for (size_t i = 0; i < s.size(); i++) {
        const char c = (char)toupper((unsigned char)s[i]);
        const bool ok = (i < 2) ? (c >= 'A' && c <= 'R') : (i < 4) ? (c >= '0' && c <= '9') : (i < 6) ? (c >= 'A' && c <= 'X') : (c >= '0' && c <= '9');
        if (!ok) { err = "that is not a Maidenhead locator"; return false; }
    }
    out = s;
    return true;
}

bool dirParseFreqMHz(const std::string& in, std::vector<std::pair<long long, long long>>& out, std::string& err) {
    out.clear();
    size_t pos = 0;
    while (pos < in.size()) {
        size_t end = in.find(',', pos);
        if (end == std::string::npos) end = in.size();
        const std::string part = in.substr(pos, end - pos);
        pos = end + 1;
        if (part.find_first_not_of(' ') == std::string::npos) continue;
        const size_t dash = part.find('-');
        char *e1 = nullptr, *e2 = nullptr;
        const std::string sa = dash == std::string::npos ? "" : part.substr(0, dash), sb = dash == std::string::npos ? "" : part.substr(dash + 1);
        const double a = sa.empty() ? NAN : strtod(sa.c_str(), &e1);
        const double b = sb.empty() ? NAN : strtod(sb.c_str(), &e2);
        if (dash == std::string::npos || (e1 && *e1 && *e1 != ' ') || (e2 && *e2 && *e2 != ' ') || !std::isfinite(a) || !std::isfinite(b) || a < 0 || b < a || b > 300000) { err = "web_directory_freq is a list like 88-108,1-30 (MHz)"; return false; }
        out.push_back({std::llround(a * 1e6), std::llround(b * 1e6)});
        if (out.size() > 8) { err = "web_directory_freq has more than 8 ranges"; return false; }
    }
    return true;
}

bool dirBuildRecord(const DirectoryConfig& c, const std::string& infoJson, long long ts, const std::string& id, const std::string& pubHex, DirRecord& out, std::string& err) {
    Json j;
    std::string e;
    if (!jsonParse(infoJson, j, e)) { err = "own info is not valid JSON: " + e; return false; }
    DirRecord r;
    r.id = id;
    r.pub = pubHex;
    r.ts = ts;
    const Json* n = j.get("name");
    r.name = dirClean(c.name.empty() ? (n && n->kind == Json::Str ? n->str : std::string()) : c.name, 48);
    if (r.name.empty()) { err = "web_name is empty"; return false; }
    if (!dirNormalizeUrl(c.publicUrl, r.url, err)) { err = "web_public_url: " + err; return false; }
    if (!dirNormalizeLoc(c.loc, c.locDecimals, r.loc, err)) { err = "web_directory_loc: " + err; return false; }
    if (!c.freq.empty()) {
        if (!dirParseFreqMHz(c.freq, r.freq, err)) return false;
    } else if (const Json* cap = j.get("capture")) {
        const Json* cf = cap->get("centre_hz");
        const Json* rt = cap->get("rate_hz");
        if (cf && rt && cf->kind == Json::Num && rt->kind == Json::Num && rt->num > 0 && cf->num > 0) {
            const long long lo = std::max(0LL, std::llround(cf->num - rt->num / 2)), hi = std::llround(cf->num + rt->num / 2);
            r.freq.push_back({lo, hi});
        }
    }
    r.radio = dirClean(c.radio, 48);
    const Json* mu = j.get("max_users");
    const Json* us = j.get("users");
    r.cap = mu && mu->kind == Json::Num ? std::clamp<long long>((long long)mu->num, 0, 100000) : 0;
    r.users = us && us->kind == Json::Num ? std::clamp<long long>((long long)us->num, 0, 100000) : 0;
    if (r.cap > 0 && r.users > r.cap) r.users = r.cap;
    r.interval = std::clamp(c.intervalSec, 60, 3600);
    r.ver = dirClean(c.version, 24);
    r.contact = dirClean(c.contact, 80);
    r.desc = dirClean(c.desc, 280);
    r.reach = c.reach ? 1 : 0;
    out = r;
    return true;
}

std::string dirAck(const DirRecord& r, const std::string& directoryUrl) {
    DirRecord a = r;
    a.ts = 0;
    a.users = 0;
    a.freq.clear();
    a.ver.clear();
    const std::string t = dirCanonical(a) + "directory=" + directoryUrl + "\n";
    uint8_t h[32];
    mc::sha256((const uint8_t*)t.data(), t.size(), h);
    return mc::hexEncode(h, 6);   // 12 characters: short enough to type, long enough that it is not guessed by accident
}

std::string dirPreview(const DirRecord& r, const std::string& directoryUrl) {
    std::string s = "OnAir Web directory: every " + std::to_string(r.interval) + " s this server will send an HTTPS POST to\n  " + directoryUrl +
                    (directoryUrl.empty() || directoryUrl.back() != '/' ? "/" : "") + "api/v1/heartbeat\nwith exactly this JSON (time, users and capture coverage change at each beat):\n  ";
    s += dirBodyJson(r, "<signature made with this server's key>") + "\n";
    s += "Nothing else is sent: no listener addresses, names or what anyone listens to, only the number of users.\n";
    if (r.reach) s += "web_directory_reach=1: the directory may fetch " + r.url + "web/v1/info about once an hour to check that the server answers.\n";
    return s;
}

} // namespace onair_web
