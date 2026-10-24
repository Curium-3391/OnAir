// The Unified layout (Settings > Display > Layout; docs/UI-LAYOUT.md). The shell lays out the central region once for every mode: a
// one-line header, the spectrum and waterfall in a fixed region (above, or on the left of a wide window), a thin splitter, and below it
// (or right of it) the mode's own tabs. A mode keeps drawing its content into its tabs; the "Overview" tab (which was only the spectrum)
// is not made. Included by main_tabs.cpp.
#include <map>
#include <sstream>

namespace {

struct UniSt { float top = 0.38f, side = 0.5f; bool col = false; };

std::map<int, UniSt>& uniMap(App& a) {
    static std::map<int, UniSt> m;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::stringstream ss(a.uniSplit);
        std::string item;
        while (std::getline(ss, item, ';')) {
            int fam = 0, c = 0; float t = 0.38f, s = 0.5f;
            if (sscanf(item.c_str(), "%d=%f,%f,%d", &fam, &t, &s, &c) == 4) m[fam] = UniSt{std::min(0.7f, std::max(0.15f, t)), std::min(0.8f, std::max(0.25f, s)), c != 0};
        }
    }
    return m;
}

void uniSave(App& a) {
    std::string out;
    char b[96];
    for (auto& kv : uniMap(a)) { snprintf(b, sizeof b, "%d=%.3f,%.3f,%d;", kv.first, kv.second.top, kv.second.side, kv.second.col ? 1 : 0); out += b; }
    a.uniSplit = out;
    savePrefs(a);
}

}  // namespace

template <class F> static void unifiedCentre(App& a, F tabs) {
    UniSt& st = uniMap(a)[a.family];
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float vpW = ImGui::GetMainViewport()->Size.x;
    const bool canSide = avail.x >= 640 * gUi;
    const bool side = a.uniSide == 2 ? canSide : a.uniSide == 1 ? false : (vpW >= 1500 && avail.x >= 900 * gUi);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 edge = ImGui::GetColorU32(ImGuiCol_Border), dim = ImGui::GetColorU32(ImGuiCol_TextDisabled), txt = ImGui::GetColorU32(ImGuiCol_Text);
    const ImGuiIO& io = ImGui::GetIO();
    bool changed = false;
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F6, false)) { st.col = !st.col; changed = true; }

    // ---- the header: a tool-window title bar with a fold arrow and a 1 px rule under it
    const float hh = std::floor(ImGui::GetTextLineHeight() + 5 * gUi);
    const ImVec2 h0 = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##uniHead", ImVec2(avail.x, hh));
    if (ImGui::IsItemClicked(0)) { st.col = !st.col; changed = true; }
    if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); ImGui::SetTooltip("%s", tr(st.col ? "Show the spectrum and waterfall (F6)" : "Fold the spectrum and waterfall to a strip (F6)")); }
    {
        const float cy = h0.y + hh * 0.5f, ax = h0.x + 8 * gUi, r = 3.5f * gUi;
        if (st.col) dl->AddTriangleFilled(ImVec2(ax - r * 0.6f, cy - r), ImVec2(ax - r * 0.6f, cy + r), ImVec2(ax + r, cy), dim);
        else dl->AddTriangleFilled(ImVec2(ax - r, cy - r * 0.6f), ImVec2(ax + r, cy - r * 0.6f), ImVec2(ax, cy + r), dim);
        char t[160];
        snprintf(t, sizeof t, "SPECTRUM  %.4f MHz", radioCenterMhz(a));
        const float ty = h0.y + (hh - ImGui::GetTextLineHeight()) * 0.5f;
        dl->AddText(ImVec2(h0.x + 18 * gUi, ty), txt, t);
        if (!a.engine.running()) dl->AddText(ImVec2(h0.x + 18 * gUi + ImGui::CalcTextSize(t).x + 12 * gUi, ty), dim, "stopped");
        dl->AddLine(ImVec2(h0.x, h0.y + hh - 1), ImVec2(h0.x + avail.x, h0.y + hh - 1), edge, 1.f);
    }
    ImGui::SameLine(0, 0);   // the placement toggle at the right end of the header
    {
        const char* lab = side ? "[left]" : "[top]";
        const float w = ImGui::CalcTextSize(lab).x + 10 * gUi;
        const ImVec2 b0(h0.x + avail.x - w - 4 * gUi, h0.y);
        ImGui::SetCursorScreenPos(b0);
        ImGui::InvisibleButton("##uniPlace", ImVec2(w, hh - 1));
        if (ImGui::IsItemClicked(0)) { a.uniSide = side ? 1 : 2; if (!canSide) a.uniSide = 1; changed = true; }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Spectrum above the decoded views, or on their left"));
        dl->AddText(ImVec2(b0.x + 5 * gUi, h0.y + (hh - ImGui::GetTextLineHeight()) * 0.5f), ImGui::IsItemHovered() ? txt : dim, lab);
    }
    ImGui::SetCursorScreenPos(ImVec2(h0.x, h0.y + hh));
    const float gap = 5 * gUi;
    const ImVec2 rest = ImGui::GetContentRegionAvail();
    gSpectrumInShell = true;
    auto drawSpec = [&](ImVec2 sz) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::BeginChild("##uniSpec", sz, 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();
        spectrumWaterfall(a, specWaterfallSplit(), true);
        ImGui::EndChild();
    };
    auto splitBar = [&](bool vertical, float len, float total, float& share, float lo, float hi) {
        ImGui::InvisibleButton("##uniSplit", vertical ? ImVec2(gap, len) : ImVec2(len, gap));
        const bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
        if (hov || act) ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
        const ImVec2 p0 = ImGui::GetItemRectMin(), p1 = ImGui::GetItemRectMax();
        const float m = vertical ? std::floor((p0.x + p1.x) * 0.5f) : std::floor((p0.y + p1.y) * 0.5f);
        const ImU32 col = (hov || act) ? ImGui::GetColorU32(ImGuiCol_SeparatorHovered) : edge;
        if (vertical) dl->AddLine(ImVec2(m, p0.y), ImVec2(m, p1.y), col, 1.f); else dl->AddLine(ImVec2(p0.x, m), ImVec2(p1.x, m), col, 1.f);
        if (hov && ImGui::IsMouseDoubleClicked(0)) { share = vertical ? 0.5f : 0.38f; changed = true; }
        else if (act && ImGui::IsMouseDragging(0, 0)) {
            const float d = vertical ? io.MouseDelta.x : io.MouseDelta.y;
            if (d != 0) { share = std::min(hi, std::max(lo, share + d / std::max(1.f, total))); changed = true; }
        }
    };
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    if (st.col) {
        ImGui::BeginChild("##uniTabs", ImVec2(0, 0), 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImGui::GetStyle().ItemSpacing);
        tabs();
        ImGui::PopStyleVar();
        ImGui::EndChild();
    } else if (!side) {
        const float specH = std::floor(std::min(std::max(rest.y * st.top, 120 * gUi), std::max(120.f * gUi, rest.y - gap - 150 * gUi)));
        drawSpec(ImVec2(0, specH));
        splitBar(false, rest.x, rest.y, st.top, 0.15f, 0.7f);
        ImGui::BeginChild("##uniTabs", ImVec2(0, 0), 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImGui::GetStyle().ItemSpacing);
        tabs();
        ImGui::PopStyleVar();
        ImGui::EndChild();
    } else {
        const float specW = std::floor(std::min(std::max(rest.x * st.side, 300 * gUi), std::max(300.f * gUi, rest.x - gap - 320 * gUi)));
        drawSpec(ImVec2(specW, rest.y));
        ImGui::SameLine(0, 0);
        splitBar(true, rest.y, rest.x, st.side, 0.25f, 0.8f);
        ImGui::SameLine(0, 0);
        ImGui::BeginChild("##uniTabs", ImVec2(0, 0), 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImGui::GetStyle().ItemSpacing);
        tabs();
        ImGui::PopStyleVar();
        ImGui::EndChild();
    }
    ImGui::PopStyleVar();
    gSpectrumInShell = false;
    if (changed) uniSave(a);
}
