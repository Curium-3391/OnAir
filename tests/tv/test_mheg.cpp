// MHEG-5 BER structure parser: a synthetic Scene with nested constructed items, high tag numbers, long lengths, an indefinite-length item, and damaged input.
#include "dect2/tv/dsmcc.h"
#include "dect2/tv/mheg.h"
using namespace dect2;
#include "../common/test_util.h"

static MhegNode prim(int cls, uint32_t tag, std::vector<uint8_t> v) { MhegNode n; n.cls = cls; n.tag = tag; n.value = std::move(v); return n; }
static MhegNode cons(int cls, uint32_t tag, std::vector<MhegNode> k) { MhegNode n; n.cls = cls; n.tag = tag; n.constructed = true; n.kids = std::move(k); return n; }

int main() {
    std::string txt = "Hello MHEG";
    MhegNode scene = cons(1, 1, {
        prim(2, 0, {1, 0x2C}),                                       // [0] 300
        cons(2, 3, {prim(0, 4, std::vector<uint8_t>(txt.begin(), txt.end())), prim(2, 40, {0xFF})}),   // a tag number above 30
        cons(2, 7, {prim(2, 5, std::vector<uint8_t>(300, 0x55))}),   // long-form length
    });
    auto enc = mhegEncode(scene);
    CHECK(mhegLooksLikeApplication(enc.data(), enc.size()), "scene recognised");
    auto r = mhegParse(enc.data(), enc.size());
    CHECK(r.error.empty() && r.roots.size() == 1 && r.consumed == enc.size(), "parse ok: %s", r.error.c_str());
    if (!r.roots.empty()) {
        const auto& s = r.roots[0];
        CHECK(s.name == "Scene" && s.kids.size() == 3, "scene with 3 children");
        CHECK(s.kids.size() == 3 && s.kids[1].kids.size() == 2 && s.kids[1].kids[1].tag == 40, "nested high tag");
        CHECK(s.kids.size() == 3 && s.kids[2].kids[0].value.size() == 300, "long length");
        CHECK(mhegEncode(s) == enc, "re-encode identical");
        std::string d = mhegDump(s);
        CHECK(d.find("Hello MHEG") != std::string::npos && d.find("[0] 300") != std::string::npos, "dump text:\n%s", d.c_str());
    }
    // an application of indefinite length
    std::vector<uint8_t> ind = {0x60, 0x80, 0x82, 0x01, 0x05, 0x00, 0x00};
    auto ri = mhegParse(ind.data(), ind.size());
    CHECK(ri.error.empty() && ri.roots.size() == 1 && ri.roots[0].name == "Application" && ri.roots[0].kids.size() == 1, "indefinite: %s", ri.error.c_str());
    // damaged: truncated, deep nesting, junk
    auto cut = enc; cut.resize(cut.size() - 5);
    CHECK(!mhegParse(cut.data(), cut.size()).error.empty(), "truncated reported");
    std::vector<uint8_t> deep;
    for (int i = 0; i < 100; i++) { deep.push_back(0x30); deep.push_back(0x80); }
    CHECK(!mhegParse(deep.data(), deep.size()).error.empty(), "deep nesting refused");
    uint8_t junk[] = {0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
    CHECK(!mhegParse(junk, sizeof junk).error.empty() && !mhegLooksLikeApplication(junk, sizeof junk), "junk refused");
    // through a carousel: the file comes out of an object carousel (compressed, several blocks) and is parsed
    std::vector<DsmccTestFile> files = {{"a", enc}, {"junk", std::vector<uint8_t>(junk, junk + sizeof junk)}};
    auto secs = buildObjectCarousel(files, 1, 200, true);
    DsmccCarousel c;
    for (int round = 0; round < 2; round++) for (auto& s : secs) c.section(s.data(), (int)s.size());
    int found = 0;
    for (auto& f : c.files()) if (mhegLooksLikeApplication(f.data.data(), f.data.size())) { found++; CHECK(f.path == "a", "path %s", f.path.c_str()); }
    CHECK(found == 1, "one application file in the carousel, found %d", found);
    return testSummary("mheg");
}
