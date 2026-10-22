// The one call the setup and the lessons make to set the gain automatically. The advanced auto-gain (branch autogain-030) provides
// autogainRun(); until it lands, autogain_stub.cpp switches on the existing AGC loop (SmartAgc). Replace the stub, keep the signature.
#pragma once
#include <string>

struct App;

// Find and set a good gain for this radio (index in App::devices) and mode (the mode id, as learn.cpp names it: "fm", "adsb", ...).
// Returns true when automatic gain is now running or a gain was set; false when the radio has no gain to set (a file, the demo signal).
bool autogainRun(App& a, int radio, const std::string& mode);
