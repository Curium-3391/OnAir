// EDACS message layer (see edacs_msg.h: the layout is an assumption, not an established fact).
#include "dect2/edacs/edacs_msg.h"
#include <cstdio>

namespace dect2::edacs {

namespace {
uint32_t get(uint32_t w, Field f) { return (w >> f.shift) & ((1u << f.bits) - 1u); }
void put(uint32_t& w, Field f, uint32_t v) { w |= (v & ((1u << f.bits) - 1u)) << f.shift; }
}

Layout defaultLayout() { return Layout(); }

Msg parse(uint32_t w, const Layout& l) {
    Msg m;
    m.raw = w & 0xFFFFFFF;
    w = m.raw;
    const int t = (int)get(w, l.type);
    if (t == l.tGroupAssign) {
        m.type = MsgType::GroupAssign;
        m.lcn = (int)get(w, l.lcn); m.gid = (int)get(w, l.gid);
        const uint32_t f = get(w, l.flags);
        m.emergency = (f & 1) != 0; m.digital = (f & 2) != 0;
    } else if (t == l.tLidInfo) {
        m.type = MsgType::LidInfo;
        m.lid = (int)get(w, l.lid);
    } else if (t == l.tIndividualAssign) {
        m.type = MsgType::IndividualAssign;
        m.lcn = (int)get(w, l.lcn); m.gid = (int)get(w, l.target);
    } else if (t == l.tSiteId) {
        m.type = MsgType::SiteId;
        m.site = (int)get(w, l.site); m.system = (int)get(w, l.system);
    } else if (t == l.tControlChannel) {
        m.type = MsgType::ControlChannel;
        m.lcn = (int)get(w, l.lcn); m.nChannels = (int)get(w, l.nChannels);
    } else if (t == l.tCallEnd) {
        m.type = MsgType::CallEnd;
        m.lcn = (int)get(w, l.lcn);
    }
    if ((m.type == MsgType::GroupAssign || m.type == MsgType::IndividualAssign || m.type == MsgType::CallEnd || m.type == MsgType::ControlChannel) && m.lcn < 1)
        m.type = MsgType::Unknown;   // there is no LCN 0
    return m;
}

uint32_t build(const Msg& m, const Layout& l) {
    uint32_t w = 0;
    switch (m.type) {
    case MsgType::GroupAssign:
        put(w, l.type, (uint32_t)l.tGroupAssign); put(w, l.lcn, (uint32_t)m.lcn); put(w, l.gid, (uint32_t)m.gid);
        put(w, l.flags, (m.emergency ? 1u : 0u) | (m.digital ? 2u : 0u));
        break;
    case MsgType::LidInfo: put(w, l.type, (uint32_t)l.tLidInfo); put(w, l.lid, (uint32_t)m.lid); break;
    case MsgType::IndividualAssign: put(w, l.type, (uint32_t)l.tIndividualAssign); put(w, l.lcn, (uint32_t)m.lcn); put(w, l.target, (uint32_t)m.gid); break;
    case MsgType::SiteId: put(w, l.type, (uint32_t)l.tSiteId); put(w, l.site, (uint32_t)m.site); put(w, l.system, (uint32_t)m.system); break;
    case MsgType::ControlChannel: put(w, l.type, (uint32_t)l.tControlChannel); put(w, l.lcn, (uint32_t)m.lcn); put(w, l.nChannels, (uint32_t)m.nChannels); break;
    case MsgType::CallEnd: put(w, l.type, (uint32_t)l.tCallEnd); put(w, l.lcn, (uint32_t)m.lcn); break;
    default: break;
    }
    return w;
}

const char* typeName(MsgType t) {
    switch (t) {
    case MsgType::GroupAssign: return "group call assignment";
    case MsgType::LidInfo: return "unit ID";
    case MsgType::IndividualAssign: return "individual call assignment";
    case MsgType::SiteId: return "site ID";
    case MsgType::ControlChannel: return "control channel";
    case MsgType::CallEnd: return "call end";
    default: return "unknown";
    }
}

std::string describe(const Msg& m) {
    char b[120];
    switch (m.type) {
    case MsgType::GroupAssign: snprintf(b, sizeof b, "group call, LCN %d, group %d%s%s", m.lcn, m.gid, m.emergency ? ", emergency" : "", m.digital ? ", digital" : ""); break;
    case MsgType::LidInfo: snprintf(b, sizeof b, "unit %d", m.lid); break;
    case MsgType::IndividualAssign: snprintf(b, sizeof b, "individual call, LCN %d, unit %d", m.lcn, m.gid); break;
    case MsgType::SiteId: snprintf(b, sizeof b, "site %d, system %d", m.site, m.system); break;
    case MsgType::ControlChannel: snprintf(b, sizeof b, "control channel LCN %d, %d channels", m.lcn, m.nChannels); break;
    case MsgType::CallEnd: snprintf(b, sizeof b, "call end, LCN %d", m.lcn); break;
    default: snprintf(b, sizeof b, "not interpreted"); break;
    }
    return b;
}

} // namespace dect2::edacs
