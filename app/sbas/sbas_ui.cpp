// SBAS screens: the satellites that are tracked with the message types each has sent, the message viewer, the fast corrections of a satellite with their UDRE,
// the ionospheric grid summary, and the GEO navigation message, time offset and almanac. Receive only: the corrections are shown, never applied to a position.
#include "app.h"
#include "dect2/gnss/gnss_tel.h"
#include "dect2/sbas/sbas_engine.h"
#include "dect2/sbas/sbas_msg.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace {

struct State {
    int view = 0;                 // 0 satellites, 1 messages, 2 fast corrections, 3 ionosphere, 4 navigation
    int selPrn = 0;               // the satellite the detail views show (0: the first)
    bool onlyFast = false;
};
State S;

bool live(const App& a) { return a.rx.standard == 151; }

void kvRow(const App& a, const char* k, const char* fmt, ...) {
    ImGui::TextDisabled("%s", k); kvColumn(150 * gUi);
    char b[512];
    va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    ImGui::PushFont(a.mono, 0); ImGui::PushTextWrapPos(0); ImGui::TextUnformatted(b); ImGui::PopTextWrapPos(); ImGui::PopFont();
}

const char* serviceOf(int prn) {
    const char* s = dect2::sbasServiceOfPrn(prn);
    return *s ? s : "-";
}

const dect2::sbas::GeoState* selected(const dect2::SbasTelemetry& t) {
    const dect2::sbas::GeoState* first = nullptr;
    for (const auto& g : t.geos) {
        if (!first) first = &g;
        if (g.prn == S.selPrn) return &g;
    }
    return first;
}

void satPicker(const dect2::SbasTelemetry& t) {
    const dect2::sbas::GeoState* g = selected(t);
    char cur[48] = "none";
    if (g) snprintf(cur, sizeof cur, "PRN %d  %s", g->prn, serviceOf(g->prn));
    ImGui::TextDisabled("satellite"); ImGui::SameLine(0, 6 * gUi);
    ImGui::SetNextItemWidth(220 * gUi);
    if (ImGui::BeginCombo("##sbsat", cur)) {
        for (const auto& x : t.geos) {
            char b[64]; snprintf(b, sizeof b, "PRN %d  %s  (%u messages)", x.prn, serviceOf(x.prn), x.messages);
            if (ImGui::Selectable(b, g && g->prn == x.prn)) S.selPrn = x.prn;
        }
        ImGui::EndCombo();
    }
}

void notLive(const App& a) { ImGui::TextDisabled("%s", a.engine.running() ? "starting" : "stopped"); }

// ---- satellites
void satellitesView(App& a, const dect2::SbasTelemetry& t) {
    if (t.sats.empty()) {
        ImGui::TextDisabled("%s", t.searching ? "searching the SBAS PRNs 120 to 158 (a satellite in view of your sky is found within a minute or two; no GPS lock is needed)"
                                              : "waiting for the first search round");
        if (t.searching) ImGui::TextDisabled("PRN %d, %.0f %%, round %u", t.searchPrn, t.searchProgress * 100.f, t.searchRounds);
    }
    if (ImGui::BeginTable("##sbsats", 8, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("PRN"); ImGui::TableSetupColumn("Service"); ImGui::TableSetupColumn("State"); ImGui::TableSetupColumn("C/N0");
        ImGui::TableSetupColumn("Doppler"); ImGui::TableSetupColumn("Messages"); ImGui::TableSetupColumn("Missed"); ImGui::TableSetupColumn("Message types seen (0 to 63)", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (const auto& s : t.sats) {
            const dect2::sbas::GeoState* g = nullptr;
            for (const auto& x : t.geos) if (x.prn == s.prn) g = &x;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(std::to_string(s.prn).c_str(), S.selPrn == s.prn, ImGuiSelectableFlags_SpanAllColumns)) S.selPrn = s.prn;
            ImGui::TableNextColumn(); ImGui::TextUnformatted(serviceOf(s.prn));
            ImGui::TableNextColumn(); ImGui::TextUnformatted(dect2::gnssChStateName(s.state));
            ImGui::TableNextColumn(); ImGui::Text("%.1f dB-Hz", s.cn0);
            ImGui::TableNextColumn(); ImGui::Text("%+.0f Hz", s.dopplerHz);
            ImGui::TableNextColumn(); ImGui::Text("%u", s.messages);
            ImGui::TableNextColumn(); ImGui::Text("%u", s.framesBad);
            ImGui::TableNextColumn();
            const ImVec2 p0 = ImGui::GetCursorScreenPos();
            const float cw = 7.f * gUi, ch = 12.f * gUi;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            for (int ty = 0; ty < 64; ty++) {
                const bool seen = g && ((g->typesSeen >> ty) & 1);
                const ImVec2 a0(p0.x + ty * cw, p0.y), a1(p0.x + ty * cw + cw - 1, p0.y + ch);
                dl->AddRectFilled(a0, a1, seen ? IM_COL32(110, 200, 130, 255) : IM_COL32(70, 74, 84, 255));
            }
            ImGui::Dummy(ImVec2(64 * cw, ch));
            if (ImGui::IsItemHovered() && g) {
                const ImVec2 m = ImGui::GetMousePos();
                const int ty = (int)((m.x - p0.x) / cw);
                if (ty >= 0 && ty < 64) ImGui::SetTooltip("type %d: %s%s", ty, dect2::sbas::typeName(ty), ((g->typesSeen >> ty) & 1) ? "" : " (not received)");
            }
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("SBAS satellites are geostationary: they sit at a fixed place in the sky, so their Doppler is small. Which services you can receive depends on where you are "
                        "(WAAS over North America, EGNOS over Europe and North Africa, MSAS over Japan, GAGAN over India, SDCM over Russia, BDSBAS over China). "
                        "The service shown is the usual one for that PRN and is informal; the satellite's own messages are the truth. QZSS L1 C/A satellites are in the GNSS mode.");
    ImGui::PopTextWrapPos();
    (void)a;
}

// ---- messages
void messagesView(App& a, const dect2::SbasTelemetry& t) {
    if (ImGui::BeginTable("##sbtypes", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV, ImVec2(std::min(ImGui::GetContentRegionAvail().x, 520.f * gUi), 150 * gUi))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Type"); ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch); ImGui::TableSetupColumn("Count");
        ImGui::TableHeadersRow();
        for (int ty = 0; ty < 64; ty++) {
            if (!t.typeCount[ty]) continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%d", ty);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(dect2::sbas::typeName(ty));
            ImGui::TableNextColumn(); ImGui::Text("%llu", (unsigned long long)t.typeCount[ty]);
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
    if (ImGui::BeginTable("##sblog", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Signal time"); ImGui::TableSetupColumn("PRN"); ImGui::TableSetupColumn("Type"); ImGui::TableSetupColumn("What it says", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (auto it = t.log.rbegin(); it != t.log.rend(); ++it) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%.0f s", it->timeS);
            ImGui::TableNextColumn(); ImGui::Text("%d", it->prn);
            ImGui::TableNextColumn(); ImGui::Text("%d", it->type);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(it->text.c_str());
        }
        ImGui::EndTable();
    }
    if (t.log.empty()) ImGui::TextDisabled("no message yet: a message arrives every second once a satellite is locked");
    (void)a;
}

// ---- fast corrections
void fastView(App& a, const dect2::SbasTelemetry& t) {
    satPicker(t);
    const dect2::sbas::GeoState* g = selected(t);
    if (!g) { ImGui::TextDisabled("no message yet"); return; }
    ImGui::SameLine(0, 12 * gUi);
    ImGui::Checkbox("only slots with a correction", &S.onlyFast);
    const dect2::sbas::Summary sm = dect2::sbas::Network::summarise(*g);
    ImGui::Text("PRN mask: %s", g->maskValid ? "received" : "not received yet (type 1)");
    if (g->maskValid) { ImGui::SameLine(); ImGui::TextDisabled("%d satellites, IODP %d", g->nMasked, g->iodp); }
    ImGui::SameLine(0, 12 * gUi);
    ImGui::TextDisabled("%d corrections, %d usable, %d not monitored, %d do not use", sm.nFast, sm.nUsable, sm.nNotMonitored, sm.nDoNotUse);
    if (g->degValid) { ImGui::SameLine(0, 12 * gUi); ImGui::TextDisabled("system latency %d s", g->tLat); }
    if (ImGui::BeginTable("##sbfast", 8, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Slot"); ImGui::TableSetupColumn("Satellite"); ImGui::TableSetupColumn("Correction");
        ImGui::TableSetupColumn("UDRE"); ImGui::TableSetupColumn("UDRE index"); ImGui::TableSetupColumn("IODF"); ImGui::TableSetupColumn("IODP"); ImGui::TableSetupColumn("Degradation a");
        ImGui::TableHeadersRow();
        for (int i = 0; i < dect2::sbas::kMaxSlots; i++) {
            const dect2::sbas::FastCorr& f = g->fast[(size_t)i];
            if (S.onlyFast && !f.valid) continue;
            if (g->maskValid && i >= g->nMasked && !f.valid) continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%d", i + 1);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(dect2::sbas::Network::slotName(*g, i).c_str());
            ImGui::TableNextColumn();
            if (f.valid) ImGui::Text("%+.3f m", f.prcM); else ImGui::TextDisabled("-");
            ImGui::TableNextColumn();
            if (f.valid) ImGui::TextUnformatted(dect2::sbas::udreText(f.udrei)); else ImGui::TextDisabled("-");
            ImGui::TableNextColumn(); if (f.valid) ImGui::Text("%d", f.udrei); else ImGui::TextDisabled("-");
            ImGui::TableNextColumn(); if (f.valid) ImGui::Text("%d", f.iodf); else ImGui::TextDisabled("-");
            ImGui::TableNextColumn(); if (f.valid) ImGui::Text("%d%s", f.iodp, g->maskValid && f.iodp != g->iodp ? " (not the mask's)" : ""); else ImGui::TextDisabled("-");
            ImGui::TableNextColumn();
            if (g->degValid) ImGui::Text("%.5f m/s2", dect2::sbas::degradationA(g->aiDeg[(size_t)i])); else ImGui::TextDisabled("-");
        }
        ImGui::EndTable();
    }
    (void)a;
}

// ---- ionosphere
void ionoView(App& a, const dect2::SbasTelemetry& t) {
    satPicker(t);
    const dect2::sbas::GeoState* g = selected(t);
    if (!g) { ImGui::TextDisabled("no message yet"); return; }
    ImGui::SameLine(0, 12 * gUi);
    ImGui::TextDisabled("IODI %d", g->ionoIodi);
    if (ImGui::BeginTable("##sbiono", 9, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Band"); ImGui::TableSetupColumn("Grid points"); ImGui::TableSetupColumn("Blocks"); ImGui::TableSetupColumn("With a delay");
        ImGui::TableSetupColumn("Not monitored"); ImGui::TableSetupColumn("Smallest"); ImGui::TableSetupColumn("Mean"); ImGui::TableSetupColumn("Largest"); ImGui::TableSetupColumn("Worst GIVE index");
        ImGui::TableHeadersRow();
        for (int b = 0; b < dect2::sbas::kIonoBands; b++) {
            if (!g->iono[(size_t)b].maskValid) continue;
            const dect2::sbas::IonoSummary s = dect2::sbas::Network::ionoSummary(*g, b);
            int seen = 0;
            for (int k = 0; k < 14; k++) seen += (s.blocksSeen >> k) & 1;
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%d", b);
            ImGui::TableNextColumn(); ImGui::Text("%d", s.nIgp);
            ImGui::TableNextColumn(); ImGui::Text("%d of %d", seen, s.blocksNeeded);
            ImGui::TableNextColumn(); ImGui::Text("%d", s.nValue);
            ImGui::TableNextColumn(); ImGui::Text("%d", s.nNotMonitored);
            ImGui::TableNextColumn(); if (s.nValue) ImGui::Text("%.3f m", s.minM); else ImGui::TextDisabled("-");
            ImGui::TableNextColumn(); if (s.nValue) ImGui::Text("%.3f m", s.meanM); else ImGui::TextDisabled("-");
            ImGui::TableNextColumn(); if (s.nValue) ImGui::Text("%.3f m", s.maxM); else ImGui::TextDisabled("-");
            ImGui::TableNextColumn(); if (s.nValue) ImGui::Text("%d", s.worstGivei); else ImGui::TextDisabled("-");
        }
        ImGui::EndTable();
    }
    bool any = false;
    for (const auto& b : g->iono) any |= b.maskValid;
    if (!any) ImGui::TextDisabled("no grid mask yet (type 18); the delays (type 26) follow it");
    ImGui::Spacing();
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("The grid has up to 201 points in each of bands 0 to 10; type 18 says which points are used and type 26 gives the vertical delay of 15 points a message. "
                        "The summary counts the points by their place in the mask; the latitude and longitude of a point come from a table of the SBAS description that is not in this screen.");
    ImGui::PopTextWrapPos();
    (void)a;
}

// ---- navigation, time, almanac
void navView(App& a, const dect2::SbasTelemetry& t) {
    satPicker(t);
    const dect2::sbas::GeoState* g = selected(t);
    if (!g) { ImGui::TextDisabled("no message yet"); return; }
    ImGui::Spacing();
    ImGui::TextUnformatted("GEO navigation message (type 9)");
    if (g->nav.valid) {
        const auto& n = g->nav;
        const double r = std::sqrt(n.pos[0] * n.pos[0] + n.pos[1] * n.pos[1] + n.pos[2] * n.pos[2]);
        const double lon = std::atan2(n.pos[1], n.pos[0]) * 180.0 / 3.14159265358979;
        kvRow(a, "position (ECEF)", "%.1f  %.1f  %.1f m", n.pos[0], n.pos[1], n.pos[2]);
        kvRow(a, "radius, longitude", "%.1f km, %.2f deg (the satellite's place over the equator)", r / 1000.0, lon);
        kvRow(a, "velocity", "%.4f  %.4f  %.4f m/s", n.vel[0], n.vel[1], n.vel[2]);
        kvRow(a, "acceleration", "%.7f  %.7f  %.7f m/s2", n.acc[0], n.acc[1], n.acc[2]);
        kvRow(a, "clock", "af0 %.3e s, af1 %.3e s/s", n.af0, n.af1);
        kvRow(a, "reference", "t0 %.0f s of the day, IODN %d, URA index %d", n.t0S, n.iodn, n.ura);
    } else ImGui::TextDisabled("not received yet");
    ImGui::Spacing();
    ImGui::TextUnformatted("Time offset (type 12)");
    if (g->utc.valid) {
        const auto& u = g->utc;
        kvRow(a, "GPS time", "week %d (10 bit), time of week %d s", u.wn, u.tow);
        kvRow(a, "leap seconds", "%d, next %d at week %d day %d", u.dtLS, u.dtLSF, u.wnLSF, u.dn);
        kvRow(a, "UTC offset", "A0 %.3e s, A1 %.3e s/s, reference time %d s, week %d", u.a0, u.a1, u.tUtc, u.wnT);
        kvRow(a, "UTC standard", "identifier %d%s", u.utcId, u.glonass ? ", GLONASS offset flagged" : "");
    } else ImGui::TextDisabled("not received yet");
    ImGui::Spacing();
    ImGui::TextUnformatted("GEO almanac (type 17)");
    if (!g->almanac.empty()) {
        for (const auto& e : g->almanac) kvRow(a, ("PRN " + std::to_string(e.prn)).c_str(), "health 0x%02X, position %.0f  %.0f  %.0f m", e.health, e.pos[0], e.pos[1], e.pos[2]);
    } else ImGui::TextDisabled("not received yet");
    ImGui::Spacing();
    ImGui::TextUnformatted("Degradation parameters (type 10, the raw fields)");
    if (g->deg.valid) {
        const auto& d = g->deg;
        kvRow(a, "fast / long term", "Brrc %d, Cltc_lsb %d, Cltc_v1 %d, Iltc_v1 %d, Cltc_v0 %d, Iltc_v0 %d", d.brrc, d.cltcLsb, d.cltcV1, d.iltcV1, d.cltcV0, d.iltcV0);
        kvRow(a, "GEO / ionosphere", "Cgeo_lsb %d, Cgeo_v %d, Igeo %d, Cer %d, Ciono_step %d, Iiono %d, Ciono_ramp %d, RSS %d / %d", d.cgeoLsb, d.cgeoV, d.igeo, d.cer, d.cionoStep, d.iiono, d.cionoRamp, d.rssUdre, d.rssIono);
    } else ImGui::TextDisabled("not received yet");
    ImGui::Spacing();
    ImGui::TextUnformatted("Long-term corrections (types 24 and 25)");
    if (!g->longTerm.empty()) {
        for (const auto& l : g->longTerm)
            kvRow(a, dect2::sbas::Network::slotName(*g, l.slot - 1).c_str(), "IODE %d, dx %.3f dy %.3f dz %.3f m, af0 %.3e s%s", l.iode, l.dpos[0], l.dpos[1], l.dpos[2], l.daf0, l.velocity ? " (with velocity terms)" : "");
    } else ImGui::TextDisabled("none yet");
}

void tab(App& a) {
    subNav("sbv", S.view, {"Satellites", "Messages", "Fast corrections", "Ionosphere", "Navigation"});
    if (!live(a)) { notLive(a); return; }
    const dect2::SbasTelemetry& t = a.rx.sbas;
    ImGui::SameLine(0, 12 * gUi);
    ImGui::TextDisabled("%s", dect2::sbasSummary(t).c_str());
    switch (S.view) {
    case 0: satellitesView(a, t); break;
    case 1: messagesView(a, t); break;
    case 2: fastView(a, t); break;
    case 3: ionoView(a, t); break;
    default: navView(a, t); break;
    }
}

void list(App& a) {
    if (!live(a)) { notLive(a); return; }
    const dect2::SbasTelemetry& t = a.rx.sbas;
    if (ImGui::BeginTable("##sbl", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("PRN"); ImGui::TableSetupColumn("Service", ImGuiTableColumnFlags_WidthStretch); ImGui::TableSetupColumn("C/N0"); ImGui::TableSetupColumn("Msgs");
        for (const auto& s : t.sats) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(std::to_string(s.prn).c_str(), S.selPrn == s.prn, ImGuiSelectableFlags_SpanAllColumns)) S.selPrn = s.prn;
            ImGui::TableNextColumn(); ImGui::TextUnformatted(serviceOf(s.prn));
            ImGui::TableNextColumn(); ImGui::Text("%.0f", s.cn0);
            ImGui::TableNextColumn(); ImGui::Text("%u", s.messages);
        }
        ImGui::EndTable();
    }
    if (t.sats.empty()) ImGui::TextDisabled("no satellite yet");
}

void receiver(App& a) {
    if (!live(a)) { notLive(a); return; }
    const dect2::SbasTelemetry& t = a.rx.sbas;
    kvRow(a, "state", "%s", dect2::sbasSummary(t).c_str());
    kvRow(a, "messages", "%llu with a good CRC, %llu missed", (unsigned long long)t.total, (unsigned long long)t.blocksBad);
    kvRow(a, "input", "%.3f Msps, centre %.3f MHz, level %.1f dBFS", t.inputRate / 1e6, t.centerMhz, t.levelDbfs);
    kvRow(a, "frequency error", "%+.0f Hz (the radio's)", t.cfoHz);
    kvRow(a, "signal time", "%.0f s", t.signalSecs);
    if (t.searching) kvRow(a, "search", "PRN %d, %.0f %% of round %u", t.searchPrn, t.searchProgress * 100.f, t.searchRounds);
    if (!t.status.empty()) kvRow(a, "status", "%s", t.status.c_str());
    ImGui::Spacing();
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("SBAS L1 is the 1575.42 MHz signal of the geostationary augmentation satellites (the C/A code of the GPS family, 250 bit/s messages with a rate 1/2 code and a 24 bit CRC). "
                        "This mode searches PRNs 120 to 158 on its own, with no GPS lock first. It needs an active antenna with a clear view of the sky to the south (in Europe) "
                        "and a radio that can reach 2.046 Msps or more. The corrections are shown, not applied. QZSS L1S and L6 are not decoded.");
    ImGui::PopTextWrapPos();
}

void decoder(App& a, bool&) {
    if (ImGui::SmallButton("Clear decoded messages")) a.engine.sbas().clearMessages();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Forget the PRN masks, corrections and logs; the satellites stay tracked and fill the tables again.");
}

void synth(App& a, bool&) {
    (void)a;
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("simulated sky of the GNSS test signal with its three geostationary SBAS satellites (PRN 123, 127 and 128). The messages carry made-up but self-consistent content: a PRN mask of 14 GPS satellites and the SBAS satellite, fast corrections, ionospheric grid points, a GEO navigation message, a time offset and a GEO almanac.");
    ImGui::PopTextWrapPos();
}

void status(App& a) {
    const bool run = a.engine.running(), on = live(a);
    const SignalStats& st = a.spec.stats;
    const AdcStatus adc = classifyAdc(st.rmsDbfs, st.peak, st.clipFraction);
    lamp("IQ", run ? (adc == AdcStatus::Overload ? 3 : (adc == AdcStatus::Good ? 1 : 2)) : 0, (int)Ic::Wave); flowNext(12 * gUi);
    lamp("Satellite", !on ? 0 : a.rx.sbas.sats.empty() ? 0 : 1); flowNext(10 * gUi);
    lamp("Messages", !on ? 0 : a.rx.sbas.total > 0 ? 1 : 0); flowNext(10 * gUi);
    ImGui::TextDisabled("|"); flowNext(10 * gUi);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("State"); ImGui::SameLine(0, 5 * gUi);
    ImGui::PushFont(a.mono, 0); ImGui::TextUnformatted(!on ? (run ? "starting" : "stopped") : dect2::sbasSummary(a.rx.sbas).c_str()); ImGui::PopFont();
}

void summary(const App& a, std::string& l1, std::string& l2) {
    l1 = "SBAS";
    if (live(a)) l2 = dect2::sbasSummary(a.rx.sbas);
}

}  // namespace

extern const ModeUi kSbasUi;
const ModeUi kSbasUi = {
    .sideTitle = "SATELLITES",
    .tabName = "SBAS",
    .tabIcon = Ic::Signal,
    .tab = tab,
    .receiver = receiver,
    .list = list,
    .status = status,
    .summary = summary,
    .decoder = decoder,
    .synth = synth,
};
