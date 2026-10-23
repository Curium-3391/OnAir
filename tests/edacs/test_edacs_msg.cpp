// EDACS message layer: build / parse round trips of the assumed layout (edacs_msg.h), a changed layout, words that mean nothing. The layout is an assumption, so this
// proves the builder and the parser agree and that the layout is one table, not that any real system uses it.
#include "dect2/edacs/edacs_msg.h"
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
using namespace dect2;
#include "../common/test_util.h"

int main() {
    const edacs::Layout l = edacs::defaultLayout();
    {
        edacs::Msg m;
        m.type = edacs::MsgType::GroupAssign; m.lcn = 17; m.gid = 54321; m.emergency = true; m.digital = true;
        edacs::Msg r = edacs::parse(edacs::build(m, l), l);
        CHECK(r.type == m.type && r.lcn == 17 && r.gid == 54321 && r.emergency && r.digital, "group assignment");
        m.emergency = false; m.digital = false;
        r = edacs::parse(edacs::build(m, l), l);
        CHECK(!r.emergency && !r.digital, "flags");
        m = edacs::Msg(); m.type = edacs::MsgType::LidInfo; m.lid = 1048575;
        r = edacs::parse(edacs::build(m, l), l);
        CHECK(r.type == m.type && r.lid == 1048575, "unit id");
        m = edacs::Msg(); m.type = edacs::MsgType::IndividualAssign; m.lcn = 30; m.gid = 262143;
        r = edacs::parse(edacs::build(m, l), l);
        CHECK(r.type == m.type && r.lcn == 30 && r.gid == 262143, "individual assignment");
        m = edacs::Msg(); m.type = edacs::MsgType::SiteId; m.site = 200; m.system = 65000;
        r = edacs::parse(edacs::build(m, l), l);
        CHECK(r.type == m.type && r.site == 200 && r.system == 65000, "site id");
        m = edacs::Msg(); m.type = edacs::MsgType::ControlChannel; m.lcn = 1; m.nChannels = 24;
        r = edacs::parse(edacs::build(m, l), l);
        CHECK(r.type == m.type && r.lcn == 1 && r.nChannels == 24, "control channel");
        m = edacs::Msg(); m.type = edacs::MsgType::CallEnd; m.lcn = 9;
        r = edacs::parse(edacs::build(m, l), l);
        CHECK(r.type == m.type && r.lcn == 9, "call end");
    }
    {   // LCN 0 does not exist; type 0 and the types past the table mean nothing
        edacs::Msg m;
        m.type = edacs::MsgType::GroupAssign; m.lcn = 0; m.gid = 5;
        CHECK(edacs::parse(edacs::build(m, l), l).type == edacs::MsgType::Unknown, "LCN 0 accepted");
        CHECK(edacs::parse(0x0ABCDEF, l).type == edacs::MsgType::Unknown, "type 0 interpreted");
        CHECK(edacs::parse(0xF000000, l).type == edacs::MsgType::Unknown, "type 15 interpreted");
        std::mt19937 rng(3);
        int known = 0;
        for (int i = 0; i < 3200; i++) known += edacs::parse((uint32_t)rng() & 0xFFFFFFF, l).type != edacs::MsgType::Unknown;
        CHECK(known > 1000 && known < 1400, "%d of 3200 random words interpreted (6 of 16 types expected, less LCN 0)", known);
    }
    {   // the layout is a table: move the group id and the type and the same builder / parser follow
        edacs::Layout o = l;
        o.type = {0, 4}; o.lcn = {4, 5}; o.gid = {9, 16};
        edacs::Msg m;
        m.type = edacs::MsgType::GroupAssign; m.lcn = 12; m.gid = 4242;
        const uint32_t w = edacs::build(m, o);
        const edacs::Msg r = edacs::parse(w, o);
        CHECK(r.type == m.type && r.lcn == 12 && r.gid == 4242, "moved layout");
        CHECK(edacs::parse(w, l).gid != 4242, "the default layout read the moved word as its own");
    }
    CHECK(std::string(edacs::describe(edacs::parse(edacs::build([] { edacs::Msg m; m.type = edacs::MsgType::GroupAssign; m.lcn = 3; m.gid = 77; return m; }(), l), l))) == "group call, LCN 3, group 77", "describe");
    return testSummary("test_edacs_msg");
}
