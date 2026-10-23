// LAN swarm discovery over UDP multicast. See dect2/net/swarm_net.h. NOT yet compiled or run (written while builds were forbidden).
#include "dect2/net/swarm_net.h"
#include <atomic>
#include <chrono>
#include <thread>

#ifdef _WIN32
namespace dect2::swarm {
struct SwarmNet::Impl {};
SwarmNet::SwarmNet() : p_(new Impl) {}
SwarmNet::~SwarmNet() {}
bool SwarmNet::start(const std::string&, AnnounceFn, std::string& err) { err = "LAN swarm discovery is not available on Windows yet"; return false; }
void SwarmNet::stop() {}
std::vector<Node> SwarmNet::nodes() { return {}; }
NetStats SwarmNet::stats() const { return {}; }
}   // namespace dect2::swarm
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace dect2::swarm {
namespace {
struct Iface { uint32_t ip = 0, mask = 0; };
int64_t nowMs() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

std::vector<Iface> privateInterfaces() {
    std::vector<Iface> v;
    ifaddrs* l = nullptr;
    if (getifaddrs(&l) != 0) return v;
    for (ifaddrs* a = l; a; a = a->ifa_next) {
        if (!a->ifa_addr || !a->ifa_netmask || a->ifa_addr->sa_family != AF_INET) continue;
        if (!(a->ifa_flags & IFF_UP) || (a->ifa_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)) || !(a->ifa_flags & IFF_MULTICAST)) continue;
        Iface f;
        f.ip = ntohl(((sockaddr_in*)a->ifa_addr)->sin_addr.s_addr);
        f.mask = ntohl(((sockaddr_in*)a->ifa_netmask)->sin_addr.s_addr);
        if (isPrivateIpv4(f.ip) && (f.ip >> 24) != 127 && onLocalSubnet(f.ip, f.ip, f.mask)) v.push_back(f);   // never a public address
    }
    freeifaddrs(l);
    return v;
}
}   // namespace

struct SwarmNet::Impl {
    std::mutex mu;
    std::thread th;
    std::atomic<bool> stop{false};
    NodeTable table;
    NetStats st;
    int rx = -1, tx = -1;
    std::vector<Iface> ifs;
    AnnounceFn announce;

    void closeAll() {
        if (rx >= 0) ::close(rx);
        if (tx >= 0) ::close(tx);
        rx = tx = -1;
    }
    void run() {
        int64_t nextSend = 0;
        while (!stop) {
            int64_t now = nowMs();
            if (now >= nextSend) {
                nextSend = now + 5000;
                Announce a = announce ? announce() : Announce();
                std::string pkt = encodeAnnounce(a);
                for (auto& f : ifs) {
                    in_addr ia; ia.s_addr = htonl(f.ip);
                    if (setsockopt(tx, IPPROTO_IP, IP_MULTICAST_IF, &ia, sizeof ia) != 0) continue;
                    sockaddr_in to{}; to.sin_family = AF_INET; to.sin_port = htons(kPort); inet_pton(AF_INET, kGroup, &to.sin_addr);
                    if (sendto(tx, pkt.data(), pkt.size(), 0, (sockaddr*)&to, sizeof to) > 0) { std::lock_guard<std::mutex> g(mu); st.sent++; }
                }
                std::lock_guard<std::mutex> g(mu);
                table.expire(now);
            }
            pollfd p{rx, POLLIN, 0};
            if (poll(&p, 1, 500) <= 0 || !(p.revents & POLLIN)) continue;
            uint8_t buf[kMaxAnnounceBytes + 64];
            sockaddr_in from{}; socklen_t fl = sizeof from;
            ssize_t n = recvfrom(rx, buf, sizeof buf, 0, (sockaddr*)&from, &fl);
            if (n <= 0) continue;
            uint32_t src = ntohl(from.sin_addr.s_addr);
            Announce a;
            std::string err;
            bool ok = n <= (ssize_t)kMaxAnnounceBytes && parseAnnounce(buf, (size_t)n, a, err);
            std::lock_guard<std::mutex> g(mu);
            if (!ok) { st.rejected++; continue; }
            bool took = false;
            for (auto& f : ifs) {
                if (src == f.ip) continue;   // our own packet looped back
                NodeTable::Result r = table.update(a, src, f.ip, f.mask, nowMs());
                if (r == NodeTable::WrongSubnet) continue;   // try the next interface
                took = (r == NodeTable::Accepted);
                break;
            }
            if (took) st.received++; else st.rejected++;
        }
    }
};

SwarmNet::SwarmNet() : p_(new Impl) {}
SwarmNet::~SwarmNet() { stop(); }

bool SwarmNet::start(const std::string& ownId, AnnounceFn announce, std::string& err) {
    stop();
    Impl& d = *p_;
    d.ifs = privateInterfaces();
    if (d.ifs.empty()) { err = "no private network interface found (LAN swarm stays off)"; return false; }
    d.rx = socket(AF_INET, SOCK_DGRAM, 0);
    d.tx = socket(AF_INET, SOCK_DGRAM, 0);
    if (d.rx < 0 || d.tx < 0) { err = "cannot open sockets"; d.closeAll(); return false; }
    int one = 1;
    setsockopt(d.rx, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
#ifdef SO_REUSEPORT
    setsockopt(d.rx, SOL_SOCKET, SO_REUSEPORT, &one, sizeof one);
#endif
    sockaddr_in me{}; me.sin_family = AF_INET; me.sin_port = htons(kPort); me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(d.rx, (sockaddr*)&me, sizeof me) != 0) { err = "cannot listen on the swarm port"; d.closeAll(); return false; }
    int joined = 0;
    for (auto& f : d.ifs) {
        ip_mreq m{};
        inet_pton(AF_INET, kGroup, &m.imr_multiaddr);
        m.imr_interface.s_addr = htonl(f.ip);
        if (setsockopt(d.rx, IPPROTO_IP, IP_ADD_MEMBERSHIP, &m, sizeof m) == 0) joined++;
    }
    if (!joined) { err = "cannot join the multicast group"; d.closeAll(); return false; }
    unsigned char ttl = 1, loop = 0;   // TTL 1: never leaves the local network; no loopback to ourselves
    setsockopt(d.tx, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof ttl);
    setsockopt(d.tx, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof loop);
    {
        std::lock_guard<std::mutex> g(d.mu);
        d.table = NodeTable(ownId);
        d.st = NetStats();
        d.st.running = true;
        d.st.interfaces = (int)d.ifs.size();
    }
    d.announce = std::move(announce);
    d.stop = false;
    d.th = std::thread([&d] { d.run(); });
    return true;
}

void SwarmNet::stop() {
    Impl& d = *p_;
    d.stop = true;
    if (d.th.joinable()) d.th.join();
    d.closeAll();
    std::lock_guard<std::mutex> g(d.mu);
    d.st.running = false;
}

std::vector<Node> SwarmNet::nodes() {
    std::lock_guard<std::mutex> g(p_->mu);
    return p_->table.list();
}
NetStats SwarmNet::stats() const {
    std::lock_guard<std::mutex> g(p_->mu);
    return p_->st;
}

}   // namespace dect2::swarm
#endif
