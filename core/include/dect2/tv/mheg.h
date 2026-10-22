// MHEG-5 (ISO/IEC 13522-5) application files as carried in a DSM-CC object carousel (UK/Ireland/NZ profile ETSI ES 202 184): the files are ASN.1 BER.
// This parses the BER structure into a tree (class, tag, constructed/primitive, value) that a viewer can list. Only the two APPLICATION tags that
// are certain (0 Application, 1 Scene) get names; the other object and attribute tags are shown by number until they are checked against the spec text.
// Read-only and bounded (depth and node count); nothing is executed, there is no scripting and no rendering.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace dect2 {

struct MhegNode {
    int cls = 0;                  // 0 universal, 1 application, 2 context-specific, 3 private
    uint32_t tag = 0;
    bool constructed = false;
    bool indefinite = false;
    std::string name;             // "Application", "Scene", "INTEGER", "[3]" ...
    std::vector<uint8_t> value;   // primitive content
    std::vector<MhegNode> kids;   // constructed content
};

struct MhegParse {
    std::vector<MhegNode> roots;
    size_t consumed = 0;
    size_t nodes = 0;
    std::string error;            // empty when the whole input parsed
};

MhegParse mhegParse(const uint8_t* p, size_t n);
bool mhegLooksLikeApplication(const uint8_t* p, size_t n);   // starts with a constructed APPLICATION 0 or 1 whose length fits the data
std::string mhegDump(const MhegNode& n, int maxDepth = 12);  // indented object tree, one line per node
std::vector<uint8_t> mhegEncode(const MhegNode& n);          // definite-length BER, for test signals

} // namespace dect2
