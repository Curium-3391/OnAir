#include "dect2/tv/mheg.h"
#include <cstdio>

namespace dect2 {
namespace {
constexpr int kMaxDepth = 40;
constexpr size_t kMaxNodes = 400000;

std::string tagName(int cls, uint32_t tag) {
    char b[32];
    if (cls == 1 && tag == 0) return "Application";
    if (cls == 1 && tag == 1) return "Scene";
    if (cls == 0) {
        switch (tag) {
        case 1: return "BOOLEAN";
        case 2: return "INTEGER";
        case 4: return "OCTET STRING";
        case 5: return "NULL";
        case 10: return "ENUMERATED";
        case 16: return "SEQUENCE";
        case 17: return "SET";
        }
        snprintf(b, sizeof b, "UNIVERSAL %u", tag);
    } else if (cls == 1) snprintf(b, sizeof b, "APPLICATION %u", tag);
    else if (cls == 2) snprintf(b, sizeof b, "[%u]", tag);
    else snprintf(b, sizeof b, "PRIVATE %u", tag);
    return b;
}

// parses one TLV at p[pos..end); returns false on malformed data
bool parseOne(const uint8_t* p, size_t& pos, size_t end, MhegNode& out, int depth, size_t& nodes, std::string& err) {
    if (depth > kMaxDepth) { err = "nesting too deep"; return false; }
    if (++nodes > kMaxNodes) { err = "too many objects"; return false; }
    if (pos >= end) { err = "truncated tag"; return false; }
    uint8_t b0 = p[pos++];
    out.cls = b0 >> 6;
    out.constructed = (b0 & 0x20) != 0;
    uint32_t tag = b0 & 0x1F;
    if (tag == 0x1F) {
        tag = 0;
        for (int i = 0;; i++) {
            if (pos >= end || i > 4) { err = "bad tag number"; return false; }
            uint8_t c = p[pos++];
            tag = (tag << 7) | (c & 0x7F);
            if (!(c & 0x80)) break;
        }
    }
    out.tag = tag;
    out.name = tagName(out.cls, tag);
    if (pos >= end) { err = "truncated length"; return false; }
    uint8_t l0 = p[pos++];
    size_t len = 0;
    if (l0 == 0x80) {
        if (!out.constructed) { err = "indefinite length on a primitive"; return false; }
        out.indefinite = true;
        for (;;) {
            if (pos + 2 <= end && p[pos] == 0 && p[pos + 1] == 0) { pos += 2; return true; }
            MhegNode k;
            if (!parseOne(p, pos, end, k, depth + 1, nodes, err)) return false;
            out.kids.push_back(std::move(k));
        }
    }
    if (l0 & 0x80) {
        int nb = l0 & 0x7F;
        if (nb > 4 || pos + (size_t)nb > end) { err = "bad length"; return false; }
        for (int i = 0; i < nb; i++) len = (len << 8) | p[pos++];
    } else len = l0;
    if (len > end - pos) { err = "length beyond the data"; return false; }
    size_t stop = pos + len;
    if (out.constructed) {
        while (pos < stop) {
            MhegNode k;
            if (!parseOne(p, pos, stop, k, depth + 1, nodes, err)) return false;
            out.kids.push_back(std::move(k));
        }
    } else {
        out.value.assign(p + pos, p + stop);
        pos = stop;
    }
    return true;
}

void dumpRec(const MhegNode& n, int depth, int maxDepth, std::string& s) {
    s.append((size_t)depth * 2, ' ');
    s += n.name;
    if (n.constructed) {
        char b[48];
        snprintf(b, sizeof b, " (%zu)", n.kids.size());
        s += b;
    } else {
        bool text = n.value.size() > 1;
        for (uint8_t c : n.value) if (c < 0x20 || c > 0x7E) text = false;
        if (text) s += " \"" + std::string(n.value.begin(), n.value.end()) + "\"";
        else if (n.value.size() <= 8 && !n.value.empty()) {
            int64_t v = (n.value[0] & 0x80) ? -1 : 0;
            for (uint8_t c : n.value) v = (int64_t)(((uint64_t)v << 8) | c);
            char b[48];
            snprintf(b, sizeof b, " %lld", (long long)v);
            s += b;
        } else if (!n.value.empty()) {
            char b[32];
            snprintf(b, sizeof b, " %zu bytes", n.value.size());
            s += b;
        }
    }
    s += '\n';
    if (depth + 1 >= maxDepth) { if (!n.kids.empty()) { s.append((size_t)(depth + 1) * 2, ' '); s += "...\n"; } return; }
    for (const auto& k : n.kids) dumpRec(k, depth + 1, maxDepth, s);
}

void putLen(std::vector<uint8_t>& o, size_t len) {
    if (len < 0x80) { o.push_back((uint8_t)len); return; }
    uint8_t t[4]; int n = 0;
    while (len) { t[n++] = (uint8_t)len; len >>= 8; }
    o.push_back((uint8_t)(0x80 | n));
    while (n) o.push_back(t[--n]);
}
} // namespace

MhegParse mhegParse(const uint8_t* p, size_t n) {
    MhegParse r;
    size_t pos = 0;
    while (pos < n) {
        MhegNode nd;
        size_t before = pos;
        if (!parseOne(p, pos, n, nd, 0, r.nodes, r.error)) { pos = before; break; }
        r.roots.push_back(std::move(nd));
    }
    r.consumed = pos;
    return r;
}

bool mhegLooksLikeApplication(const uint8_t* p, size_t n) {
    if (n < 2 || (p[0] != 0x60 && p[0] != 0x61)) return false;
    MhegNode nd;
    size_t pos = 0, nodes = 0;
    std::string err;
    return parseOne(p, pos, n, nd, 0, nodes, err);
}

std::string mhegDump(const MhegNode& n, int maxDepth) {
    std::string s;
    dumpRec(n, 0, maxDepth, s);
    return s;
}

std::vector<uint8_t> mhegEncode(const MhegNode& n) {
    std::vector<uint8_t> body;
    if (n.constructed) for (const auto& k : n.kids) { auto e = mhegEncode(k); body.insert(body.end(), e.begin(), e.end()); }
    else body = n.value;
    std::vector<uint8_t> o;
    uint8_t b0 = (uint8_t)((n.cls << 6) | (n.constructed ? 0x20 : 0));
    if (n.tag < 31) o.push_back(b0 | (uint8_t)n.tag);
    else {
        o.push_back(b0 | 0x1F);
        uint8_t t[5]; int k = 0; uint32_t v = n.tag;
        while (v) { t[k++] = v & 0x7F; v >>= 7; }
        while (k) { uint8_t c = t[--k]; o.push_back(k ? (c | 0x80) : c); }
    }
    putLen(o, body.size());
    o.insert(o.end(), body.begin(), body.end());
    return o;
}

} // namespace dect2
