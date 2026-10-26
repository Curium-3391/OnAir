#include "dect2/general/swbc.h"

namespace dect2 {

// ITU RR Art. 5 broadcasting allocations in the HF range (footnotes 5.113 tropical bands, 5.127 75 m, 5.134 / 5.136 / 5.137 and the WRC-03 changes).
static const SwBand kBands[] = {
    {"120 m", 2300, 2495, true, "tropical band (RR 5.113); only in the tropical zone, 2300-2498 kHz in places"},
    {"90 m", 3200, 3400, true, "tropical band (RR 5.113); only in the tropical zone"},
    {"75 m", 3900, 4000, false, "Region 1: 3950-4000 kHz (Europe, Africa); 3900-3950 kHz Region 3 and part of Africa; not broadcasting in Region 2 (amateur)"},
    {"60 m", 4750, 5060, true, "tropical band (RR 5.113); only in the tropical zone"},
    {"49 m", 5900, 6200, false, "shared with fixed and mobile services"},
    {"41 m", 7200, 7450, false, "Region 1 and 3: 7200-7450 kHz; Region 2 broadcasts only above 7300 kHz (7200-7300 amateur)"},
    {"31 m", 9400, 9900, false, "the busiest band"},
    {"25 m", 11600, 12100, false, ""},
    {"22 m", 13570, 13870, false, ""},
    {"19 m", 15100, 15800, false, ""},
    {"16 m", 17480, 17900, false, ""},
    {"15 m", 18900, 19020, false, "narrow band, digital (DRM) trials"},
    {"13 m", 21450, 21850, false, ""},
    {"11 m", 25670, 26100, false, "little used; best around the sunspot maximum"},
};

const SwBand* swBroadcastBands(size_t& count) { count = sizeof kBands / sizeof *kBands; return kBands; }
double swBandCentreKhz(const SwBand& b) { return 0.5 * (b.startKhz + b.endKhz); }
const SwBand* swBandAt(double khz) {
    for (const SwBand& b : kBands) if (khz >= b.startKhz && khz <= b.endKhz) return &b;
    return nullptr;
}

}  // namespace dect2
