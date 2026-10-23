// Project Horus balloon telemetry: the mode numbers. OnAir has three numbering domains: the app family, the engine standard
// (Engine::setStandard, family + 2) and the active standard (RxTelemetry::standard, feed / poll / reset, engine standard - 1).
#pragma once
namespace dect2::horus {
inline constexpr int kFamily = 148;
inline constexpr int kStandard = kFamily + 2;          // 150
inline constexpr int kActiveStandard = kStandard - 1;  // 149, also the poll slot (Engine::kPollSeqSlots is 150)
// During a mode switch the last snapshot can still belong to the old receiver.
inline bool matchesMode(int family, int activeStandard) { return family == kFamily && activeStandard == kActiveStandard; }
} // namespace dect2::horus
