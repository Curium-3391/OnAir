// EDACS control channel message layer: what a 28-bit data word of the outbound control channel means, and the same words built for the test signal.
//
// NOT ESTABLISHED FROM PUBLIC SOURCES. The framing (edacs_proto.h) is described in a public patent; the meaning of the bits inside a word is not in anything
// this program is built from. The layout below is THIS PROGRAM'S OWN ASSUMPTION, chosen so that the rest of the chain (call table, talkgroup table, site
// information, following a voice channel) can be built and tested; it is not the layout of any real EDACS system (Standard, Networked or ProVoice). A real
// system will therefore show wrong or no fields here; the raw words stay visible next to the decoded ones, and the layout is one table (Layout) so that a
// verified one is a one-place change (EdacsReceiver::setLayout). The activity scan of the voice channels (a carrier on an LCN of the frequency table) does not
// depend on any of this. ProVoice (the manufacturer's digital voice) is not decoded: there is no codec for it in this program.
#pragma once
#include <cstdint>
#include <string>

namespace dect2::edacs {

struct Field { int shift = 0, bits = 0; };

struct Layout {
    Field type{24, 4};
    // message types
    int tGroupAssign = 1, tLidInfo = 2, tIndividualAssign = 3, tSiteId = 4, tControlChannel = 5, tCallEnd = 6;
    Field lcn{19, 5}, gid{3, 16}, flags{0, 3};          // group assignment (flags: bit 0 emergency, bit 1 digital voice)
    Field lid{4, 20};                                   // source unit of the call, in the information word that follows an assignment
    Field target{0, 19};                                // individual assignment: the called unit
    Field site{16, 8}, system{0, 16};                   // site information
    Field nChannels{14, 5};                             // control channel word: how many channels the site has
};

enum class MsgType { Unknown, GroupAssign, LidInfo, IndividualAssign, SiteId, ControlChannel, CallEnd };

struct Msg {
    MsgType type = MsgType::Unknown;
    int lcn = -1, gid = -1, lid = -1, site = -1, system = -1, nChannels = -1;
    bool emergency = false, digital = false;
    uint32_t raw = 0;
};

Layout defaultLayout();
Msg parse(uint32_t word28, const Layout& l);
uint32_t build(const Msg& m, const Layout& l);          // the test signal
const char* typeName(MsgType t);
std::string describe(const Msg& m);                     // one line of text

} // namespace dect2::edacs
