// Shortwave broadcast bands (ITU Radio Regulations Art. 5, as used in the HFCC seasonal schedules). All frequencies in kHz.
// The AM broadcast entry of the General receiver shows one button per band.
#pragma once
#include <cstddef>

namespace dect2 {

struct SwBand {
    const char* name;     // "49 m"
    double startKhz, endKhz;
    bool tropical;        // a tropical-zone band (120, 90, 60 m): used mainly between 30N and 30S
    const char* note;     // region differences
};

const SwBand* swBroadcastBands(size_t& count);   // sorted by frequency, no overlaps
double swBandCentreKhz(const SwBand& b);
const SwBand* swBandAt(double khz);              // the band holding khz, or nullptr
constexpr double kSwStepKhz = 5;                 // the HFCC channel raster (9 kHz on medium wave)
constexpr double kSwAmWidthHz = 9000;            // default AM filter width (the 10 kHz channel less a guard)

}  // namespace dect2
