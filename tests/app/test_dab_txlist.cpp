// DAB transmitter lists (app/dab/dab_txlist.h): reading a line, writing one, importing a list into the own one (#249)
#include "../../app/dab/dab_txlist.h"
#include <cstdio>
static int fails = 0;
static void check(bool ok, const char* w) { if (!ok) { printf("FAIL: %s\n", w); fails++; } }
int main() {
    using namespace dabtx;
    Row r;
    check(parse("", r) == Line::Skip && parse("# a comment", r) == Line::Skip && parse("eid,main,sub,lat,lon,site,power_kw,channel_mhz", r) == Line::Skip, "comments and the header");
    check(parse("\xEF\xBB\xBF" "eid,main,sub,lat,lon", r) == Line::Skip, "a header behind a byte order mark");
    check(parse("10BC,1,3,47.70361,12.01194,Wendelstein,10,178.352", r) == Line::Ok, "a full line");
    check(r.eid == 0x10BC && r.mainId == 1 && r.subId == 3 && r.site == "Wendelstein" && r.powerKw == 10 && r.channelMhz == 178.352, "its fields");
    check(parse("10bc\t1\t3\t47.7\t12.0\tWendelstein", r) == Line::Ok && r.eid == 0x10BC, "a TSV, lower-case hex");
    check(parse("10BC,1,3,\"48.1,\",11.5", r) == Line::Bad, "a latitude with junk");
    check(parse("10BC,1,3,,,,,178.352,DR Deutschland,12.5", r) == Line::NoPosition && r.channelMhz == 178.352, "an exported row without a position");
    check(parse("10BC,1,3,48.1,,x", r) == Line::NoPosition, "only a latitude: no position (not placed at longitude 0)");
    check(parse("10BC,70,3,48,11", r) == Line::Bad && parse("10BC,1,24,48,11", r) == Line::Bad, "TII ids out of range");
    check(parse("XYZ,1,3,48,11", r) == Line::Bad && parse("1FFFF,1,3,48,11", r) == Line::Bad, "a bad ensemble id");
    check(parse("10BC,1,3,91,11", r) == Line::Bad, "a latitude beyond the pole");
    check(parse("10BC,1,3", r) == Line::Bad, "too few columns");

    Row w; w.eid = 0x10BC; w.mainId = 1; w.subId = 3; w.lat = 47.70361; w.lon = 12.01194; w.site = "Wendelstein, \"Sender\""; w.channelMhz = 178.352;
    const std::string line = format(w);
    Row back;
    check(parse(line, back) == Line::Ok && back.site == "Wendelstein, Sender" && back.eid == 0x10BC && back.subId == 3 && std::fabs(back.lat - 47.70361) < 1e-6, "a written line reads back");

    const std::string own = "# my list\neid,main,sub,lat,lon,site,power_kw,channel_mhz\n10BC,1,3,47.0,12.0,old place,,\n1001,5,5,50.0,8.0,Feldberg,,\n";
    const std::string imp = "eid,main,sub,lat,lon,site,power_kw,channel_mhz,ensemble,level_db\n"
                            "10BC,1,3,47.70361,12.01194,Wendelstein,10,178.352,\"DR Deutschland\",12.5\n"
                            "10BC,2,7,,,\"\",,178.352,\"DR Deutschland\",4.0\n"
                            "D210,4,1,48.1,11.5,Olympiaturm,,202.928,\"BR Bayern\",20\n"
                            "garbage\n";
    MergeStats st;
    const std::string m = merge(own, imp, st);
    check(st.added == 1 && st.replaced == 1 && st.noPosition == 1 && st.bad == 1, "import counts");
    check(m.find("# my list") == 0, "the own comments stay");
    check(m.find("old place") == std::string::npos && m.find("Wendelstein") != std::string::npos, "a transmitter of both is updated");
    check(m.find("Feldberg") != std::string::npos && m.find("Olympiaturm") != std::string::npos, "the own others stay, the new one is added");
    check(m.find("10BC,2,7") == std::string::npos, "no row without a position is written");
    int ok = 0;
    {
        size_t p = 0;
        while (p < m.size()) { const size_t e = m.find('\n', p); Row x; if (parse(m.substr(p, e - p), x) == Line::Ok) ok++; p = e == std::string::npos ? m.size() : e + 1; }
    }
    check(ok == 3, "the merged list reads back with three transmitters");
    const std::string fresh = merge("", imp, st);
    check(fresh.find("eid,main,sub") == 0 && st.added == 2, "into no own file yet: a header and the transmitters");
    if (!fails) printf("dab transmitter lists: ok\n");
    return fails ? 1 : 0;
}
