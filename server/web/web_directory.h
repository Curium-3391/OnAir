// OnAir Web directory: an optional heartbeat that tells a public directory of OnAir Web servers that this one exists (name, location, users,
// capture). Extension point only: the module itself (webonair-dir) is not part of this build, it is OFF by default, and nothing is sent
// anywhere unless the host turns it on with web_directory=1 and a directory URL. What it may send is the public /web/v1/info JSON:
// the public /web/v1/info fields, never a listener's address or what anyone listens to.
#pragma once
#include <functional>
#include <string>

namespace onair_web {

struct DirectoryConfig {
    bool on = false;            // web_directory
    std::string url;            // web_directory_url (https only)
    std::string publicUrl;      // web_public_url: how listeners reach this server (https://...)
    int intervalSec = 300;
};

class DirectoryHeartbeat {
public:
    virtual ~DirectoryHeartbeat() = default;
    // the module starts its own thread; `info` returns the public info JSON at each beat
    virtual bool start(const DirectoryConfig& c, std::function<std::string()> info, std::string& err) = 0;
    virtual void stop() = 0;
};

// The module's factory when it is built in; nullptr in this build (the server then says the directory is not available)
DirectoryHeartbeat* makeDirectoryHeartbeat();

} // namespace onair_web
