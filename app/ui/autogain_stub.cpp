// Stand-in for the advanced auto-gain (autogain-030): switches on the existing AGC loop. Delete this file when autogainRun() lands there.
#include "autogain_iface.h"
#include "../app.h"

bool autogainRun(App& a, int radio, const std::string& mode) {
    (void)mode;
    if (radio < 0 || radio >= (int)a.devices.size() || !a.devices[radio].isRadio()) return false;
    if (!a.agcOn) { a.agcOn = true; agcReset(a); }
    return true;
}
