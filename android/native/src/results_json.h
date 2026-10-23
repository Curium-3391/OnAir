#pragma once
#include <string>
#include "dect2/t2/t2rx.h"
namespace onair {
// The telemetry the phone shows: cards (label, value), tables (columns + rows of text) and the one-line summary of the mode.
// The same names as the desktop's tables. One JSON object; `std` is RxTelemetry::standard of the active mode.
std::string resultsJson(const dect2::RxTelemetry& rx, bool running, const dect2::DabEnsemble* dabEnsemble = nullptr);
}
