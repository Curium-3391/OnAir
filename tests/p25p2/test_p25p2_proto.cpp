// P25 Phase 2 protocol layer: ISCH, DUID, MAC PDU codewords (Reed-Solomon + CRC) with errors, MAC builders. The layout under test is this program's own
// (p25p2_proto.h): it proves the encoder and the decoder agree and the codes correct what they should, not that a real transmitter does the same.
#include "dect2/p25p2/p25p2_proto.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>
using namespace dect2;
#include "../common/test_util.h"

int main() {
    std::mt19937 rng(5);
    // ISCH
    {
        uint8_t d[20];
        p25p2::ischSync(d);
        CHECK(p25p2::syncDistance(d) == 0, "sync distance of the sync word");
        for (int s = 0; s < 12; s++)
            for (int sf = 0; sf < 4; sf++) {
                p25p2::ischInfo(s, sf, d);
                int slot = -1, f = -1, e = 0;
                CHECK(p25p2::ischDecode(d, slot, f, &e) && slot == s && f == sf && e == 0, "isch %d/%d -> %d/%d", s, sf, slot, f);
                CHECK(p25p2::syncDistance(d) > 10, "info ISCH looks like the sync word (%d)", p25p2::syncDistance(d));
                d[rng() % 20] ^= (uint8_t)(1 + rng() % 3);   // up to two bit errors in one dibit
                CHECK(p25p2::ischDecode(d, slot, f) && slot == s && f == sf, "isch with an error %d/%d", s, sf);
            }
        p25p2::ischSync(d);
        int slot, f;
        CHECK(!p25p2::ischDecode(d, slot, f), "the sync word decodes as an info ISCH");
    }
    // DUID
    {
        uint8_t d[4];
        for (int duid : {p25p2::kDuid4V, p25p2::kDuid2V, p25p2::kDuidSacch, p25p2::kDuidFacch}) {
            p25p2::duidEncode(duid, d);
            CHECK(p25p2::duidDecode(d) == duid, "duid %d", duid);
            d[rng() % 4] ^= 1;   // one bit error
            CHECK(p25p2::duidDecode(d) == duid, "duid %d with one bit error", duid);
        }
    }
    // MAC PDU codewords
    for (int facch = 0; facch < 2; facch++) {
        const int nb = facch ? p25p2::kFacchMacBytes : p25p2::kSacchMacBytes, nd = facch ? p25p2::kFacchDibits : p25p2::kSacchDibits;
        const int t = facch ? 11 : 10;   // hexbit errors the code corrects
        int okClean = 0, okErr = 0, rejected = 0, badAccepted = 0;
        for (int it = 0; it < 200; it++) {
            std::vector<uint8_t> mac((size_t)nb);
            for (auto& b : mac) b = (uint8_t)rng();
            std::vector<uint8_t> cw((size_t)nd);
            p25p2::macEncode(facch, mac.data(), cw.data());
            std::vector<uint8_t> out((size_t)nb);
            int fixed = 0;
            if (p25p2::macDecode(facch, cw.data(), out.data(), &fixed) && out == mac && fixed == 0) okClean++;
            // t hexbit errors (each hexbit is three dibits: damage one of them)
            std::vector<uint8_t> bad = cw;
            std::vector<int> pos;
            while ((int)pos.size() < t) { const int p = (int)(rng() % (unsigned)(nd / 3)); bool dup = false; for (int q : pos) dup |= q == p; if (!dup) pos.push_back(p); }
            for (int p : pos) bad[(size_t)(p * 3 + (int)(rng() % 3))] ^= (uint8_t)(1 + rng() % 3);
            if (p25p2::macDecode(facch, bad.data(), out.data(), &fixed) && out == mac && fixed == t) okErr++;
            // far too many errors: must not come out as a valid PDU
            std::vector<uint8_t> junk = cw;
            for (int p = 0; p < nd / 3 - 4; p++) junk[(size_t)(p * 3)] ^= (uint8_t)(1 + rng() % 3);
            if (p25p2::macDecode(facch, junk.data(), out.data())) { if (out != mac) badAccepted++; } else rejected++;
        }
        CHECK(okClean == 200, "%s clean %d/200", facch ? "FACCH" : "SACCH", okClean);
        CHECK(okErr == 200, "%s with %d hexbit errors %d/200", facch ? "FACCH" : "SACCH", t, okErr);
        CHECK(badAccepted == 0, "%s: %d damaged PDUs accepted with wrong content", facch ? "FACCH" : "SACCH", badAccepted);
        CHECK(rejected > 190, "%s: only %d of 200 hopeless PDUs rejected", facch ? "FACCH" : "SACCH", rejected);
    }
    // builders and field offsets
    {
        auto p = p25p2::macPtt(20, 0xABCDEF, 0x1234, 0x84);
        CHECK(p.size() == 20 && p25p2::macHeader(p[0]).opcode == p25p2::kMacPtt, "ptt header");
        CHECK(p25p2::be24(&p[p25p2::kPttSrcOff]) == 0xABCDEFu && p25p2::be16(&p[p25p2::kPttGrpOff]) == 0x1234u && p[p25p2::kPttAlgOff] == 0x84, "ptt fields");
        bool miZero = true;
        for (int i = p25p2::kPttMiOff; i < p25p2::kPttAlgOff; i++) miZero &= p[(size_t)i] == 0;
        CHECK(miZero, "the builders never write a message indicator");
        auto g = p25p2::msgGrpVGrant(0xC0, 0x1005, 777, 424242);
        CHECK(g.size() == 9 && (int)g.size() == p25p2::macMsgLen(g[0]), "grant length");
        const uint64_t a = p25p2::be64(&g[1]);
        CHECK(((a >> 24) & 0xFFFF) == 777 && (a & 0xFFFFFF) == 424242 && ((a >> 40) & 0xFFFF) == 0x1005 && (a >> 56) == 0xC0, "grant fields");
        for (int op : {0x01, 0x02, 0x40, 0x42, 0x44, 0x74, 0x7A, 0x7B, 0x7C, 0x7D}) CHECK(p25p2::macMsgLen(op) > 0 && std::strcmp(p25p2::macMsgName(op), "unknown") != 0, "message 0x%02X", op);
        CHECK(p25p2::macMsgLen(0x55) == 0, "unknown message has a length");
    }
    return testSummary("test_p25p2_proto");
}
