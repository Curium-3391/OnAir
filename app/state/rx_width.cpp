// The "Receive bandwidth" control and its saved value (rx_width.h).
#include "rx_width.h"
#include "../app.h"
#include <cstdio>
#include <string>

static std::string key(int stdMode) {
    const ModeTuning* mt = modeTuning(stdMode);
    return std::string("rxbw.") + (mt ? mt->id : "x");
}

static double savedWidth(int stdMode) {
    const double w = plat::prefs().getD(key(stdMode).c_str(), 0);
    return w > 0 ? std::min(kRxWidthMaxMhz, std::max(kRxWidthMinMhz, w)) : 0;
}
static bool customOf(int stdMode) { return bwCustomTicked((int)plat::prefs().getI((key(stdMode) + ".custom").c_str(), -1), savedWidth(stdMode)); }

// 0 (the mode's own standard bandwidth) unless the user ticked Custom
double rxWidthMhz(int stdMode) {
    if (!rxWidthMode(stdMode)) return 0;
    return bwEffective(customOf(stdMode), savedWidth(stdMode));
}

// the decoder options row of VDL2, TETRA and TETRAPOL: the mode's standard bandwidth as text, and a Custom tick that opens the choice
void rxWidthControl(App& a, bool& retune) {
    (void)retune;
    const ModeTuning* mt = a.family >= 6 ? modeTuning(a.family + 2) : nullptr;
    if (!mt || !rxWidthMode(mt->stdMode)) return;
    bool custom = customOf(mt->stdMode);
    const double saved = savedWidth(mt->stdMode), cur = custom ? saved : 0;
    static const double kPre[] = {0.25, 0.5, 1.0, 1.5, 2.0};
    double pick = -1;
    bool ticked = false, openCustom = false;
    flowNext(10 * gUi);   // wraps with the rest of the row when the bar is narrow (it was cut off at the right edge)
    char sb[64];
    if (mt->bandwidthMhz < 1) std::snprintf(sb, sizeof sb, "%s: %g kHz (%s)", tr("Bandwidth"), mt->bandwidthMhz * 1e3, tr("standard"));
    else std::snprintf(sb, sizeof sb, "%s: %g MHz (%s)", tr("Bandwidth"), mt->bandwidthMhz, tr("standard"));
    ImGui::TextDisabled("%s", custom ? tr("Receive bandwidth") : sb);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("How much spectrum the radio captures around the frequency, up to 2 MHz, to receive several channels at once. The standard bandwidth is the mode's own width; tick Custom to change it. A wider capture needs a higher sample rate and more processor time."));
    flowNext(8 * gUi);
    if (ImGui::Checkbox(tr("Custom##rxbwcustom"), &custom)) {
        plat::prefs().setI((key(mt->stdMode) + ".custom").c_str(), custom ? 1 : 0);
        if (custom && saved <= 0) plat::prefs().setD(key(mt->stdMode).c_str(), 1.0);   // a first tick starts at 1 MHz
        plat::prefs().flush();
        ticked = true;
    }
    if (custom) {
        char pv[48];
        std::snprintf(pv, sizeof pv, "%g MHz", cur > 0 ? cur : (saved > 0 ? saved : 1.0));
        flowNext(6 * gUi);
        ImGui::SetNextItemWidth(130 * gUi);
        const double shown = cur > 0 ? cur : (saved > 0 ? saved : 1.0);
        if (ImGui::BeginCombo("##rxbw", pv)) {
            for (double p : kPre) { char l[24]; std::snprintf(l, sizeof l, "%g MHz", p); if (ImGui::Selectable(l, shown == p)) pick = p; }
            if (ImGui::Selectable(tr("Other…"))) openCustom = true;
            ImGui::EndCombo();
        }
        static double typed = 1.0;
        if (openCustom) { typed = shown; ImGui::OpenPopup("##rxbwc"); }
        if (ImGui::BeginPopup("##rxbwc")) {
            ImGui::TextDisabled("%s", tr("Bandwidth (MHz), 0.05 to 2"));
            ImGui::SetNextItemWidth(120 * gUi);
            if (ImGui::InputDouble("##rxbwv", &typed, 0, 0, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue)) { pick = typed; ImGui::CloseCurrentPopup(); }
            if (ImGui::Button(tr("Apply"))) { pick = typed; ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
    }
    if (pick >= 0) {
        plat::prefs().setD(key(mt->stdMode).c_str(), std::min(kRxWidthMaxMhz, std::max(kRxWidthMinMhz, pick)));
        plat::prefs().flush();
    } else if (!ticked) return;
    const double oldRate = a.tune.sampleRate, oldBw = a.tune.bandwidthMhz;
    a.tune.centerHz = a.freqMhz * 1e6;
    applyBandwidth(a);
    if (a.engine.running() && (a.tune.sampleRate != oldRate || a.tune.bandwidthMhz != oldBw)) startReceiver(a);
}
