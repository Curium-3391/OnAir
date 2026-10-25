// DMR Map and Table sub-tabs: the LRRP positions of the radios (see core/src/dmr/dmr_lrrp.cpp)
// drawn on the shared slippy map, with Src / TG / age filters, per-radio movement tracks, a table
// of the received positions and a CSV export.
#include "dmr_map.h"
#include "app.h"
#include "../adsb/adsb_map.h"
#include "dect2/common/u8file.h"
#include "dect2/geo/maplayers.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr double kStaleSec = 300.0;   // a radio not heard for this long is drawn grey

struct State {
    bool loaded = false;
    adsbmap::View map;
    char srcBuf[24] = "";   // filter: source radio id (decimal; empty = all)
    char tgBuf[24] = "";    // filter: talkgroup
    int ageMin = 0;         // 0 = all, else only the last N minutes
    uint32_t selSrc = 0;    // the radio selected on the map (0 = none)
    bool selValid = false;
    std::string msg;        // CSV export feedback
};
State S;

bool live(const App& a) { return a.rx.standard == 10; }

void load() {
    if (S.loaded) return;
    S.loaded = true;
    receiverLocation(S.map.lat, S.map.lon);
    S.map.zoom = 8;
}

uint32_t parseId(const char* s) {
    char* e = nullptr;
    const unsigned long v = std::strtoul(s, &e, 10);
    return (e && *e == '\0') ? (uint32_t)v : 0;
}

bool pass(const DmrLocation& loc, double now) {
    if (S.srcBuf[0]) { const uint32_t f = parseId(S.srcBuf); if (f && loc.src != f) return false; }
    if (S.tgBuf[0]) { const uint32_t f = parseId(S.tgBuf); if (f && loc.dst != f) return false; }
    if (S.ageMin > 0 && now - loc.unixSec > S.ageMin * 60.0) return false;
    return true;
}

// latest fix of each radio that passes the filters, with its movement track
std::vector<dect2::MapMarker> dmrMarkers(const DmrTelemetry& t, double now) {
    std::vector<dect2::MapMarker> v;
    for (size_t i = t.locations.size(); i-- > 0;) {
        const DmrLocation& loc = t.locations[i];
        if (!loc.hasFix || !pass(loc, now)) continue;
        const std::string id = std::to_string(loc.src);
        bool have = false;
        for (const auto& m : v) if (m.id == id) { have = true; break; }
        if (have) continue;
        dect2::MapMarker m; m.layer = dect2::LayerDmr; m.id = id; m.label = id;
        m.lat = loc.lat; m.lon = loc.lon;
        if (loc.hasSpeed) { m.hasSpeed = true; m.speedKt = loc.speedMs * 1.943844; }
        m.headingDeg = loc.hasHeading ? loc.headingDeg : -1;
        m.ageSec = std::max(0.0, now - loc.unixSec);
        char b[64]; snprintf(b, sizeof b, "TG %u, slot %d", loc.dst, loc.slot);
        m.detail = b;
        v.push_back(std::move(m));
    }
    for (auto& m : v) {
        const uint32_t src = (uint32_t)std::stoul(m.id);
        for (const DmrLocation& loc : t.locations)
            if (loc.src == src && loc.hasFix && pass(loc, now)) m.track.push_back({(float)loc.lat, (float)loc.lon});
    }
    return v;
}

void filters() {
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Src");
    ImGui::SameLine(0, 5 * gUi);
    ImGui::SetNextItemWidth(90 * gUi);
    ImGui::InputTextWithHint("##dmrsrc", "all", S.srcBuf, sizeof S.srcBuf);
    ImGui::SameLine(0, 10 * gUi);
    ImGui::TextDisabled("TG");
    ImGui::SameLine(0, 5 * gUi);
    ImGui::SetNextItemWidth(90 * gUi);
    ImGui::InputTextWithHint("##dmrtg", "all", S.tgBuf, sizeof S.tgBuf);
    ImGui::SameLine(0, 10 * gUi);
    ImGui::TextDisabled("last");
    ImGui::SameLine(0, 5 * gUi);
    ImGui::SetNextItemWidth(120 * gUi);
    static const char* ages[] = {"all", "1 min", "5 min", "15 min", "30 min", "1 h"};
    static const int mins[] = {0, 1, 5, 15, 30, 60};
    int cur = 0;
    for (int i = 0; i < 6; i++) if (S.ageMin == mins[i]) cur = i;
    if (ImGui::BeginCombo("##dmrage", ages[cur])) {
        for (int i = 0; i < 6; i++) if (ImGui::Selectable(ages[i], cur == i)) S.ageMin = mins[i];
        ImGui::EndCombo();
    }
}

// one marker colour per radio, stable for the whole session
constexpr ImU32 kDmrColours[] = {
    IM_COL32(0x22, 0xD3, 0xEE, 255), IM_COL32(0xF5, 0x9E, 0x0B, 255), IM_COL32(0x34, 0xD3, 0x99, 255),
    IM_COL32(0xA7, 0x8B, 0xFA, 255), IM_COL32(0xFB, 0x71, 0x85, 255), IM_COL32(0x60, 0xA5, 0xFA, 255),
    IM_COL32(0xFA, 0xCC, 0x15, 255), IM_COL32(0xE8, 0x79, 0xF9, 255), IM_COL32(0xF9, 0x73, 0x16, 255),
    IM_COL32(0x2D, 0xD4, 0xBF, 255),
};
ImU32 dmrColour(uint32_t src) { return kDmrColours[src % (sizeof(kDmrColours) / sizeof(kDmrColours[0]))]; }

ImU32 greyMix(ImU32 c, float k) {   // k = 1 -> full grey, 0 -> unchanged
    const int r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
    const int L = (r * 299 + g * 587 + b * 114) / 1000;
    return IM_COL32((int)(r + (L - r) * k), (int)(g + (L - g) * k), (int)(b + (L - b) * k), 255);
}

// the vehicle badge: a coloured circle with a white ring, a soft halo and a shadow, the id inside
// and a white arrow along the heading (grey and without an arrow when stale, desaturated when stopped)
void drawBadge(ImDrawList* dl, const ImVec2& q, uint32_t src, ImU32 col, double headingDeg, bool stale, bool stopped, bool sel) {
    char id[16];
    snprintf(id, sizeof id, "%u", src);
    const ImVec2 ts = ImGui::CalcTextSize(id);
    const float r = std::clamp(ts.x * 0.5f + 5.0f * gUi, 13.0f * gUi, 22.0f * gUi);
    const int a = stale ? 178 : 255;   // 70% opacity when stale
    ImU32 fill = stale ? greyMix(col, 1.0f) : stopped ? greyMix(col, 0.45f) : col;
    fill = (fill & 0x00FFFFFFu) | ((ImU32)a << 24);
    const ImU32 white = (IM_COL32(255, 255, 255, 235) & 0x00FFFFFFu) | ((ImU32)a << 24);
    dl->AddCircleFilled(ImVec2(q.x, q.y + 2.5f * gUi), r, IM_COL32(0, 0, 0, 90));                 // drop shadow
    dl->AddCircleFilled(q, r + 3.0f * gUi, (col & 0x00FFFFFFu) | 0x47000000u);                    // soft halo
    dl->AddCircleFilled(q, r, fill);
    dl->AddCircle(q, r, white, 0, 2.0f * gUi);                                                    // white ring
    if (sel) dl->AddCircle(q, r + 4.0f * gUi, white, 0, 1.5f * gUi);
    if (!stale && headingDeg >= 0) {
        const float h = (float)(headingDeg * M_PI / 180.0), sx = std::sin(h), sy = -std::cos(h);
        const ImVec2 apex(q.x + sx * (r + 6.0f * gUi), q.y + sy * (r + 6.0f * gUi));
        const ImVec2 b1(q.x + sx * (r + 0.5f * gUi) + std::cos(h) * 4.0f * gUi, q.y + sy * (r + 0.5f * gUi) + std::sin(h) * 4.0f * gUi);
        const ImVec2 b2(q.x + sx * (r + 0.5f * gUi) - std::cos(h) * 4.0f * gUi, q.y + sy * (r + 0.5f * gUi) - std::sin(h) * 4.0f * gUi);
        dl->AddTriangleFilled(apex, b1, b2, IM_COL32(255, 255, 255, a));
        dl->AddTriangle(apex, b1, b2, IM_COL32(0, 0, 0, 60), 1.0f * gUi);
    }
    dl->AddText(ImVec2(q.x - ts.x * 0.5f, q.y - ts.y * 0.5f), IM_COL32(3, 17, 29, a), id);
}

const DmrLocation* latestFix(const DmrTelemetry& t, uint32_t src) {
    for (size_t i = t.locations.size(); i-- > 0;)
        if (t.locations[i].src == src && t.locations[i].hasFix) return &t.locations[i];
    return nullptr;
}

const char* wallClock(double unixSec, char* b, size_t n) {
    const std::time_t tt = (std::time_t)unixSec;
    if (const std::tm* tm = std::localtime(&tt)) std::strftime(b, n, "%H:%M:%S", tm);
    else snprintf(b, n, "-");
    return b;
}

// a small tinted tag with coloured text; the cursor ends after it
void statusPill(const char* s, ImU32 rgb) {
    const ImVec2 ts = ImGui::CalcTextSize(s);
    const float px = 5.0f * gUi, py = 1.5f * gUi;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 mx(p.x + ts.x + px * 2.0f, p.y + ts.y + py * 2.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, mx, (rgb & 0x00FFFFFFu) | 0x26000000u, 4.0f * gUi);
    dl->AddText(ImVec2(p.x + px, p.y + py), rgb, s);
    ImGui::Dummy(ImVec2(mx.x - p.x, mx.y - p.y)); ImGui::SameLine(0.0f, 6.0f * gUi);
}

void cardRow(const char* k, const char* v) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", k);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(v);
}

} // namespace

void dmrMap(App& a) {
    load();
    const DmrTelemetry& t = a.rx.dmr;
    const bool on = live(a);
    const double now = (double)std::time(nullptr);
    filters();
    if (!on && t.locations.empty()) {
        ImGui::TextDisabled("%s", a.engine.running() ? "starting" : "start the receiver to see radios on the map");
        return;
    }
    const float W = ImGui::GetContentRegionAvail().x, H = ImGui::GetContentRegionAvail().y;
    const float dw = std::min(250.f * gUi, W * 0.28f);
    ImGui::BeginChild("##dmrmap", ImVec2(W - dw - 8 * gUi, H), false);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 size(ImGui::GetContentRegionAvail().x, std::max(120 * gUi, ImGui::GetContentRegionAvail().y));
    adsbmap::draw(S.map, size);
    const std::vector<dect2::MapMarker> mk = dmrMarkers(t, now);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(p0, ImVec2(p0.x + size.x, p0.y + size.y), true);
    struct Hit { ImVec2 p; const dect2::MapMarker* m; };
    std::vector<Hit> hits;
    hits.reserve(mk.size());
    for (const dect2::MapMarker& m : mk) {
        const uint32_t src = (uint32_t)std::stoul(m.id);
        const ImU32 col = dmrColour(src);
        const double kmh = m.hasSpeed ? m.speedKt * 1.852 : 0.0;
        const bool stale = m.ageSec > kStaleSec;
        const bool stopped = !stale && kmh < 1.0;
        const bool sel = S.selValid && src == S.selSrc;
        if (m.track.size() > 1) {
            const ImU32 trackCol = (col & 0x00FFFFFFu) | 0x8C000000u;   // the movement trail
            ImVec2 prev = adsbmap::project(m.track[0].first, m.track[0].second);
            for (size_t k = 1; k < m.track.size(); k++) {
                const ImVec2 q = adsbmap::project(m.track[k].first, m.track[k].second);
                if (std::fabs(m.track[k].second - m.track[k - 1].second) < 180) dl->AddLine(prev, q, trackCol, 2.5f * gUi);
                prev = q;
            }
        }
        const ImVec2 q = adsbmap::project(m.lat, m.lon);
        drawBadge(dl, q, src, col, m.headingDeg, stale, stopped, sel);
        hits.push_back({q, &m});
    }
    dl->PopClipRect();
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 8 * gUi, p0.y + 8 * gUi));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.07f, 0.08f, 0.85f));
    if (ImGui::Button("+", ImVec2(26 * gUi, 0))) S.map.zoom = std::min(adsbmap::kMaxZoom, S.map.zoom + 1);
    ImGui::SameLine(0, 2 * gUi);
    if (ImGui::Button("-", ImVec2(26 * gUi, 0))) S.map.zoom = std::max(adsbmap::kMinZoom, S.map.zoom - 1);
    ImGui::SameLine(0, 6 * gUi);
    if (ImGui::Button("me")) { double rl, ro; receiverLocation(rl, ro); S.map.lat = rl; S.map.lon = ro; }
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 6 * gUi);
    adsbmap::tileControls("never the radio positions");
    ImVec2 cp;
    if (adsbmap::clickedAt(cp) && cp.y > p0.y + 34.0f * gUi) {   // the top strip holds the buttons
        const dect2::MapMarker* best = nullptr;
        float bd = 24.0f * gUi;
        for (const Hit& h : hits) {
            const float d = std::hypotf(cp.x - h.p.x, cp.y - h.p.y);
            if (d < bd) { bd = d; best = h.m; }
        }
        if (best) { S.selSrc = (uint32_t)std::stoul(best->id); S.selValid = true; }
        else S.selValid = false;
    }
    ImGui::EndChild();
    if (S.selValid) {
        const DmrLocation* loc = latestFix(t, S.selSrc);
        if (loc && loc->hasFix) {
            const ImVec2 q = adsbmap::project(loc->lat, loc->lon);
            ImGui::SetNextWindowPos(ImVec2(q.x + 20 * gUi, q.y + 12 * gUi), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.94f);
            if (ImGui::Begin("##dmrcard", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
                const ImU32 col = dmrColour(loc->src);
                const ImVec2 hp = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(hp.x + 5.0f * gUi, hp.y + 8.0f * gUi), 4.5f * gUi, col);
                ImGui::SetCursorScreenPos(ImVec2(hp.x + 13.0f * gUi, hp.y + 2.0f * gUi));
                ImGui::TextColored(ImVec4(1, 1, 1, 1), "Src %u", loc->src);
                ImGui::SameLine(0, 8 * gUi);
                const double kmh = loc->hasSpeed ? loc->speedMs * 3.6 : 0.0;
                const bool stale = now - loc->unixSec > kStaleSec;
                const char* status = stale ? "STALE" : kmh < 1.0 ? "STOPPED" : "MOVING";
                statusPill(status, stale ? IM_COL32(240, 190, 60, 255) : kmh < 1.0 ? IM_COL32(150, 160, 175, 255) : IM_COL32(80, 200, 120, 255));
                ImGui::Separator();
                char b[64];
                if (ImGui::BeginTable("##dmrcardg", 2, ImGuiTableFlags_SizingFixedFit)) {
                    snprintf(b, sizeof b, "%.1f km/h", kmh);
                    cardRow("Speed", b);
                    snprintf(b, sizeof b, "%.0f deg", loc->hasHeading ? loc->headingDeg : 0.0);
                    cardRow("Course", loc->hasHeading ? b : "-");
                    snprintf(b, sizeof b, "%.5f", loc->lat);
                    cardRow("Latitude", b);
                    snprintf(b, sizeof b, "%.5f", loc->lon);
                    cardRow("Longitude", b);
                    cardRow("Updated", wallClock(loc->unixSec, b, sizeof b));
                    cardRow("Age", dect2::ageText(std::max(0.0, now - loc->unixSec)).c_str());
                    ImGui::EndTable();
                }
                ImGui::TextDisabled("TG %u, slot %d", loc->dst, loc->slot);
                ImGui::End();
            }
        }
    }
    ImGui::SameLine(0, 8 * gUi);
    ImGui::BeginChild("##dmrside", ImVec2(dw, H), false);
    ImGui::TextDisabled("radios %zu", mk.size());
    for (const dect2::MapMarker& m : mk) {
        char b[128];
        snprintf(b, sizeof b, "Src %s  %.0f km/h", m.id.c_str(), m.hasSpeed ? m.speedKt * 1.852 : 0.0);
        const bool sel = S.selValid && std::stoul(m.id) == S.selSrc;
        if (ImGui::Selectable(b, sel)) { S.selSrc = (uint32_t)std::stoul(m.id); S.selValid = true; S.map.lat = m.lat; S.map.lon = m.lon; S.map.zoom = std::max(S.map.zoom, 13); }
        ImGui::TextDisabled("%.5f, %.5f", m.lat, m.lon);
        if (!m.detail.empty()) ImGui::TextDisabled("%s", m.detail.c_str());
        ImGui::TextDisabled("last heard %s ago", dect2::ageText(m.ageSec).c_str());
        ImGui::Separator();
    }
    ImGui::EndChild();
}

void dmrTable(App& a) {
    const DmrTelemetry& t = a.rx.dmr;
    const double now = (double)std::time(nullptr);
    filters();
    ImGui::SameLine(0, 10 * gUi);
    if (ImGui::SmallButton("Export CSV")) {
        char nm[64];
        const std::time_t tnow = std::time(nullptr);
        std::strftime(nm, sizeof nm, "dmr_lrrp_%Y%m%d_%H%M%S.csv", std::localtime(&tnow));
        const std::string p = saveFileDialog(nm);
        if (!p.empty()) {
            std::string csv = "sec,src,tg,slot,lat,lon,speed_kph,heading_deg,age_s,type\n";
            for (const DmrLocation& loc : t.locations) {
                if (!pass(loc, now)) continue;
                char row[256];
                snprintf(row, sizeof row, "%.3f,%u,%u,%d,%.6f,%.6f,%.2f,%.0f,%.0f,%s\n",
                         loc.sec, loc.src, loc.dst, loc.slot, loc.lat, loc.lon,
                         loc.hasSpeed ? loc.speedMs * 3.6 : 0.0,
                         loc.hasHeading ? loc.headingDeg : -1.0,
                         std::max(0.0, now - loc.unixSec),
                         loc.hasFix ? "fix" : (loc.resultCode == 16 ? "nofix" : "status"));
                csv += row;
            }
            std::ofstream f(dect2::u8file(p), std::ios::binary);
            if (f) { f << csv; f.close(); S.msg = "saved " + p; } else S.msg = "could not write " + p;
        }
    }
    if (!S.msg.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%s", S.msg.c_str()); }
    if (!ImGui::BeginTable("##dmrloc", 10, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit, ImVec2(0, ImGui::GetContentRegionAvail().y))) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 48 * gUi);
    ImGui::TableSetupColumn("Src", ImGuiTableColumnFlags_WidthFixed, 60 * gUi);
    ImGui::TableSetupColumn("TG", ImGuiTableColumnFlags_WidthFixed, 60 * gUi);
    ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, 34 * gUi);
    ImGui::TableSetupColumn("Lat", ImGuiTableColumnFlags_WidthFixed, 90 * gUi);
    ImGui::TableSetupColumn("Lon", ImGuiTableColumnFlags_WidthFixed, 90 * gUi);
    ImGui::TableSetupColumn("km/h", ImGuiTableColumnFlags_WidthFixed, 54 * gUi);
    ImGui::TableSetupColumn("Heading", ImGuiTableColumnFlags_WidthFixed, 60 * gUi);
    ImGui::TableSetupColumn("Age", ImGuiTableColumnFlags_WidthFixed, 60 * gUi);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 60 * gUi);
    ImGui::TableHeadersRow();
    for (size_t i = t.locations.size(); i-- > 0;) {
        const DmrLocation& loc = t.locations[i];
        if (!pass(loc, now)) continue;
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("%d:%02d", (int)loc.sec / 60, (int)loc.sec % 60);
        ImGui::TableNextColumn(); ImGui::Text("%u", loc.src);
        ImGui::TableNextColumn(); ImGui::Text("%u", loc.dst);
        ImGui::TableNextColumn(); if (loc.slot) ImGui::Text("%d", loc.slot); else ImGui::TextUnformatted("-");
        if (loc.hasFix) {
            ImGui::TableNextColumn(); ImGui::Text("%.5f", loc.lat);
            ImGui::TableNextColumn(); ImGui::Text("%.5f", loc.lon);
            ImGui::TableNextColumn(); ImGui::Text("%.1f", loc.hasSpeed ? loc.speedMs * 3.6 : 0.0);
            ImGui::TableNextColumn(); if (loc.hasHeading) ImGui::Text("%.0f", loc.headingDeg); else ImGui::TextUnformatted("-");
        } else {
            for (int c = 0; c < 4; c++) { ImGui::TableNextColumn(); ImGui::TextUnformatted("-"); }
        }
        ImGui::TableNextColumn(); ImGui::TextUnformatted(dect2::ageText(std::max(0.0, now - loc.unixSec)).c_str());
        ImGui::TableNextColumn(); ImGui::TextUnformatted(loc.hasFix ? "fix" : (loc.resultCode == 16 ? "no fix" : "status"));
    }
    ImGui::EndTable();
}
