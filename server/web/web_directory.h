// OnAir Web directory: an optional heartbeat that tells a public directory of OnAir Web servers that this one exists (name, location, coverage,
// user counts). It is OFF by default and nothing is sent anywhere unless the host turns it on with web_directory=1, gives a directory URL
// (https only) and confirms what will be sent with web_directory_ack (the server prints the exact JSON and the code first). It never sends a
// listener's address or what anyone listens to, only the number of users. The messages are signed with an Ed25519 key made on first start
// (docs/ONAIR-DIRECTORY.md).
#pragma once
#include <functional>
#include <string>

namespace onair_web {

struct DirectoryConfig {
    bool on = false;            // web_directory
    std::string url;            // web_directory_url (https only)
    std::string publicUrl;      // web_public_url: how listeners reach this server (https://...)
    int intervalSec = 300;      // web_directory_interval (60..3600)
    std::string keyFile;        // web_directory_key: where the Ed25519 seed is kept (set by the server when empty)
    std::string loc;            // web_directory_loc: "lat,lon" or a Maidenhead locator; empty: no location is sent
    int locDecimals = 2;        // web_directory_loc_decimals: 0..5 (2 is about a kilometre)
    std::string freq;           // web_directory_freq: "88-108,1.0-30" MHz; empty: the capture window
    std::string radio;          // web_directory_radio: "HackRF Pro", "RTL-SDR v4", ...
    std::string contact;        // web_directory_contact (optional, shown publicly)
    std::string desc;           // web_directory_desc
    bool reach = false;         // web_directory_reach: let the directory check that /web/v1/info answers (opt in)
    std::string ack;            // web_directory_ack: the code printed with the preview
    std::string version;        // software version (set by the server)
    std::string name;           // not a setting: filled from the live info when empty
};

class DirectoryHeartbeat {
public:
    virtual ~DirectoryHeartbeat() = default;
    // Prepares the key and checks the settings. Returns false (err says why and what to do) when nothing will be sent: no HTTPS in the build, no
    // confirmation yet, a bad address. Otherwise starts its own thread; `info` returns the public info JSON at each beat.
    virtual bool start(const DirectoryConfig& c, std::function<std::string()> info, std::string& err) = 0;
    virtual void stop() = 0;
    // Asks the directory to remove this server now (signed delete); called by `onair-server --directory-remove` (needs the directory URL and the key file, not the confirmation). Blocking.
    virtual bool removeNow(const DirectoryConfig& c, std::string& err) = 0;
};

// The module's factory (always built in; start() says when the build has no TLS client)
DirectoryHeartbeat* makeDirectoryHeartbeat();

} // namespace onair_web
