// OnAir Web: what the host lets web listeners use, and who may connect.
//
// SAFETY AND LEGAL DEFAULTS (hard rules; tests/server/test_web_safety.cpp checks them):
//  * a web session can never transmit, arm a transmitter, import keys, decrypt, or write to the host's disk: the protocol has no such
//    message (web_proto.cpp answers them with "forbidden"), and the web code is compiled with ONAIR_WEB_SESSION, which makes the transmit
//    headers (dect2/tx/*.h) a compile error, so no transmit function can even be linked into it;
//  * the web-safe modes are an ALLOW-LIST (analog audio, spectrum, ADS-B, AIS, APRS, ACARS / VDL2, FT8 / FT4 / WSPR, SSTV, weather fax, satellite
//    images). Everything else is OFF for web listeners, including digital voice (DMR, P25, NXDN, TETRA, dPMR, D-STAR, Yaesu System Fusion),
//    pagers (POCSAG, FLEX), mesh, LoRa, FreeDV, key-related modes and every mode that is added later;
//    the host can enable each one only after accepting the legal notice (its version is stored in the config as web_legal_ack);
//  * encrypted traffic is never decoded, and the voice key store is not part of the server;
//  * the IP address of a listener is kept in memory for the per-address limits only; it is shown or logged only with web_log_ip=1.
#pragma once
#include "web_proto.h"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace onair_server { class ConfigFile; }

namespace onair_web {

enum class DecoderClass { Data, Voice, Pager, Other };   // Other: anything that is not on the web-safe allow-list (off for web listeners)

struct DecoderInfo { const char* kind; DecoderClass cls; const char* name; };
// The decoder kinds the policy knows (data ones on by default, voice and pager ones off)
const std::vector<DecoderInfo>& decoderCatalogue();
// The class of a decoder kind or an engine mode id ("dmr", "pocsag", "fusion", ...). FAIL CLOSED: only the kinds on the web-safe allow-list are
// Data (analog audio demodulators, the spectrum views and the public data modes: ADS-B, AIS, APRS, ACARS / VDL2 text, FT8 / FT4 / WSPR, SSTV,
// weather fax and weather-satellite images); a kind that is not known at all is Other, so a new mode is blocked until it is added to the list.
DecoderClass decoderClass(const std::string& kind);
// True when the kind is on one of the explicit lists (safe, voice, pager, or "known and restricted"). tests/server/test_web_safety.cpp checks
// that every mode id of the receiver registry is classified, so a new mode forces a decision here.
bool decoderKindClassified(const std::string& kind);
const char* className(DecoderClass c);

constexpr const char* kLegalNoticeVersion = "onair-web-legal-1";
const char* legalNoticeText();

class WebPolicy {
public:
    void load(const onair_server::ConfigFile& c);
    void save(onair_server::ConfigFile& c) const;
    bool demodAllowed(Demod d) const { return demods_.count(d) > 0; }
    bool decoderAllowed(const std::string& kind) const;
    // Turns a decoder kind on or off for web listeners. A voice or pager kind can be turned on only with ack == kLegalNoticeVersion.
    bool setDecoder(const std::string& kind, bool on, const std::string& ack, std::string& err);
    bool setDemod(Demod d, bool on);
    bool legalAccepted() const { return legalAck_ == kLegalNoticeVersion; }
    std::string json() const;   // {"demods":[...],"decoders":{"adsb":true,...},"legal_ack":bool,"legal_version":"..."}

private:
    std::set<Demod> demods_ = {Demod::Am, Demod::Nfm, Demod::Wfm, Demod::Usb, Demod::Lsb, Demod::Cw};
    std::set<std::string> dataOff_;       // data decoders the host turned off
    std::set<std::string> restrictedOn_;  // voice / pager decoders the host turned on (only honoured with the legal notice accepted)
    std::string legalAck_;
};

// ---------------------------------------------------------------- access
enum class AuthMode { Open, Password, Invite };
enum class UserClass { Public, Member, Invited };   // Public: open access; Member: knew the password; Invited: an invite token
const char* userClassName(UserClass c);

struct InviteToken { std::string token, label; int maxMinutes = 0; };   // maxMinutes 0: no time limit of its own

struct AuthConfig {
    AuthMode mode = AuthMode::Open;
    std::string password;              // AuthMode::Password (8 characters or more)
    std::vector<InviteToken> invites;  // valid in every mode
    std::string adminToken;            // the admin API (24 characters or more); empty: no admin API
};
bool authModeFromName(const std::string& s, AuthMode& m);
const char* authModeName(AuthMode m);

struct AuthResult { bool ok = false; UserClass cls = UserClass::Public; std::string label; int maxMinutes = 0; std::string why; };
AuthResult authenticate(const AuthConfig& a, const std::string& password, const std::string& invite);

std::string newToken(int bytes = 16);   // random, hex

// Failed logins per address: after 5 failures within 60 s the address is refused for 60 s. Bounded table (oldest entries go first).
class FailLimiter {
public:
    bool blocked(const std::string& ip, double now);
    void fail(const std::string& ip, double now);
    void clear(const std::string& ip);
private:
    struct E { int n = 0; double first = 0, until = 0; };
    std::map<std::string, E> t_;
};

} // namespace onair_web
