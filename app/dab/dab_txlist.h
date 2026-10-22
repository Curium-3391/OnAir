// DAB transmitter lists (the Map tab, dab_tii_ui.cpp): one transmitter per line, found by (ensemble id, TII MainId, SubId).
//   eid,main,sub,lat,lon,site,power_kw,channel_mhz[,anything else]      eid in hex, a line starting with # is a comment, a TSV works too
// The format is described in docs/dab-transmitters.md. Pure functions, no UI (tests/app/test_dab_txlist.cpp).
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace dabtx {

struct Row { int eid = 0, mainId = 0, subId = 0; double lat = 0, lon = 0, powerKw = 0, channelMhz = 0; std::string site; };

inline uint64_t key(int eid, int mainId, int subId) { return ((uint64_t)(eid & 0xFFFF) << 16) | ((uint64_t)(mainId & 0xFF) << 8) | (uint64_t)(subId & 0xFF); }

// commas (or tabs, when the line has one); a field in double quotes may hold the separator; spaces around a field are dropped
inline std::vector<std::string> splitCsv(const std::string& line) {
    std::vector<std::string> f;
    const char sep = line.find('\t') != std::string::npos ? '\t' : ',';
    std::string cur;
    bool q = false;
    for (char c : line) {
        if (c == '"') q = !q;
        else if (c == sep && !q) { f.push_back(cur); cur.clear(); }
        else cur += c;
    }
    f.push_back(cur);
    for (auto& s : f) { while (!s.empty() && (s.back() == ' ' || s.back() == '\r')) s.pop_back(); while (!s.empty() && s.front() == ' ') s.erase(0, 1); }
    return f;
}

enum class Line { Skip, Bad, NoPosition, Ok };   // Skip: empty, a comment or the header

// One line of a list. NoPosition: the ids are fine but latitude or longitude is missing (a row of an export or a submission still to fill in).
inline Line parse(const std::string& line, Row& r) {
    if (line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) return parse(line.substr(3), r);   // the byte order mark a spreadsheet writes
    size_t b = 0;
    while (b < line.size() && (line[b] == ' ' || line[b] == '\t' || line[b] == '\r')) b++;
    if (b == line.size() || line[b] == '#') return Line::Skip;
    const auto f = splitCsv(line);
    if (!f.empty() && (f[0] == "eid" || f[0] == "EId")) return Line::Skip;
    if (f.size() < 5) return Line::Bad;
    char* e = nullptr;
    const long eid = std::strtol(f[0].c_str(), &e, 16);
    if (f[0].empty() || *e || eid < 0 || eid > 0xFFFF) return Line::Bad;
    char* em = nullptr; char* es = nullptr;
    const long m = std::strtol(f[1].c_str(), &em, 10), sb = std::strtol(f[2].c_str(), &es, 10);
    if (f[1].empty() || f[2].empty() || *em || *es || m < 0 || m > 69 || sb < 0 || sb > 23) return Line::Bad;
    r = Row();
    r.eid = (int)eid; r.mainId = (int)m; r.subId = (int)sb;
    if (f.size() > 5) r.site = f[5];
    if (f.size() > 6) r.powerKw = std::atof(f[6].c_str());
    if (f.size() > 7) r.channelMhz = std::atof(f[7].c_str());
    if (f[3].empty() || f[4].empty()) return Line::NoPosition;
    char* ea = nullptr; char* eo = nullptr;
    r.lat = std::strtod(f[3].c_str(), &ea); r.lon = std::strtod(f[4].c_str(), &eo);
    if (*ea || *eo || !std::isfinite(r.lat) || !std::isfinite(r.lon) || std::fabs(r.lat) > 90 || std::fabs(r.lon) > 180) return Line::Bad;
    return Line::Ok;
}

// a row as a line of the list (the site in quotes when it holds a comma; quotes inside it are dropped)
inline std::string format(const Row& r) {
    std::string site;
    for (char c : r.site) if (c != '"' && c != '\n' && c != '\r') site += c;
    if (site.find(',') != std::string::npos) site = "\"" + site + "\"";
    char b[160];
    snprintf(b, sizeof b, "%04X,%d,%d,%.5f,%.5f,", r.eid & 0xFFFF, r.mainId, r.subId, r.lat, r.lon);
    std::string s = b + site + ",";
    if (r.powerKw > 0) { snprintf(b, sizeof b, "%g", r.powerKw); s += b; }
    s += ",";
    if (r.channelMhz > 0) { snprintf(b, sizeof b, "%.3f", r.channelMhz); s += b; }
    return s;
}

struct MergeStats { int added = 0, replaced = 0, noPosition = 0, bad = 0; };

// The own list after importing another (a friend's export filled in, a regional list): every transmitter of the import with a position goes in;
// one the own list already has is replaced where it stands, the others are added at the end. The own file's other lines, comments and
// transmitters stay as they were. Rows without a position and lines not understood are counted, never written.
inline std::string merge(const std::string& own, const std::string& import, MergeStats& st) {
    st = MergeStats();
    std::vector<Row> rows;
    std::map<uint64_t, size_t> at;   // key -> index in rows (the last one wins inside the import)
    {
        std::istringstream in(import);
        std::string line;
        while (std::getline(in, line)) {
            Row r;
            const Line t = parse(line, r);
            if (t == Line::Bad) st.bad++;
            else if (t == Line::NoPosition) st.noPosition++;
            else if (t == Line::Ok) {
                const uint64_t k = key(r.eid, r.mainId, r.subId);
                auto it = at.find(k);
                if (it != at.end()) rows[it->second] = r;
                else { at[k] = rows.size(); rows.push_back(r); }
            }
        }
    }
    std::vector<bool> used(rows.size(), false);
    std::string out;
    bool header = false;
    {
        std::istringstream in(own);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            Row r;
            const Line t = parse(line, r);
            if (t == Line::Skip && line.find("eid") != std::string::npos && line.find('#') == std::string::npos) header = true;
            if (t == Line::Ok || t == Line::NoPosition) {
                auto it = at.find(key(r.eid, r.mainId, r.subId));
                if (it != at.end() && !used[it->second]) { out += format(rows[it->second]) + "\n"; used[it->second] = true; st.replaced++; continue; }
                if (it != at.end()) continue;   // a second line for the same transmitter in the own file: the imported one stands already
            }
            out += line + "\n";
        }
    }
    if (!header) out = "eid,main,sub,lat,lon,site,power_kw,channel_mhz\n" + out;
    for (size_t i = 0; i < rows.size(); i++) if (!used[i]) { out += format(rows[i]) + "\n"; st.added++; }
    return out;
}

} // namespace dabtx
