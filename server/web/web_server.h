// OnAir Web: the network side. One listening port (default 8073) serving
//   GET  /                       the browser UI: static files from web_root (web/onair-web/ by default; a placeholder page when it is missing)
//   GET  /web/v1/info            public server information (name, users, capture, auth mode); no addresses
//   GET  /web/v1/ws              the WebSocket of a listener (protocol: docs/ONAIR-WEB-PROTOCOL.md)
//   *    /web/v1/admin/...       the admin API (Authorization: Bearer <admin token>; from this computer only unless admin_lan)
// Hardening: request head <= 8 KB, body <= 4 KB, 5 s to send a request, connection limits (total and per address), WebSocket origin
// check, no CORS, strict Content-Security-Policy, static files only below the web root (no "..", no dot files, symlinks resolved and
// checked), constant-time token compares, failed logins throttled per address, plain HTTP refused to non-private addresses unless the host
// allowed it (use TLS or a reverse proxy for the internet).
#pragma once
#include "web_hub.h"
#include "web_policy.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace onair_web {

struct WebServerConfig {
    bool on = false;
    int port = 8073;                  // 0: any free port (tests)
    std::string bind = "127.0.0.1";   // "0.0.0.0": every interface
    std::string tlsCert, tlsKey;      // PEM files (a build with OpenSSL); both or neither
    bool plainInternet = false;       // allow plain HTTP from non-private addresses (not recommended)
    bool trustProxy = false;          // a reverse proxy on this computer: the listener's address comes from X-Forwarded-For (loopback peers only)
    std::string webRoot;              // static files; empty: the placeholder page
    std::vector<std::string> origins; // extra allowed WebSocket origins (e.g. "https://radio.example.org")
    AuthConfig auth;
    bool adminLan = false;            // admin API from other computers too (still needs the admin token)
    int maxConnections = 64, maxConnPerIp = 12;
    int helloTimeoutMs = 5000, pingSec = 15, keepaliveSec = 45;
    int msgPerSec = 20, msgBurst = 40;
};

struct WebServerStatus { bool listening = false, tls = false; int port = 0; std::string lastError; int connections = 0; uint64_t refused = 0; };

class WebServer {
public:
    explicit WebServer(WebHub& hub);
    ~WebServer();
    bool start(const WebServerConfig& c);   // false: status().lastError
    void stop();
    WebServerStatus status() const;
    // the admin API changed something the config file keeps (policy, invites): the host saves it
    void setOnChange(std::function<void(const WebPolicy&, const std::vector<InviteToken>&)> f);
    std::vector<InviteToken> invites() const;

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

// ---- helpers (public for the tests)
// The file below `root` a request path names, or "" when it is not allowed (traversal, dot files, odd characters, outside the root)
std::string staticPath(const std::string& root, const std::string& urlPath);
const char* contentType(const std::string& path);
bool privateAddress(const std::string& ipv4);   // 10/8, 172.16/12, 192.168/16, 169.254/16, 127/8, 100.64/10 (CGNAT)
std::string urlDecodeStrict(const std::string& s, bool& ok);   // %XX only ('+' stays), %00 and bad escapes: ok = false

} // namespace onair_web
