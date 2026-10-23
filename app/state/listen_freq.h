// The frequency being listened to (the VFO), as everything that names "the" frequency should show it: the big readout, the status strip,
// the receiver tabs. The General receiver's VFO may sit off its dial (a.freqMhz); every other mode moves a.freqMhz itself (tuneDecode).
#pragma once
#include "../app.h"
#include "freq_model.h"

inline double listenMhz(const App& a) { return a.family == 55 ? generalListenHz(a) / 1e6 : a.freqMhz; }
