// The setup wizard for a first-time user: welcome and language, "what do you want to do?", the radio (found, explained, or how to fix
// the driver; else the demo signal), the antenna, the region and location, then a summary that starts the hands-on lessons (learn.cpp).
// The step is saved, so a setup closed halfway resumes there. Goals, antennas, regions and the radio notes are in learn_logic.h (tested).
#include "../app.h"
#include "../panels/fixit.h"
#include "dect2/geo/maidenhead.h"
#include "dect2/engine/mode_tuning.h"
#include "dect2/engine/updater.h"
#include "tour_logic.h"
#include "learn_logic.h"

// The first-run tour is in tour.cpp; the lessons, the glossary and the checkup in learn.cpp.

namespace {

constexpr int kSteps = 6;   // welcome, goal, radio, antenna, region, ready

// the first ticked goal (in table order) picks the starting mode
int pickGoal(unsigned g) { for (int i = 0; i < learn::GoalCount; i++) if (g & (1u << i)) return i; return learn::GoalExplore; }

std::string goalModeName(int goal, const learn::Region& r) {
    const std::string id = learn::goals()[goal].modeId;
    if (id == "fm") { char b[48]; snprintf(b, sizeof b, " (%.1f MHz)", r.fmMhz); return std::string(tr("FM radio")) + b; }
    if (id == "tv") return r.tvId ? modeTuningById(r.tvId)->name : r.tvFamily == 1 ? "ATSC" : r.tvFamily == 4 ? "ISDB-T" : r.tvStd == 2 ? "DVB-T" : "DVB-T2";
    int n = 0;
    const SubMode* t = subModes(n);
    for (int i = 0; i < n; i++) if (id == t[i].id) return t[i].name;
    if (const ModeTuning* mt = modeTuningById(id)) return mt->name;
    return id;
}

bool parseLoc(const char* text, double& lat, double& lon) {
    const std::string s = text;
    if (s.empty()) return false;
    return dect2::fromMaidenhead(s, lat, lon) || dect2::parseCoordinates(s, lat, lon);
}

void saveStep(const App& a) { plat::prefs().setI(learn::prefSetupStep(), a.setupStep); plat::prefs().flush(); }

// a fixit entry: why, the steps for this computer, its buttons
void fixBox(const fixit::Entry& e) {
    ImGui::TextColored(pal::warnAmber(), "%s", tr(e.title));
    ImGui::TextUnformatted(tr(e.why));
    int k = 1;
    for (const char* st : fixit::stepsFor(e, fixit::currentOs())) { ImGui::Text("%d.", k++); ImGui::SameLine(); ImGui::TextUnformatted(tr(st)); }
    bool first = true;
    for (int b = 0; b < 3 && e.buttons[b].label; b++) {
        if (!(e.buttons[b].os & fixit::currentOs())) continue;
        if (!first) ImGui::SameLine();
        first = false;
        ImGui::PushID(b);
        if (ImGui::Button(tr(e.buttons[b].label))) {
            if (e.buttons[b].act == fixit::Act::OpenUrl) dect2::openUrl(e.buttons[b].arg);
            else if (e.buttons[b].act == fixit::Act::CopyText) ImGui::SetClipboardText(e.buttons[b].arg);
        }
        ImGui::PopID();
    }
}

// the OS's own note on drivers, in one place
const char* driverNote() {
    switch (fixit::currentOs()) {
    case fixit::OsWin: return N_("Windows: most USB radios (RTL-SDR, HackRF, Airspy) need the WinUSB driver once. Run Zadig, pick the radio, choose WinUSB and press Install. SDRplay radios use the SDRplay API instead.");
    case fixit::OsLinux: return N_("Linux: your account needs permission to open the radio. The udev rules (install-udev-rules.sh, done by the .deb) give it; unplug and replug afterwards. If the kernel's TV driver grabs an RTL-SDR, the rules blacklist it.");
    default: return N_("macOS: USB radios need no driver. If one is not found, try another cable or port (some cables only charge), and allow OnAir if macOS asks about USB accessories. SDRplay radios need the SDRplay API.");
    }
}

void goalStep(App& a) {
    ImGui::TextUnformatted(tr("What do you want to do? Tick everything that interests you; the first one picks where OnAir starts."));
    ImGui::Spacing();
    const learn::GoalDef* G = learn::goals();
    for (int i = 0; i < learn::GoalCount; i++) {
        bool on = (a.setupGoals >> i) & 1u;
        ImGui::PushID(i);
        if (ImGui::Checkbox(tr(G[i].label), &on)) a.setupGoals = on ? (a.setupGoals | (1u << i)) : (a.setupGoals & ~(1u << i));
        ImGui::Indent(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextDisabled("%s", tr(G[i].blurb));
        ImGui::Unindent(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::PopID();
    }
}

void radioStep(App& a) {
    ImGui::TextUnformatted(tr("Your radio. OnAir looks for radios on USB; a radio on the network is added by its address."));
    ImGui::Spacing();
    int radios = 0, demo = -1, cur = -1;
    for (int i = 0; i < (int)a.devices.size(); i++) {
        const DeviceInfo& d = a.devices[i];
        if (d.kind == DeviceInfo::Synthetic && demo < 0) demo = i;
        if (!d.isRadio()) continue;
        radios++;
        ImGui::PushID(i);
        if (ImGui::RadioButton(d.name.c_str(), a.devIdx == i)) a.devIdx = i;
        if (d.isNetwork()) { ImGui::SameLine(); ImGui::TextDisabled("%s", tr("(network)")); }
        ImGui::Indent(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextDisabled("%s", tr(learn::radioBlurb(d.board, d.name)));
        ImGui::Unindent(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::PopID();
        if (a.devIdx == i) cur = i;
    }
    if (demo >= 0) {
        if (ImGui::RadioButton(tr("No radio: use the built-in demo signal"), a.devIdx == demo)) a.devIdx = demo;
        ImGui::Indent(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextDisabled("%s", tr("A test signal made inside OnAir (a TV programme, FM and more). Everything in the lessons works with it; recordings (IQ files) open from Source."));
        ImGui::Unindent(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x);
    }
    ImGui::Spacing();
    if (!radios) {
        if (const fixit::Entry* e = fixit::byId("no_radio")) fixBox(*e);
        ImGui::Spacing();
    }
    // the last start failed: the matching fix, in plain words
    if (!a.startErr.empty() && (cur >= 0 || !radios)) {
        if (const fixit::Entry* e = fixit::lookup(a.startErr)) { if (std::string(e->id) != "no_radio") { fixBox(*e); ImGui::Spacing(); } }
    }
    if (ImGui::TreeNode(trw("Driver and permission notes for this computer"))) {
        ImGui::TextUnformatted(tr(driverNote()));
        if (const fixit::Entry* e = fixit::byId("usb_permission")) { ImGui::Spacing(); fixBox(*e); }
        ImGui::TreePop();
    }
    ImGui::Spacing();
    if (ImGui::Button(tr("Look again"))) refreshDevices(a);
    ImGui::SameLine();
    if (ImGui::Button(tr("Add a network radio..."))) { a.netAddOpen = true; a.netAddErr.clear(); }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("A radio on another computer or on the network: a PlutoSDR by IP, an rtl_tcp server, a SpyServer, an SDR++ server, SoapyRemote, a Hermes Lite 2 or a Red Pitaya"));
}

void antennaStep(App& a) {
    ImGui::TextUnformatted(tr("Which antenna do you have? The antenna matters more than the radio; the checkup at the end gives tips for it."));
    ImGui::Spacing();
    const learn::AntennaDef* A = learn::antennas();
    for (int i = 0; i < learn::AntCount; i++) {
        ImGui::PushID(i);
        if (ImGui::RadioButton(tr(A[i].label), a.setupAntenna == i)) a.setupAntenna = i;
        ImGui::Indent(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextDisabled("%s", tr(A[i].blurb));
        ImGui::Unindent(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::PopID();
    }
}

void regionStep(App& a) {
    const float u = gUi;
    int nr = 0;
    const learn::Region* RG = learn::regions(&nr);
    ImGui::TextUnformatted(tr("Where are you? This sets the TV standard, the FM band and its de-emphasis, the channel plan and the units."));
    ImGui::Spacing();
    ImGui::SetNextItemWidth(280 * u);
    if (ImGui::BeginCombo("##region", tr(RG[a.setupRegion].name))) {
        for (int i = 0; i < nr; i++) if (ImGui::Selectable(tr(RG[i].name), i == a.setupRegion)) a.setupRegion = i;
        ImGui::EndCombo();
    }
    const learn::Region& r = RG[a.setupRegion];
    ImGui::TextDisabled(tr("TV: %s.  FM: %.1f to %.1f MHz, %d us de-emphasis.  Distances in %s."), goalModeName(learn::GoalTv, r).c_str(), r.fmLoMhz, r.fmHiMhz, r.fmDeemphUs, tr(r.imperial ? "miles" : "km"));
    ImGui::Spacing();
    ImGui::TextUnformatted(tr("Optional: your position, as a Maidenhead locator (KO24pq) or coordinates (54.68, 25.28). It is kept on this computer and used for maps and satellite passes."));
    ImGui::TextDisabled("%s", tr("Privacy: the location never leaves this computer unless you turn on sending to a public map yourself."));
    ImGui::SetNextItemWidth(260 * u);
    ImGui::InputText("##loc", a.setupLoc, sizeof a.setupLoc);
    double lat, lon;
    if (a.setupLoc[0]) {
        if (parseLoc(a.setupLoc, lat, lon)) ImGui::TextDisabled("%.3f, %.3f  (%s)", lat, lon, dect2::toMaidenhead(lat, lon, 6).c_str());
        else ImGui::TextColored(pal::warnAmber(), "%s", tr("Not a locator or coordinates"));
    }
}

void readyStep(App& a) {
    int nr = 0;
    const learn::Region* RG = learn::regions(&nr);
    const int g = pickGoal(a.setupGoals);
    ImGui::TextUnformatted(tr("All set. OnAir will start here:"));
    ImGui::Spacing();
    ImGui::TextColored(pal::accent(), "%s", goalModeName(g, RG[a.setupRegion]).c_str());
    ImGui::Spacing();
    const bool okDev = a.devIdx >= 0 && a.devIdx < (int)a.devices.size();
    ImGui::TextDisabled(tr("Radio: %s"), okDev ? a.devices[a.devIdx].name.c_str() : tr("none"));
    ImGui::TextDisabled(tr("Antenna: %s"), tr(learn::antennas()[std::max(0, std::min(learn::AntCount - 1, a.setupAntenna))].label));
    const std::vector<int> plan = learn::lessonPlan(a.setupGoals);
    ImGui::Spacing();
    ImGui::Text(tr("Next: %d short hands-on lessons on the real controls (tuning, gain, the waterfall, modes, sound, recording, and your goals). Each one takes under a minute and can be skipped."), (int)plan.size());
    ImGui::TextDisabled("%s", tr("Everything here can be changed later, and the setup and the lessons run again from Help > Setup and lessons."));
}

}  // namespace

void setupWizardOpen(App& a) {
    a.setupOpen = true;
    // first run: resume a setup closed halfway; from the Help menu: from the start
    a.setupStep = plat::prefs().getB("setupDone", false) ? 0 : learn::setupResumeStep(plat::prefs().getI(learn::prefSetupStep(), 0), kSteps);
    a.setupRegion = learn::clampRegion(plat::prefs().getI("setupRegion", 0));
    a.setupGoals = (unsigned)plat::prefs().getI("setupGoals", 0);
    a.setupAntenna = (int)std::max(0L, std::min((long)learn::AntCount - 1, plat::prefs().getI(learn::prefAntenna(), learn::AntWhip)));
    snprintf(a.setupLoc, sizeof a.setupLoc, "%s", plat::prefs().getS("setupLoc", "").c_str());
}

// how = 0 skip (keep the answers given so far, change nothing), 1 finish and start the lessons, 2 finish only, 3 finish and take the window tour
static void setupFinish(App& a, int how) {
    a.setupOpen = false;
    const learn::Region& r = learn::regions(nullptr)[a.setupRegion];
    if (how) {
        selectModeById(a, learn::goals()[pickGoal(a.setupGoals)].modeId);
        a.fmDeemph = r.fmDeemphUs;
        if (a.bandPlan.empty() && *r.bandPlan) a.bandPlan = r.bandPlan;
    }
    plat::prefs().setB("setupDone", true);
    plat::prefs().setI(learn::prefSetupStep(), 0);
    plat::prefs().setI("setupRegion", a.setupRegion);
    plat::prefs().setI("setupGoals", (long)a.setupGoals);
    plat::prefs().setI(learn::prefAntenna(), a.setupAntenna);
    plat::prefs().setB("unitsImperial", r.imperial);
    plat::prefs().setS("setupLoc", a.setupLoc);
    double lat, lon;
    if (parseLoc(a.setupLoc, lat, lon)) { plat::prefs().setD("adsbLat", lat); plat::prefs().setD("adsbLon", lon); }
    savePrefs(a);
    plat::prefs().flush();
    if (how == 1) { plat::prefs().setB(tour::prefKey(), true); lessonsStart(a); }   // the lessons cover the tour's ground, on the real controls
    else if (how == 3 || (how == 0 && !plat::prefs().getB(tour::prefKey(), false))) tourStart(a);
}

void setupWizard(App& a, ImVec2 disp) {
    if (!a.setupOpen) return;
    const float u = gUi;
    ImGui::SetNextWindowPos(ImVec2(disp.x * 0.5f, disp.y * 0.45f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(640.f * u, disp.x - 24 * u), 0));
    ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, disp.y - 24 * u));
    ImGui::PushStyleColor(ImGuiCol_Border, pal::accent(0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::Begin("##setup", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    static const char* const kNames[kSteps] = {N_("Welcome"), N_("Your goal"), N_("Radio"), N_("Antenna"), N_("Region"), N_("Ready")};
    ImGui::TextColored(pal::accent(), "%s  (%d / %d)", tr(kNames[a.setupStep]), a.setupStep + 1, kSteps);
    {   // the steps as dots
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float r = 4 * u, sp = 14 * u;
        ImGui::SameLine(ImGui::GetWindowWidth() - kSteps * sp - 10 * u);
        const ImVec2 c = ImGui::GetCursorScreenPos();
        for (int i = 0; i < kSteps; i++) {
            const ImVec2 p(c.x + i * sp + r, c.y + ImGui::GetTextLineHeight() * 0.5f);
            if (i <= a.setupStep) dl->AddCircleFilled(p, r, ImGui::ColorConvertFloat4ToU32(pal::accent(i == a.setupStep ? 1.f : 0.5f)));
            else dl->AddCircle(p, r, IM_COL32(255, 255, 255, 80));
        }
        ImGui::Dummy(ImVec2(kSteps * sp, ImGui::GetTextLineHeight()));
    }
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + std::min(600.f * u, disp.x - 60 * u));
    switch (a.setupStep) {
    case 0: {
        ImGui::TextUnformatted(tr("Welcome to OnAir. A radio receiver on your computer can hear FM stations, aircraft, ships, satellites, TV and much more."));
        ImGui::TextUnformatted(tr("This setup asks five short questions, then shows you the controls with a few hands-on lessons. You can stop at any time; it continues where you left."));
        ImGui::Spacing();
        ImGui::TextUnformatted(tr("Which language do you want OnAir in?"));
        if (ImGui::RadioButton(tr("Same as the system"), a.uiLang.empty())) setUiLanguage(a, "");
        int n = 0;
        const i18n::Language* L = i18n::languages(n);
        for (int i = 0; i < n; i++) if (ImGui::RadioButton(L[i].name, !a.uiLang.empty() && a.uiLang == L[i].code)) setUiLanguage(a, L[i].code);
        break;
    }
    case 1: goalStep(a); break;
    case 2: radioStep(a); break;
    case 3: antennaStep(a); break;
    case 4: regionStep(a); break;
    default: readyStep(a); break;
    }
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    ImGui::Separator();
    const float bh = a.touchUi ? 40.f * u : 0.f;
    const int before = a.setupStep;
    if (a.setupStep > 0 && ImGui::Button(tr("Back"), ImVec2(70 * u, bh))) a.setupStep--;
    if (before > 0) ImGui::SameLine();
    const bool last = before == kSteps - 1;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.34f, 0.40f, 1)); ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.27f, 0.45f, 0.53f, 1));
    int finish = -1;
    if (last) {
        if (ImGui::Button(tr("Start the lessons"), ImVec2(0, bh))) finish = 1;
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        if (ImGui::Button(tr("Just start"), ImVec2(0, bh))) finish = 2;
        ImGui::SameLine();
        if (ImGui::Button(tr("Window tour instead"), ImVec2(0, bh))) finish = 3;
    } else {
        if (ImGui::Button(tr("Next"), ImVec2(90 * u, bh))) a.setupStep++;
        ImGui::PopStyleColor(2);
    }
    ImGui::SameLine(0, 18 * u);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    if (!last && ImGui::Button(tr("Later"), ImVec2(0, bh))) { a.setupOpen = false; saveStep(a); }   // closes it; the next start resumes here
    if (!last && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Close the setup for now. It continues at this step next time, or from Help > Setup and lessons."));
    if (!last) ImGui::SameLine();
    if (ImGui::Button(tr("Skip setup"), ImVec2(0, bh))) finish = 0;
    ImGui::PopStyleColor();
    if (!ImGui::GetIO().WantTextInput && ImGui::IsWindowFocused()) {   // keyboard: Enter for Next, Escape for Later
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) && !last) a.setupStep++;
        else if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !last) { a.setupOpen = false; saveStep(a); }
    }
    ImGui::End();
    if (a.setupStep != before) saveStep(a);
    if (finish >= 0) setupFinish(a, finish);
}
