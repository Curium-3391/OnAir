// The "Receive bandwidth" control and its saved value (rx_width.h).
#include "rx_width.h"
#include "../app.h"
#include <cstdio>
#include <string>

static std::string key(int stdMode) {
    const ModeTuning* mt = modeTuning(stdMode);
    return std::string("rxbw.") + (mt ? mt->id : "x");
}

double rxWidthMhz(int stdMode) {
    if (!rxWidthMode(stdMode)) return 0;
    const double w = plat::prefs().getD(key(stdMode).c_str(), 0);
    return w > 0 ? std::min(kRxWidthMaxMhz, std::max(kRxWidthMinMhz, w)) : 0;
}

// the decoder options row of VDL2, TETRA and TETRAPOL: Advanced, a receive bandwidth (Auto by default)
void rxWidthControl(App& a, bool& retune) {
    (void)retune;
    const ModeTuning* mt = a.family >= 6 ? modeTuning(a.family + 2) : nullptr;
    if (!mt || !rxWidthMode(mt->stdMode)) return;
    const double cur = rxWidthMhz(mt->stdMode);
    static const double kPre[] = {0.25, 0.5, 1.0, 1.5, 2.0};
    char pv[48];
    if (cur > 0) std::snprintf(pv, sizeof pv, "%g MHz", cur); else std::snprintf(pv, sizeof pv, "%s", tr("Auto"));
    flowNext(10 * gUi);   // wraps with the rest of the row when the bar is narrow (it was cut off at the right edge)
    ImGui::TextDisabled("%s", tr("Advanced: receive bandwidth"));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("How much spectrum the radio captures around the frequency, up to 2 MHz, to receive several channels at once. Auto uses the mode's own width. A wider capture needs a higher sample rate and more processor time."));
    flowNext(6 * gUi);
    ImGui::SetNextItemWidth(130 * gUi);
    double pick = -1;
    bool openCustom = false;
    if (ImGui::BeginCombo("##rxbw", pv)) {
        if (ImGui::Selectable(tr("Auto"), cur <= 0)) pick = 0;
        for (double p : kPre) { char l[24]; std::snprintf(l, sizeof l, "%g MHz", p); if (ImGui::Selectable(l, cur == p)) pick = p; }
        if (ImGui::Selectable(tr("Custom…"))) openCustom = true;
        ImGui::EndCombo();
    }
    static double typed = 1.0;
    if (openCustom) { typed = cur > 0 ? cur : 1.0; ImGui::OpenPopup("##rxbwc"); }
    if (ImGui::BeginPopup("##rxbwc")) {
        ImGui::TextDisabled("%s", tr("Bandwidth (MHz), 0.05 to 2"));
        ImGui::SetNextItemWidth(120 * gUi);
        if (ImGui::InputDouble("##rxbwv", &typed, 0, 0, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue)) { pick = typed; ImGui::CloseCurrentPopup(); }
        if (ImGui::Button(tr("Apply"))) { pick = typed; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if (pick < 0) return;
    const double w = pick > 0 ? std::min(kRxWidthMaxMhz, std::max(kRxWidthMinMhz, pick)) : 0;
    plat::prefs().setD(key(mt->stdMode).c_str(), w);
    plat::prefs().flush();
    const double oldRate = a.tune.sampleRate, oldBw = a.tune.bandwidthMhz;
    a.tune.centerHz = a.freqMhz * 1e6;
    applyBandwidth(a);
    if (a.engine.running() && (a.tune.sampleRate != oldRate || a.tune.bandwidthMhz != oldBw)) startReceiver(a);
}
