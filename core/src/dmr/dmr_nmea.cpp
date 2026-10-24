// NMEA 0183 RMC / GGA position sentences carried as plain text in DMR short-data messages.
// Format references: the NMEA 0183 standard (copyrighted) and the public field tables of Trimble
// (RMC/GGA) and Microchip GNSSDO; the DMR transport is short data, ETSI TS 102 361-3 clause 6.
#include "dect2/dmr/dmr_nmea.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace dect2 {
namespace {

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> v;
    size_t a = 0;
    for (;;) {
        const size_t b = s.find(sep, a);
        v.push_back(s.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return v;
}

double num(const std::string& s) { return s.empty() ? 0.0 : std::strtod(s.c_str(), nullptr); }

// ddmm.mmmm (latitude) or dddmm.mmmm (longitude) -> signed degrees
double coord(const std::string& v, char hemi) {
    const double raw = num(v);
    if (raw <= 0.0) return 0.0;
    const int deg = (int)(raw / 100.0);
    const double d = deg + (raw - deg * 100.0) / 60.0;
    return (hemi == 'S' || hemi == 'W') ? -d : d;
}

bool checksumOk(const std::string& body, const std::string& cs) {
    if (cs.size() < 2) return true;                 // no checksum given
    unsigned calc = 0;
    for (unsigned char c : body) calc ^= c;
    unsigned given = 0;
    return std::sscanf(cs.c_str(), "%2x", &given) == 1 && calc == given;
}

} // namespace

bool parseDmrNmea(const char* text, size_t size, DmrLocation& out) {
    if (!text || size < 6) return false;
    const std::string s(text, size);
    for (size_t start = s.find('$'); start != std::string::npos; start = s.find('$', start + 1)) {
        if (start + 6 > s.size()) break;
        const char* p = s.c_str() + start;                  // "$GNRMC" / "$GPGGA" / "$GLRMC" ...
        const bool rmc = p[3] == 'R' && p[4] == 'M' && p[5] == 'C';
        const bool gga = p[3] == 'G' && p[4] == 'G' && p[5] == 'A';
        if (!rmc && !gga) continue;
        size_t end = s.find_first_of("*\r\n", start);
        if (end == std::string::npos) end = s.size();
        const std::string cs = end < s.size() && s[end] == '*' ? s.substr(end + 1, 2) : std::string();
        if (!checksumOk(s.substr(start + 1, end - start - 1), cs)) continue;
        const std::vector<std::string> f = split(s.substr(start, end - start), ',');
        if (rmc) {
            if (f.size() < 9 || f[2] != "A") continue;      // A = valid, V = void
            const double lat = coord(f[3], f[4].empty() ? 'N' : f[4][0]);
            const double lon = coord(f[5], f[6].empty() ? 'E' : f[6][0]);
            if (lat == 0.0 && lon == 0.0) continue;
            out = DmrLocation();
            out.hasFix = true; out.lat = lat; out.lon = lon;
            if (!f[7].empty()) { out.hasSpeed = true; out.speedMs = num(f[7]) * 0.514444; }   // knots -> m/s
            if (!f[8].empty()) { out.hasHeading = true; out.headingDeg = num(f[8]); }
            return true;
        }
        if (f.size() < 7 || num(f[6]) == 0.0) continue;     // GGA quality 0 = no fix
        const double lat = coord(f[2], f[3].empty() ? 'N' : f[3][0]);
        const double lon = coord(f[4], f[5].empty() ? 'E' : f[5][0]);
        if (lat == 0.0 && lon == 0.0) continue;
        out = DmrLocation();
        out.hasFix = true; out.lat = lat; out.lon = lon;
        return true;
    }
    return false;
}

} // namespace dect2
