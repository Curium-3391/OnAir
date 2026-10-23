// View > LAN swarm: opt-in discovery of other OnAir nodes on this local network and the swarm view (name, radio, tuned range, mode, load, summary).
// Off every time the program starts. Before it is switched on, the exact announcement is shown. Pooled results, coordinated scanning, pairing and
// memory sync are in core (swarm.h) but not wired to the network or this window yet. NOT yet compiled (written while builds were forbidden).
#include "../app.h"
#include "../ui/typography.h"
#include "dect2/net/swarm_net.h"
#include <cstdio>
#include <mutex>
#include <random>

namespace {
using namespace dect2::swarm;

struct SwarmState {
    SwarmNet net;
    std::mutex mu;
    Announce mine;          // refreshed from the UI thread, read by the network thread
    bool on = false;
    bool previewing = false;
    std::string ownId, err;
};
SwarmState& S() { static SwarmState s; return s; }

std::string newNodeId() {
    std::random_device rd;
    char b[20];
    snprintf(b, sizeof b, "%08x%08x", (unsigned)rd(), (unsigned)rd());
    return b;
}

Announce describe(App& a) {
    Announce n;
    const DeviceInfo& d = a.devices[(size_t)std::max(0, std::min(a.devIdx, (int)a.devices.size() - 1))];
    const bool run = a.engine.running();
    std::string mode = "idle";
    if (run) for (int i = 0; i < kNumModes; i++) if (kModes[i].family == a.family && (!kModes[i].subId || subIndexOf(kModes[i].subId) == a.sub)) { mode = kModes[i].name; break; }
    n.id = S().ownId;
    n.name = "OnAir";
    n.radio = a.devices.empty() ? "none" : d.name;
    n.mode = mode;
    n.loHz = a.devices.empty() ? 0 : std::max(0.0, d.minFreqHz);
    n.hiHz = a.devices.empty() ? 0 : std::max(n.loHz, d.maxFreqHz);
    n.summary = run ? "receiving" : "stopped";
    n.load = 0;     // TODO: engine CPU load once the engine exposes it
    n.ctlPort = 0;  // pairing is not offered until the control channel exists
    return n;
}
}   // namespace

void swarmWindow(App& a) {
    if (a.rxId != 1) {   // one swarm per program: receiver 1 owns the window
        if (App* f = receiverById(1)) { f->swarmOpen = a.swarmOpen; swarmWindow(*f); a.swarmOpen = f->swarmOpen; }
        return;
    }
    SwarmState& s = S();
    if (s.on) { std::lock_guard<std::mutex> g(s.mu); s.mine = describe(a); }
    if (!a.swarmOpen) return;
    ImGui::SetNextWindowSize(ImVec2(560 * gUi, 0), ImGuiCond_Appearing);
    if (!ImGui::Begin(trw("LAN swarm"), &a.swarmOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::End(); return; }
    ImGui::PushTextWrapPos(540 * gUi);
    ImGui::TextDisabled("%s", tr("Finds other OnAir programs on this local network and lists what they are doing. Off until you switch it on. Stays on this network (nothing goes to the internet) and sends no IQ and no audio."));
    ImGui::PopTextWrapPos();
    if (s.ownId.empty()) s.ownId = newNodeId();

    if (!s.on) {
        if (!s.previewing) {
            if (ImGui::Button(tr("Join the swarm..."))) s.previewing = true;
        } else {
            ImGui::Separator();
            ImGui::TextUnformatted(tr("This is exactly what will be announced every 5 seconds:"));
            ImGui::PushTextWrapPos(540 * gUi);
            ImGui::TextUnformatted(announcePreview(describe(a)).c_str());
            ImGui::PopTextWrapPos();
            if (ImGui::Button(tr("Switch on"))) {
                s.err.clear();
                { std::lock_guard<std::mutex> g(s.mu); s.mine = describe(a); }
                SwarmState* sp = &s;
                if (s.net.start(s.ownId, [sp] { std::lock_guard<std::mutex> g(sp->mu); return sp->mine; }, s.err)) s.on = true;
                s.previewing = false;
            }
            ImGui::SameLine();
            if (ImGui::Button(tr("Cancel"))) s.previewing = false;
        }
        if (!s.err.empty()) ImGui::TextColored(pal::warnAmber(), "%s", s.err.c_str());
    } else {
        if (ImGui::Button(tr("Leave the swarm"))) { s.net.stop(); s.on = false; }
        const NetStats st = s.net.stats();
        ImGui::SameLine();
        ImGui::TextDisabled(tr("%d network(s), %llu seen, %llu ignored"), st.interfaces, (unsigned long long)st.received, (unsigned long long)st.rejected);
        const std::vector<Node> nodes = s.net.nodes();
        if (nodes.empty()) ImGui::TextDisabled("%s", tr("No other nodes seen yet."));
        else if (ImGui::BeginTable("swarm", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn(tr("Name")); ImGui::TableSetupColumn(tr("Radio")); ImGui::TableSetupColumn(tr("Range (MHz)"));
            ImGui::TableSetupColumn(tr("Mode")); ImGui::TableSetupColumn(tr("Load")); ImGui::TableSetupColumn(tr("Summary"));
            ImGui::TableHeadersRow();
            for (const Node& n : nodes) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(n.info.name.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(n.info.radio.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%.1f - %.1f", n.info.loHz / 1e6, n.info.hiHz / 1e6);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(n.info.mode.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%d%%", n.info.load);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(n.info.summary.c_str());
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}
