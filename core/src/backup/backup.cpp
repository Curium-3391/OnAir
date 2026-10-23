#include "dect2/backup/backup.h"
#include "dect2/common/u8file.h"
#include "dect2/mesh/mesh_crypto.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

namespace dect2 {
namespace backup {

const char* const kExt = ".onairbackup";
namespace fs = std::filesystem;

// ---------------------------------------------------------------- catalog
const std::vector<ItemDef>& catalog() {
    static const std::vector<ItemDef> c = {
        {"settings", "Settings and bookmarks", false, {}},
        {"memories", "Memories and frequency lists", false, {"memories.json", "frequencies.json", "watchlists/"}},
        {"presets", "Presets", false, {"presets.json", "*.onairpreset", "presets/", "meshcore-presets.json"}},
        {"scheduler", "Scheduler jobs, automations and history", false, {"automations/", "tv/schedule.json"}},
        {"logbook", "Ham logbook (ADIF)", false, {"onair_ham.adi", "onair_ham_upload.tsv"}},
        {"layouts", "Saved layouts and last session", false, {"layouts/", "last-session.txt"}},
        {"scans", "DAB, TV, FM and scanner results", false, {"results/app-dab-scan.bin", "results/app-tv-scan.bin", "results/app-fm-scan.bin", "results/app-scanner.bin"}},
        {"stations", "Custom station lists", false, {"stations.txt", "dab-transmitters.csv", "trunk_talkgroups.csv", "ship_names.tsv", "satellite_beacon.csv"}},
        {"codecs", "Plug-in codecs (names only, not the codec files)", false, {}},
        {"voicekeys", "Voice keys (needs a passphrase)", true, {"voicekeys/keys.oavk"}},
    };
    return c;
}

const ItemDef* findItem(const std::string& id) {
    for (const ItemDef& d : catalog()) if (id == d.id) return &d;
    return nullptr;
}

std::string itemOfPath(const std::string& rel) {
    for (const ItemDef& d : catalog()) {
        for (const std::string& p : d.paths) {
            if (p.empty()) continue;
            if (p[0] == '*') {
                const std::string ext = p.substr(1);
                if (rel.find('/') == std::string::npos && rel.size() > ext.size() && rel.compare(rel.size() - ext.size(), ext.size(), ext) == 0) return d.id;
            } else if (p.back() == '/') {
                if (rel.size() > p.size() && rel.compare(0, p.size(), p) == 0) return d.id;
            } else if (rel == p) return d.id;
        }
    }
    return std::string();
}

bool isSecretPrefKey(const std::string& key) {
    std::string k;
    for (char c : key) k += (char)std::tolower((unsigned char)c);
    static const char* const kPart[] = {"password", "passwd", "passphrase", "token", "secret", "apikey", "api_key", "qrzkey", "clubapi", "clubpw", "eqslpw", "authkey", "privkey", "private"};
    for (const char* p : kPart) if (k.find(p) != std::string::npos) return true;
    if (k.size() >= 2 && k.compare(k.size() - 2, 2, "pw") == 0) return true;
    if (k.size() >= 3 && k.compare(k.size() - 3, 3, "key") == 0 && (k.find("api") != std::string::npos || k.find("licen") != std::string::npos || k.find("auth") != std::string::npos)) return true;
    return false;
}

// ---------------------------------------------------------------- settings records
static std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '\\') o += "\\\\"; else if (c == '\n') o += "\\n"; else if (c == '\r') o += "\\r"; else if (c == '\t') o += "\\t"; else o += c;
    }
    return o;
}
static std::string unesc(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            const char n = s[++i];
            o += n == 'n' ? '\n' : n == 'r' ? '\r' : n == 't' ? '\t' : n;
        } else o += s[i];
    }
    return o;
}

std::string serializePrefs(const std::vector<PrefItem>& items) {
    std::string o = "OnAir settings 1\n";
    for (const PrefItem& p : items) { o += p.type; o += '\t'; o += esc(p.key); o += '\t'; o += esc(p.value); o += '\n'; }
    return o;
}

bool parsePrefs(const std::string& text, std::vector<PrefItem>& out) {
    out.clear();
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line != "OnAir settings 1") return false;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const size_t a = line.find('\t');
        if (a != 1) return false;
        const size_t b = line.find('\t', a + 1);
        if (b == std::string::npos) return false;
        const char t = line[0];
        if (!std::strchr("DIBSC", t)) return false;
        out.push_back({unesc(line.substr(a + 1, b - a - 1)), t, unesc(line.substr(b + 1))});
    }
    return true;
}

void splitSecretPrefs(const std::vector<PrefItem>& all, std::vector<PrefItem>& plain, std::vector<PrefItem>& secret) {
    plain.clear(); secret.clear();
    for (const PrefItem& p : all) (p.type != 'C' && isSecretPrefKey(p.key) ? secret : plain).push_back(p);
}

std::vector<PrefItem> selectPrefs(const std::vector<PrefItem>& in, Mode mode, const std::set<std::string>& have) {
    std::vector<PrefItem> o;
    for (const PrefItem& p : in) if (mode == Mode::Replace || p.type == 'C' || !have.count(p.key)) o.push_back(p);
    return o;
}

// ---------------------------------------------------------------- zip
static uint32_t crc32z(const std::string& d) {
    static uint32_t t[256]; static bool init = false;
    if (!init) { for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; t[i] = c; } init = true; }
    uint32_t c = 0xFFFFFFFFu;
    for (unsigned char ch : d) c = t[(c ^ ch) & 255] ^ (c >> 8);
    return ~c;
}
static void put16(std::string& o, uint32_t v) { o.push_back((char)(v & 255)); o.push_back((char)((v >> 8) & 255)); }
static void put32(std::string& o, uint32_t v) { put16(o, v & 0xFFFF); put16(o, v >> 16); }
static uint32_t g16(const std::string& s, size_t p) { return (uint8_t)s[p] | ((uint32_t)(uint8_t)s[p + 1] << 8); }
static uint32_t g32(const std::string& s, size_t p) { return g16(s, p) | (g16(s, p + 2) << 16); }

std::string zipPack(const std::vector<Entry>& files) {
    std::string out, dir;
    for (const Entry& f : files) {
        const uint32_t crc = crc32z(f.data), off = (uint32_t)out.size(), n = (uint32_t)f.data.size();
        put32(out, 0x04034b50); put16(out, 20); put16(out, 0x0800); put16(out, 0); put16(out, 0); put16(out, 0x21);
        put32(out, crc); put32(out, n); put32(out, n); put16(out, (uint32_t)f.name.size()); put16(out, 0);
        out += f.name; out += f.data;
        put32(dir, 0x02014b50); put16(dir, 20); put16(dir, 20); put16(dir, 0x0800); put16(dir, 0); put16(dir, 0); put16(dir, 0x21);
        put32(dir, crc); put32(dir, n); put32(dir, n); put16(dir, (uint32_t)f.name.size()); put16(dir, 0); put16(dir, 0); put16(dir, 0); put16(dir, 0);
        put32(dir, 0); put32(dir, off); dir += f.name;
    }
    const uint32_t dirOff = (uint32_t)out.size();
    out += dir;
    put32(out, 0x06054b50); put16(out, 0); put16(out, 0); put16(out, (uint32_t)files.size()); put16(out, (uint32_t)files.size());
    put32(out, (uint32_t)dir.size()); put32(out, dirOff); put16(out, 0);
    return out;
}

bool safeRelPath(const std::string& rel, size_t maxLen) {
    if (rel.empty() || rel.size() > maxLen) return false;
    if (rel[0] == '/' || rel[0] == '\\') return false;
    size_t start = 0;
    for (size_t i = 0; i <= rel.size(); i++) {
        if (i == rel.size() || rel[i] == '/') {
            const std::string part = rel.substr(start, i - start);
            if (part.empty() || part == "." || part == "..") return false;
            if (part.back() == '.' || part.back() == ' ') return false;
            start = i + 1;
        } else {
            const unsigned char c = (unsigned char)rel[i];
            if (c < 32 || c == 127 || c == '\\' || c == ':') return false;
        }
    }
    return true;
}

std::string zipUnpack(const std::string& z, std::vector<Entry>& out, const Limits& lim) {
    out.clear();
    if (z.size() < 22) return "not a backup file (too small)";
    // end of central directory, searched backwards (a comment of up to 64 KB is allowed)
    size_t eocd = std::string::npos;
    const size_t lo = z.size() > 22 + 65535 ? z.size() - 22 - 65535 : 0;
    for (size_t p = z.size() - 22 + 1; p-- > lo;) if (g32(z, p) == 0x06054b50) { eocd = p; break; }
    if (eocd == std::string::npos) return "not a backup file (no zip directory)";
    const uint32_t nEntries = g16(z, eocd + 10), dirSize = g32(z, eocd + 12), dirOff = g32(z, eocd + 16);
    if (g16(z, eocd + 4) != 0 || g16(z, eocd + 6) != 0 || g16(z, eocd + 8) != nEntries) return "multi-part archives are not supported";
    if (nEntries == 0xFFFF || dirSize == 0xFFFFFFFFu || dirOff == 0xFFFFFFFFu) return "zip64 archives are not supported";
    if (nEntries > lim.maxEntries) return "too many entries in the archive";
    if ((uint64_t)dirOff + dirSize > eocd) return "the zip directory is out of range";
    size_t p = dirOff;
    uint64_t total = 0;
    std::set<std::string> seen;
    std::vector<std::pair<uint64_t, uint64_t>> spans;   // [begin,end) of every local record, for the overlap check
    for (uint32_t i = 0; i < nEntries; i++) {
        if (p + 46 > (size_t)dirOff + dirSize || g32(z, p) != 0x02014b50) return "damaged zip directory";
        const uint32_t flags = g16(z, p + 8), method = g16(z, p + 10), crc = g32(z, p + 16), csz = g32(z, p + 20), usz = g32(z, p + 24);
        const uint32_t nl = g16(z, p + 28), xl = g16(z, p + 30), cl = g16(z, p + 32), lho = g32(z, p + 42);
        if (p + 46 + nl + xl + cl > (size_t)dirOff + dirSize) return "damaged zip directory";
        const std::string name(z, p + 46, nl);
        p += 46 + (size_t)nl + xl + cl;
        if (flags & 1) return "encrypted zip entries are not supported";
        if (method != 0) return "compressed entries are refused (an OnAir backup is stored, not compressed): " + name;
        if (csz != usz) return "entry sizes do not match: " + name;
        if (csz == 0xFFFFFFFFu || lho == 0xFFFFFFFFu) return "zip64 archives are not supported";
        if (!name.empty() && name.back() == '/') continue;   // a folder entry: nothing to extract
        if (!safeRelPath(name, lim.maxNameLen)) return "unsafe path in the archive (refused): " + name;
        if (!seen.insert(name).second) return "duplicate entry in the archive: " + name;
        if (usz > lim.maxEntryBytes) return "an entry is larger than the limit: " + name;
        total += usz;
        if (total > lim.maxTotalBytes) return "the archive is larger than the limit";
        if ((uint64_t)lho + 30 > dirOff) return "entry offset is out of range: " + name;
        if (g32(z, lho) != 0x04034b50) return "damaged entry: " + name;
        const uint32_t lnl = g16(z, lho + 26), lxl = g16(z, lho + 28), lmethod = g16(z, lho + 8);
        if (lmethod != 0) return "compressed entries are refused: " + name;
        if (lnl != nl || z.compare(lho + 30, lnl, name) != 0) return "entry names do not match: " + name;
        const uint64_t dataBeg = (uint64_t)lho + 30 + lnl + lxl, dataEnd = dataBeg + usz;
        if (dataEnd > dirOff) return "entry data is out of range: " + name;
        for (const auto& s : spans) if (lho < s.second && dataEnd > s.first) return "overlapping entries: " + name;
        spans.push_back({lho, dataEnd});
        Entry e; e.name = name; e.data.assign(z, (size_t)dataBeg, usz);
        if (crc32z(e.data) != crc) return "checksum error in: " + name;
        out.push_back(std::move(e));
    }
    return std::string();
}

// ---------------------------------------------------------------- manifest
std::string writeManifest(const Manifest& m) {
    std::string o = "OnAir backup\nformat=" + std::to_string(m.format) + "\napp=" + m.app + "\ncreated=" + m.created + "\nsecrets=" + (m.secrets ? "1" : "0") + "\n";
    for (const auto& i : m.items) o += "item\t" + i.id + "\t" + std::to_string(i.files) + "\t" + std::to_string(i.bytes) + "\n";
    for (const auto& f : m.files) o += "file\t" + f.name + "\t" + std::to_string(f.bytes) + "\t" + f.item + "\n";
    return o;
}

static std::vector<std::string> tabs(const std::string& s) {
    std::vector<std::string> v; std::string cur;
    for (char c : s) { if (c == '\t') { v.push_back(cur); cur.clear(); } else cur += c; }
    v.push_back(cur);
    return v;
}
static bool toU64(const std::string& s, uint64_t& v) {
    if (s.empty() || s.size() > 18) return false;
    v = 0;
    for (char c : s) { if (c < '0' || c > '9') return false; v = v * 10 + (uint64_t)(c - '0'); }
    return true;
}

bool parseManifest(const std::string& text, Manifest& m) {
    m = Manifest();
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line != "OnAir backup") return false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line.compare(0, 7, "format=") == 0) { uint64_t v; if (!toU64(line.substr(7), v)) return false; m.format = (int)v; }
        else if (line.compare(0, 4, "app=") == 0) m.app = line.substr(4);
        else if (line.compare(0, 8, "created=") == 0) m.created = line.substr(8);
        else if (line.compare(0, 8, "secrets=") == 0) m.secrets = line.substr(8) == "1";
        else if (line.compare(0, 5, "item\t") == 0) {
            auto t = tabs(line); uint64_t f, b;
            if (t.size() != 4 || !toU64(t[2], f) || !toU64(t[3], b)) return false;
            m.items.push_back({t[1], (int)f, b});
        } else if (line.compare(0, 5, "file\t") == 0) {
            auto t = tabs(line); uint64_t b;
            if (t.size() != 4 || !toU64(t[2], b)) return false;
            m.files.push_back({t[1], b, t[3]});
        }
    }
    return m.format > 0;
}

std::string checkManifestAgainst(const Manifest& m, const std::vector<Entry>& entries) {
    std::set<std::string> want = {"manifest.txt"};
    std::set<std::string> have;
    for (const Entry& e : entries) have.insert(e.name);
    if (!have.count("manifest.txt")) return "manifest.txt is missing";
    for (const auto& f : m.files) {
        if (!safeRelPath(f.name)) return "unsafe path in the manifest: " + f.name;
        const ItemDef* d = findItem(f.item);
        if (!d) return "unknown item in the manifest: " + f.item;
        if (d->secret) { if (!m.secrets) return "secret file listed but the archive has no secrets"; continue; }
        want.insert("data/" + f.name);
        auto it = std::find_if(entries.begin(), entries.end(), [&](const Entry& e) { return e.name == "data/" + f.name; });
        if (it == entries.end()) return "a file listed in the manifest is missing: " + f.name;
        if (it->data.size() != f.bytes) return "size differs from the manifest: " + f.name;
    }
    if (m.secrets) want.insert("secrets.enc");
    for (const char* fixed : {"settings.txt", "codecs-listing.txt"}) if (have.count(fixed)) want.insert(fixed);
    for (const std::string& n : have) if (!want.count(n)) return "an entry the manifest does not list: " + n;
    if (m.secrets && !have.count("secrets.enc")) return "secrets.enc is missing";
    return std::string();
}

int compareVersions(const std::string& a, const std::string& b) {
    auto parts = [](const std::string& s) {
        std::vector<long> v; long cur = 0; bool any = false;
        for (char c : s) {
            if (c >= '0' && c <= '9') { cur = cur * 10 + (c - '0'); any = true; if (cur > 1000000) cur = 1000000; }
            else if (any) { v.push_back(cur); cur = 0; any = false; if (c != '.') break; }
        }
        if (any) v.push_back(cur);
        return v;
    };
    auto x = parts(a), y = parts(b);
    for (size_t i = 0; i < std::max(x.size(), y.size()); i++) {
        const long p = i < x.size() ? x[i] : 0, q = i < y.size() ? y[i] : 0;
        if (p != q) return p < q ? -1 : 1;
    }
    return 0;
}

VersionState checkVersion(const Manifest& m, const std::string& thisApp, std::string& msg) {
    if (m.format > kFormat) { msg = "This backup was made by a newer OnAir (backup format " + std::to_string(m.format) + "); update OnAir to restore it."; return VersionState::Refuse; }
    if (m.format < 1) { msg = "Not an OnAir backup."; return VersionState::Refuse; }
    if (!m.app.empty() && compareVersions(m.app, thisApp) > 0) { msg = "This backup is from OnAir " + m.app + ", newer than this " + thisApp + ". Some settings may not be understood."; return VersionState::Warn; }
    msg.clear();
    return VersionState::Ok;
}

std::vector<PlanOp> planRestore(const Manifest& m, const std::set<std::string>& ids, Mode mode, const std::set<std::string>& existing) {
    std::vector<PlanOp> ops;
    for (const auto& f : m.files) {
        if (!ids.count(f.item)) continue;
        const bool ex = existing.count(f.name) != 0;
        ops.push_back({f.name, mode == Mode::Merge && ex ? PlanOp::SkipExisting : PlanOp::Write, ex});
    }
    return ops;
}

// ---------------------------------------------------------------- disk
static bool writeAtomicP(const fs::path& p, const std::string& data) {
    std::error_code ec;
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    fs::path tmp = p; tmp += ".onairtmp";
    {
        std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
        if (!o) return false;
        o.write(data.data(), (std::streamsize)data.size());
        o.flush();
        if (!o) { o.close(); fs::remove(tmp, ec); return false; }
    }
    fs::rename(tmp, p, ec);
    if (ec) { fs::remove(tmp, ec); return false; }
    return true;
}
bool writeFileAtomic(const std::string& path, const std::string& data) { return writeAtomicP(u8file(path), data); }

static bool insideRoot(const fs::path& root, const std::string& rel, fs::path& target) {
    if (!safeRelPath(rel)) return false;
    std::error_code ec;
    const fs::path r = fs::weakly_canonical(root, ec);
    if (ec) return false;
    // no existing parent (or the file itself) may be a symlink
    fs::path cur = r;
    size_t start = 0;
    for (size_t i = 0; i <= rel.size(); i++) {
        if (i == rel.size() || rel[i] == '/') {
            cur /= u8file(rel.substr(start, i - start));
            start = i + 1;
            if (fs::is_symlink(cur, ec)) return false;
        }
    }
    target = cur;
    const std::string a = r.generic_string(), b = cur.generic_string();
    return b.size() > a.size() && b.compare(0, a.size(), a) == 0 && (a.empty() || a.back() == '/' || b[a.size()] == '/');
}

ExtractReport extractTo(const std::string& root, const std::vector<Entry>& entries, const std::vector<PlanOp>& plan, bool dryRun) {
    ExtractReport rep;
    for (const PlanOp& op : plan) {
        if (op.kind == PlanOp::SkipExisting) { rep.skipped++; continue; }
        const Entry* e = nullptr;
        for (const Entry& x : entries) if (x.name == "data/" + op.name || x.name == op.name) { e = &x; break; }
        if (!e) { rep.errors.push_back("not in the archive: " + op.name); continue; }
        fs::path target;
        if (!insideRoot(u8file(root), op.name, target)) { rep.errors.push_back("refused (outside the data folder): " + op.name); continue; }
        if (dryRun) { rep.written++; continue; }
        if (writeAtomicP(target, e->data)) rep.written++;
        else rep.errors.push_back("could not write: " + op.name);
    }
    return rep;
}

// ---------------------------------------------------------------- secrets
static void putLe32(std::string& o, uint32_t v) { for (int i = 0; i < 4; i++) o.push_back((char)((v >> (8 * i)) & 255)); }
static void putLe64(std::string& o, uint64_t v) { for (int i = 0; i < 8; i++) o.push_back((char)((v >> (8 * i)) & 255)); }

std::string packEntries(const std::vector<Entry>& e) {
    std::string o;
    putLe32(o, (uint32_t)e.size());
    for (const Entry& x : e) { putLe32(o, (uint32_t)x.name.size()); o += x.name; putLe64(o, x.data.size()); o += x.data; }
    return o;
}

bool unpackEntries(const std::string& b, std::vector<Entry>& out, const Limits& lim) {
    out.clear();
    size_t p = 0;
    auto rd = [&](uint64_t& v, int n) { if (b.size() - p < (size_t)n) return false; v = 0; for (int i = 0; i < n; i++) v |= (uint64_t)(uint8_t)b[p + i] << (8 * i); p += n; return true; };
    uint64_t n;
    if (!rd(n, 4) || n > lim.maxEntries) return false;
    uint64_t total = 0;
    for (uint64_t i = 0; i < n; i++) {
        uint64_t nl, dl;
        if (!rd(nl, 4) || nl == 0 || nl > lim.maxNameLen || b.size() - p < nl) return false;
        Entry e; e.name.assign(b, p, (size_t)nl); p += (size_t)nl;
        if (!rd(dl, 8) || dl > lim.maxEntryBytes || b.size() - p < dl) return false;
        total += dl;
        if (total > lim.maxTotalBytes || !safeRelPath(e.name)) return false;
        e.data.assign(b, p, (size_t)dl); p += (size_t)dl;
        out.push_back(std::move(e));
    }
    return p == b.size();
}

bool passphraseOk(const std::string& pass, std::string& why) {
    if (pass.size() < 8) { why = "The passphrase needs at least 8 characters."; return false; }
    why.clear();
    return true;
}

void pbkdf2Sha256(const std::string& pw, const uint8_t* salt, size_t saltLen, uint32_t rounds, uint8_t* out, size_t outLen) {
    const uint8_t* key = (const uint8_t*)pw.data();
    size_t done = 0;
    for (uint32_t block = 1; done < outLen; block++) {
        std::vector<uint8_t> msg(salt, salt + saltLen);
        msg.push_back((uint8_t)(block >> 24)); msg.push_back((uint8_t)(block >> 16)); msg.push_back((uint8_t)(block >> 8)); msg.push_back((uint8_t)block);
        uint8_t u[32], t[32], nu[32];
        meshcrypto::hmacSha256(key, pw.size(), msg.data(), msg.size(), u);
        std::memcpy(t, u, 32);
        for (uint32_t i = 1; i < rounds; i++) {
            meshcrypto::hmacSha256(key, pw.size(), u, 32, nu);
            std::memcpy(u, nu, 32);
            for (int k = 0; k < 32; k++) t[k] ^= u[k];
        }
        const size_t n = std::min<size_t>(32, outLen - done);
        std::memcpy(out + done, t, n);
        done += n;
    }
}

static const char kMagic[6] = {'O', 'A', 'B', 'K', '1', '\n'};
constexpr size_t kHeader = 6 + 16 + 4 + 16 + 32;

std::string sealSecrets(const std::string& plain, const std::string& pass, std::string& blob, uint32_t rounds) {
    std::string why;
    if (!passphraseOk(pass, why)) return why;
    uint8_t salt[16], iv[16];
    std::random_device rd;
    for (auto& b : salt) b = (uint8_t)rd();
    for (auto& b : iv) b = (uint8_t)rd();
    uint8_t keys[64];
    pbkdf2Sha256(pass, salt, 16, rounds, keys, 64);   // 32 bytes encryption key, 32 bytes MAC key
    std::vector<uint8_t> ct(plain.begin(), plain.end());
    uint8_t ctr[16]; std::memcpy(ctr, iv, 16);
    if (!meshcrypto::aesCtr(keys, 32, ctr, ct.data(), ct.size())) return "encryption failed";
    std::vector<uint8_t> macIn(salt, salt + 16);
    macIn.insert(macIn.end(), iv, iv + 16);
    macIn.insert(macIn.end(), ct.begin(), ct.end());
    uint8_t mac[32];
    meshcrypto::hmacSha256(keys + 32, 32, macIn.data(), macIn.size(), mac);
    blob.assign(kMagic, 6);
    blob.append((const char*)salt, 16);
    putLe32(blob, rounds);
    blob.append((const char*)iv, 16);
    blob.append((const char*)mac, 32);
    blob.append((const char*)ct.data(), ct.size());
    std::memset(keys, 0, sizeof keys);
    return std::string();
}

std::string openSecrets(const std::string& blob, const std::string& pass, std::string& plain) {
    plain.clear();
    if (blob.size() < kHeader || std::memcmp(blob.data(), kMagic, 6) != 0) return "not an OnAir secrets block";
    const uint8_t* p = (const uint8_t*)blob.data() + 6;
    const uint8_t* salt = p; p += 16;
    const uint32_t rounds = p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); p += 4;
    const uint8_t* iv = p; p += 16;
    const uint8_t* mac = p; p += 32;
    const size_t ctLen = blob.size() - kHeader;
    if (rounds < 1000 || rounds > 5000000) return "the secrets block is damaged";
    if (pass.empty()) return "a passphrase is needed";
    uint8_t keys[64];
    pbkdf2Sha256(pass, salt, 16, rounds, keys, 64);
    std::vector<uint8_t> macIn(salt, salt + 16);
    macIn.insert(macIn.end(), iv, iv + 16);
    macIn.insert(macIn.end(), p, p + ctLen);
    uint8_t want[32];
    meshcrypto::hmacSha256(keys + 32, 32, macIn.data(), macIn.size(), want);
    uint8_t diff = 0;
    for (int i = 0; i < 32; i++) diff |= (uint8_t)(want[i] ^ mac[i]);
    if (diff) { std::memset(keys, 0, sizeof keys); return "wrong passphrase, or the file was changed"; }
    std::vector<uint8_t> pt(p, p + ctLen);
    uint8_t ctr[16]; std::memcpy(ctr, iv, 16);
    const bool ok = meshcrypto::aesCtr(keys, 32, ctr, pt.data(), pt.size());
    std::memset(keys, 0, sizeof keys);
    if (!ok) return "decryption failed";
    plain.assign((const char*)pt.data(), pt.size());
    return std::string();
}

// ---------------------------------------------------------------- automatic backup
std::string autoBackupName(int64_t t) {
    int64_t days = t / 86400, rem = t % 86400;
    if (rem < 0) { rem += 86400; days--; }
    // civil date from days since 1970-01-01 (Howard Hinnant)
    days += 719468;
    const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const int64_t doe = days - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t y = yoe + era * 400;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    const int d = (int)(doy - (153 * mp + 2) / 5 + 1);
    const int mo = (int)(mp < 10 ? mp + 3 : mp - 9);
    if (mo <= 2) y++;
    char b[64];
    snprintf(b, sizeof b, "onair-auto-%04d%02d%02d-%02d%02d%02d%s", (int)y, mo, d, (int)(rem / 3600), (int)(rem % 3600 / 60), (int)(rem % 60), kExt);
    return b;
}

bool isAutoBackupName(const std::string& n) {
    const std::string pre = "onair-auto-";
    const size_t len = pre.size() + 15 + std::strlen(kExt);
    if (n.size() != len || n.compare(0, pre.size(), pre) != 0 || n.compare(len - std::strlen(kExt), std::strlen(kExt), kExt) != 0) return false;
    for (size_t i = 0; i < 15; i++) {
        const char c = n[pre.size() + i];
        if (i == 8 ? c != '-' : !(c >= '0' && c <= '9')) return false;
    }
    return true;
}

bool autoBackupDue(int64_t last, int64_t now, int days) {
    if (days < 1) days = 1;
    if (last <= 0) return true;
    if (now < last) return false;   // clock went back: wait
    return now - last >= (int64_t)days * 86400;
}

std::vector<std::string> autoBackupsToDelete(std::vector<std::string> names, int keep) {
    std::vector<std::string> a;
    for (auto& n : names) if (isAutoBackupName(n)) a.push_back(n);
    std::sort(a.begin(), a.end());   // the date is in the name: oldest first
    if (keep < 1) keep = 1;
    std::vector<std::string> del;
    if ((int)a.size() > keep) del.assign(a.begin(), a.end() - keep);
    return del;
}

} // namespace backup
} // namespace dect2
