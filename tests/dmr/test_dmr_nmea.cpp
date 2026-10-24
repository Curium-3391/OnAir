// NMEA 0183 over DMR short data: the parser against the sentence seen in the 2026-10-24 log
// (144.625 MHz, TG 100, Src 22001) and the public Trimble RMC / GGA examples.
#include "dect2/dmr/dmr_nmea.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
using namespace dect2;
static int fails = 0;
static void fail(const char* fmt, ...) {   // one printf with a constant format, as Codacy wants
    char b[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    std::printf("FAIL: %s\n", b);
    fails++;
}
#define CHECK(c, ...) do { if (!(c)) fail(__VA_ARGS__); } while (0)

static bool parse(const char* s, DmrLocation& loc) { return parseDmrNmea(s, std::strlen(s), loc); }
static bool near(double a, double b, double eps) { return std::fabs(a - b) < eps; }

int main() {
    // the real message from the log: a stationary radio, UTC 09:39:19, valid fix (*46)
    {
        const char* s = "DMR TEXT: \"$GNRMC,093919.000,A,5315.93271,N,08332.15710,E,0.00,93.55,241026,,,A*46\"";
        DmrLocation loc;
        CHECK(parse(s, loc), "log sample: not parsed");
        CHECK(loc.hasFix, "log sample: no fix");
        CHECK(near(loc.lat, 53.2655452, 1e-5), "log sample: lat %.7f", loc.lat);
        CHECK(near(loc.lon, 83.5359517, 1e-5), "log sample: lon %.7f", loc.lon);
        CHECK(loc.hasSpeed && near(loc.speedMs, 0.0, 1e-3), "log sample: speed %.4f m/s", loc.speedMs);
        CHECK(loc.hasHeading && near(loc.headingDeg, 93.55, 1e-2), "log sample: heading %.2f", loc.headingDeg);
        // cross-check: the same radio's LRRP "Locn;" 2.5 min earlier was 53.26550 83.53600 0.0 kph 93 deg
        CHECK(near(loc.lat, 53.26550, 1e-4) && near(loc.lon, 83.53600, 1e-4), "log sample: differs from the DSD+ LRRP reference");
    }
    // a moving radio (the Trimble RMC example), no checksum in the string
    {
        const char* s = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W";
        DmrLocation loc;
        CHECK(parse(s, loc), "moving RMC: not parsed");
        CHECK(near(loc.lat, 48.1173, 1e-4), "moving RMC: lat %.6f", loc.lat);
        CHECK(near(loc.lon, 11.5166667, 1e-4), "moving RMC: lon %.6f", loc.lon);
        CHECK(loc.hasSpeed && near(loc.speedMs, 11.52355, 1e-3), "moving RMC: speed %.4f", loc.speedMs);
        CHECK(loc.hasHeading && near(loc.headingDeg, 84.4, 1e-2), "moving RMC: heading %.2f", loc.headingDeg);
    }
    // a GGA fix: no speed or course there
    {
        const char* s = "$GPGGA,172814.0,3723.46587704,N,12202.26957864,W,2,6,1.2,18.893,M,-25.669,M,2.0,0031";
        DmrLocation loc;
        CHECK(parse(s, loc), "GGA: not parsed");
        CHECK(near(loc.lat, 37.3910979, 1e-5), "GGA: lat %.7f", loc.lat);
        CHECK(near(loc.lon, -122.0378263, 1e-5), "GGA: lon %.7f", loc.lon);
        CHECK(!loc.hasSpeed && !loc.hasHeading, "GGA: extra speed/heading");
    }
    // RMC status V (void): no position
    {
        const char* s = "$GPRMC,123519,V,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W";
        DmrLocation loc;
        CHECK(!parse(s, loc), "void RMC parsed");
    }
    // GGA quality 0: no position
    {
        const char* s = "$GPGGA,172814.0,3723.46587704,N,12202.26957864,W,0,6,1.2,18.893,M,-25.669,M,2.0,0031";
        DmrLocation loc;
        CHECK(!parse(s, loc), "GGA quality 0 parsed");
    }
    // a checksum that does not match: the sentence is skipped
    {
        const char* s = "$GNRMC,093919.000,A,5315.93271,N,08332.15710,E,0.00,93.55,241026,,,A*00";
        DmrLocation loc;
        CHECK(!parse(s, loc), "bad checksum parsed");
    }
    // not a position sentence
    {
        DmrLocation loc;
        CHECK(!parse("no position here", loc), "plain text parsed");
    }
    // another $ sentence first, then a valid RMC: the loop must find it
    {
        const char* s = "$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39 $GPRMC,123519,A,4807.038,N,01131.000,E,0,0,230394,,";
        DmrLocation loc;
        CHECK(parse(s, loc), "second sentence: not parsed");
        CHECK(near(loc.lat, 48.1173, 1e-4) && near(loc.lon, 11.5166667, 1e-4), "second sentence: pos %.5f,%.5f", loc.lat, loc.lon);
    }
    // a zero position is not a fix
    {
        const char* s = "$GPRMC,123519,A,0000.000,N,00000.000,E,0,0,230394,,";
        DmrLocation loc;
        CHECK(!parse(s, loc), "zero position parsed");
    }
    if (fails) { printf("%d nmea checks failed\n", fails); return 1; }
    printf("nmea ok\n");
    return 0;
}
