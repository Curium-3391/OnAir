// Tools > Propagation, Advanced mode: every input of the model (transmitter, receiver chain and SDR, path, ionosphere, atmosphere, terrain),
// the link budget term by term with an explanation on each, sweeps (signal vs distance, range vs power and frequency, the HF MUF over the
// day), CSV export and a second scenario to compare. See prop_ui.cpp for the window, the worker and the map.
#include "prop_internal.h"
#include "../gfx/plot.h"
#include "dect2/common/u8file.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace propui {

namespace {

const char* kPol = "vertical\0horizontal\0circular (right)\0circular (left)\0";

void section(const char* title, bool open, void (*body)(State&, pr::Scenario&)) {
    if (ImGui::CollapsingHeader(title, open ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
        ImGui::PushID(title);
        body(st(), cur());
        ImGui::PopID();
        ImGui::Spacing();
    }
}

void txSection(State& S, pr::Scenario& s) {
    placeRowUi();
    S.labelCol = 150 * gUi;
    const auto& ps = pr::presets();
    ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("Preset"); ImGui::SameLine(S.labelCol);
    ImGui::SetNextItemWidth(190 * gUi);
    if (ImGui::BeginCombo("##advpreset", ps[s.preset].name)) {
        for (int i = 0; i < (int)ps.size(); i++)
            if (ImGui::Selectable(ps[i].name, i == s.preset)) pr::applyPreset(s, i);
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("%s", ps[s.preset].note);
    numField("Power", &s.power, nullptr, "Power into the antenna cable.", 80 * gUi);
    ImGui::SameLine();
    int u = (int)s.powerUnit;
    ImGui::SetNextItemWidth(60 * gUi);
    if (ImGui::Combo("##advpu", &u, "W\0dBW\0dBm\0kW\0")) s.powerUnit = (pr::PowerUnit)u;
    numField("Antenna gain", &s.txGainDbi, "dBi", "Gain toward the horizon over an isotropic antenna: dipole 2.15, 5/8 vertical about 3, small yagi 7 to 10. "
             "Ignored when a pattern file is loaded below.");
    numField("Feeder loss", &s.txFeederDb, "dB", "Cable and connectors between the transmitter and the antenna (e.g. 20 m of RG-58 at 145 MHz: about 4 dB).");
    numField("Height above ground", &s.txHeightM, "m", "Antenna height above the ground under it (with terrain: above the terrain at the transmitter).");
    int pol = (int)s.txPol;
    enumCombo("Polarisation", &pol, kPol, "Vertical for most mobile and FM, horizontal for TV and HF dipoles. A mismatch with the receive antenna costs 3 to 20 dB.");
    s.txPol = (pr::Pol)pol;
    const int i = S.advanced && S.compare ? S.editing : 0;
    ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("Pattern file"); ImGui::SameLine(S.labelCol);
    if (ImGui::SmallButton("Load...")) {
        const std::string f = openFileDialog();
        if (!f.empty()) { s.txPatternPath = f; loadPatternFor(i); }
    }
    ImGui::SetItemTooltip("%s", "An antenna pattern (CSV lines \"azimuth_deg,gain_dbi\", optional \"el,elevation_deg,relative_db\"), e.g. exported by the antenna designer. "
                                "It replaces the gain above, by bearing from the boresight.");
    if (!s.txPatternPath.empty()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear##pat")) { s.txPatternPath.clear(); loadPatternFor(i); }
        if (S.pattern[i]) {
            ImGui::SameLine(); ImGui::TextDisabled("%s", S.pattern[i]->name.c_str());
            numField("Boresight azimuth", &s.txAzDeg, "deg", "Where the pattern's 0 degrees points (0 north, 90 east).");
        } else ImGui::TextColored(pal::warnAmber(), "%s", S.patternMsg[i].c_str());
    }
    const double eirp = pr::powerDbw(s.power, s.powerUnit) + s.txGainDbi - s.txFeederDb;
    ImGui::TextDisabled("EIRP %.1f dBW (%.1f dBm, %.3g W); ERP %.3g W", eirp, eirp + 30, std::pow(10.0, eirp / 10), std::pow(10.0, (eirp - 2.15) / 10));
}

void signalSection(State& S, pr::Scenario& s) {
    S.labelCol = 150 * gUi;
    numField("Frequency", &s.fMHz, "MHz", "Below 30 MHz the HF model (ground wave + skywave), from 30 MHz the VHF/UHF model.");
    numField("Bandwidth", &s.bandwidthHz, "Hz", "The receiver's noise bandwidth: 2400 SSB, 12500 narrow FM, 200000 broadcast FM, 125000 LoRa, 7.6e6 DVB-T.", 100 * gUi, "%.6g");
    numField("Required SNR", &s.requiredSnrDb, "dB", "SNR in that bandwidth at which the mode just works: narrow FM 12, SSB 10, DAB 10, DVB-T 64QAM about 20, FT8 -21, LoRa SF12 -20.");
}

void rxSection(State& S, pr::Scenario& s) {
    S.labelCol = 150 * gUi;
    numField("Antenna gain", &s.rxGainDbi, "dBi", "Receive antenna gain toward the transmitter (a rubber duck is about -5 to 0, a discone 0 to 2, a yagi 7 to 15).");
    numField("Height above ground", &s.rxHeightM, "m", "A handheld is about 1.5 m, a car 1.5 to 2 m, a roof aerial 10 m.");
    numField("Feeder loss", &s.rxFeederDb, "dB", "Cable loss between the antenna and the radio (or the preamplifier and the radio). It adds to the noise figure.");
    int pol = (int)s.rxPol;
    enumCombo("Polarisation", &pol, kPol, "The receive antenna's polarisation.");
    s.rxPol = (pr::Pol)pol;
    const auto& rs = pr::radios();
    ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("Radio"); ImGui::SameLine(S.labelCol);
    ImGui::SetNextItemWidth(200 * gUi);
    if (ImGui::BeginCombo("##advradio", rs[s.radio].name)) {
        for (int i = 0; i < (int)rs.size(); i++)
            if (ImGui::Selectable(rs[i].name, i == s.radio)) s.radio = i;
        ImGui::EndCombo();
    }
    if (*rs[s.radio].note) ImGui::SetItemTooltip("%s", rs[s.radio].note);
    if (s.radio == 0) numField("Noise figure", &s.rxNfDb, "dB", "The receiver's own noise figure (an SDR 4 to 8, a good preamplifier 0.5 to 1).");
    else {
        ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("Gain setting"); ImGui::SameLine(S.labelCol);
        ImGui::SetNextItemWidth(140 * gUi);
        float g = (float)s.radioGainPct;
        if (ImGui::SliderFloat("##advgain", &g, 0, 100, "%.0f %%")) s.radioGainPct = g;
        ImGui::SetItemTooltip("%s", "The radio's gain, as a share of its range. Low gain costs noise figure (roughly +16 dB at 0 %); too much overloads on strong signals.");
        ImGui::SameLine();
        ImGui::TextDisabled("NF %.1f dB", pr::radioNfDb(s, s.fMHz));
    }
    ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("Preamplifier"); ImGui::SameLine(S.labelCol);
    ImGui::Checkbox("at the antenna##advpre", &s.preamp);
    ImGui::SetItemTooltip("%s", "A low-noise amplifier at the antenna, before the feeder: it hides the feeder loss and the radio's noise figure (Friis).");
    if (s.preamp) {
        numField("  gain", &s.preampGainDb, "dB", "Preamplifier gain (typically 15 to 25 dB).", 70 * gUi);
        numField("  noise figure", &s.preampNfDb, "dB", "Preamplifier noise figure (typically 0.5 to 1.5 dB).", 70 * gUi);
    }
    const double nf = pr::cascadeNfDb(s.rxFeederDb, pr::radioNfDb(s, s.fMHz), s.preamp, s.preampGainDb, s.preampNfDb);
    const double floorR = pr::ktbDbm(std::max(s.bandwidthHz, 1.0)) + nf;
    ImGui::TextDisabled("Chain noise figure %.1f dB. Radio alone: noise %.1f dBm, sensitivity %.1f dBm (%.3g uV) for %.0f dB SNR.", nf, floorR,
                        floorR + s.requiredSnrDb, pr::dbmToUv50(floorR + s.requiredSnrDb), s.requiredSnrDb);
    helpMark("Sensitivity = kTB + noise figure + required SNR, before the outside noise. The link budget adds the man-made, galactic and atmospheric noise at the antenna.");
}

void pathSection(State& S, pr::Scenario& s) {
    S.labelCol = 150 * gUi;
    int pm = (int)s.pathModel, hm = (int)s.hfModel, gr = (int)s.ground, cl = (int)s.rxClutter, en = (int)s.env;
    if (s.fMHz >= 30)
        enumCombo("Path model", &pm, "auto (recommended)\0free space only\0smooth earth only\0two-ray exact\0",
                  "Auto: the larger of P.526 smooth-earth diffraction and the plane-earth ground reflection, plus troposcatter. Free space: the upper bound. "
                  "Smooth earth: P.526 alone (no ground reflection). Two-ray exact: with the reflection's peaks and nulls (one per wavelength of path difference).");
    else
        enumCombo("HF modes", &hm, "ground wave + skywave\0ground wave only\0skywave only\0", "Which HF propagation modes are added up.");
    enumCombo("Ground", &gr, "sea\0wet ground\0average land\0dry ground / city\0",
              "Ground constants (sea 5 S/m, wet 10 mS/m, average 5 mS/m, dry 1 mS/m): they decide the HF ground wave and, a little, VHF diffraction.");
    enumCombo("Around the receiver", &cl, "open\0suburban\0urban\0dense urban\0forest\0",
              "ITU-R P.2108 clutter class: the loss of a receive antenna below the roofs or trees around it (none when it is above them: 10 to 20 m).");
    enumCombo("Noise there", &en, "quiet rural\0rural\0residential\0city\0",
              "ITU-R P.372 man-made noise. It dominates HF and low VHF: a city is about 20 dB noisier than quiet countryside.");
    s.pathModel = (pr::PathModel)pm; s.hfModel = (pr::HfModel)hm; s.ground = (pr::Ground)gr; s.rxClutter = (pr::Clutter)cl; s.env = (pr::Env)en;
    ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("Atmospheric noise"); ImGui::SameLine(S.labelCol);
    ImGui::Checkbox("lightning (HF)##advatm", &s.atmosphericNoise);
    ImGui::SetItemTooltip("%s", "Noise from thunderstorms, carried by the ionosphere: strong at night on the low HF bands, in summer and in the tropics (rough P.372 fit).");
}

void ionoSection(State& S, pr::Scenario& s) {
    S.labelCol = 150 * gUi;
    if (s.fMHz >= 30) ImGui::TextDisabled("Used below 30 MHz (and for the atmospheric noise).");
    numField("Solar flux (SFI)", &s.sfi, "sfu", "The 10.7 cm solar flux index, 65 (quiet sun) to 300 (very active). Type today's value from any space-weather source: nothing is downloaded.");
    numField("K index", &s.kIndex, "", "Planetary K index 0 to 9: 5 and up is a storm (lower MUF, more absorption at high latitudes).", 60 * gUi);
    ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("Date and time"); ImGui::SameLine(S.labelCol);
    ImGui::Checkbox("now (UTC)##advnow", &s.useNow);
    if (s.useNow) { ImGui::SameLine(); ImGui::TextDisabled("%04d-%02d-%02d %02d:%02d", s.year, s.month, s.day, s.hour, s.minute); }
    else {
        ImGui::SetCursorPosX(S.labelCol);
        ImGui::SetNextItemWidth(56 * gUi); ImGui::InputInt("##advy", &s.year, 0, 0); ImGui::SameLine();
        ImGui::SetNextItemWidth(34 * gUi); ImGui::InputInt("##advmo", &s.month, 0, 0); ImGui::SameLine();
        ImGui::SetNextItemWidth(34 * gUi); ImGui::InputInt("##advd", &s.day, 0, 0); ImGui::SameLine();
        ImGui::SetNextItemWidth(34 * gUi); ImGui::InputInt("##advh", &s.hour, 0, 0); ImGui::SameLine();
        ImGui::SetNextItemWidth(34 * gUi); ImGui::InputInt("##advmi", &s.minute, 0, 0);
        ImGui::SameLine(); ImGui::TextDisabled("y m d h min");
        s.year = std::clamp(s.year, 1950, 2100);
    }
    const double c = pr::solarCosZenith(s.txLat, s.txLon, s.year, s.month, s.day, s.hour + s.minute / 60.0);
    const bool north = s.txLat >= 0;
    const int m = s.month;
    const char* season = (m >= 5 && m <= 8) == north ? "summer" : (m == 11 || m == 12 || m <= 2) == north ? "winter" : "equinox season";
    ImGui::TextDisabled("At the transmitter: %s, sun %.0f deg %s the horizon; %s.", c > 0.1 ? "day" : c < -0.1 ? "night" : "dawn or dusk",
                        std::fabs(std::asin(std::clamp(c, -1.0, 1.0)) * 57.29578), c >= 0 ? "above" : "below", season);
}

void atmoSection(State& S, pr::Scenario& s) {
    S.labelCol = 150 * gUi;
    numField("Rain rate", &s.rainMmH, "mm/h", "Rain along the path (P.838): it matters above about 5 GHz. 2.5 light, 12.5 moderate, 25 heavy, 50 very heavy, 100 tropical.", 70 * gUi);
    ImGui::SameLine();
    static const double rr[] = {0, 2.5, 12.5, 25, 50, 100};
    int sel = -1;
    for (int i = 0; i < 6; i++) if (std::fabs(s.rainMmH - rr[i]) < 1e-9) sel = i;
    ImGui::SetNextItemWidth(100 * gUi);
    if (ImGui::Combo("##advrain", &sel, "none\0light\0moderate\0heavy\0very heavy\0tropical\0") && sel >= 0) s.rainMmH = rr[sel];
    ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("Gases"); ImGui::SameLine(S.labelCol);
    ImGui::Checkbox("oxygen and water vapour##advgas", &s.gas);
    ImGui::SetItemTooltip("%s", "Sea-level gas absorption (rough P.676 curve): small below 10 GHz, about 0.2 dB/km at 22 GHz, 15 dB/km at 60 GHz.");
}

void terrainSection(State& S, pr::Scenario&) {
    ImGui::TextWrapped("Hills are only modelled from an elevation file you already have: SRTM .hgt tiles (keep their names, like N54E025.hgt) or an "
                       "uncompressed GeoTIFF in WGS 84 latitude / longitude. Nothing is downloaded.");
    if (ImGui::SmallButton("Add elevation file...")) {
        const std::string f = openFileDialog();
        if (!f.empty() && std::find(S.terrainFiles.begin(), S.terrainFiles.end(), f) == S.terrainFiles.end()) {
            S.terrainFiles.push_back(f);
            reloadTerrain();
            if (!S.terrainMsg.empty()) S.terrainFiles.pop_back();   // keep only files that load
        }
    }
    if (!S.terrainFiles.empty()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove all##terr")) { S.terrainFiles.clear(); reloadTerrain(); }
    }
    ImGui::TextDisabled("%s", S.terrain ? S.terrain->describe().c_str() : "no terrain: smooth earth");
    if (!S.terrainMsg.empty()) ImGui::TextColored(pal::warnAmber(), "%s", S.terrainMsg.c_str());
    if (S.terrain) ImGui::TextDisabled("Paths that start or end outside the files use smooth earth. The sweeps always use smooth earth.");
}

void viewSection(State& S, pr::Scenario&) {
    S.labelCol = 150 * gUi;
    enumCombo("Map resolution", &S.resolution, "coarse (40)\0medium (80)\0fine (140)\0", "Grid cells per side. Coarse is the cheapest, for a phone or a Raspberry Pi.", 120 * gUi);
    numField("Budget distance", &S.budgetKm, "km", "Where the link budget is computed when no point is chosen on the map (north of the transmitter).", 70 * gUi);
    numField("HF chart distance", &S.hfChartKm, "km", "Path length of the HF time-of-day chart when no point is chosen on the map.", 70 * gUi);
    S.budgetKm = std::clamp(S.budgetKm, 0.05, 20000.0);
    S.hfChartKm = std::clamp(S.hfChartKm, 50.0, 20000.0);
}

// ------------------------------------------------------------------ results
bool budgetPoint(const Result& r, int i, pr::Point& q, std::string& where) {
    double la, lo; std::string lab;
    if (receiverPoint(la, lo, lab)) { q = pr::evaluateAt(r.p[i], la, lo); where = lab; return true; }
    q = pr::evaluate(r.p[i], st().budgetKm, 0);
    where = pr::kmText(st().budgetKm) + " north of the transmitter";
    return false;
}

void probeLine(const Result& r, int i) {
    pr::Point q; std::string where;
    budgetPoint(r, i, q, where);
    const pr::Params& p = r.p[i];
    if (r.compare) { ImGui::TextColored(i ? ImVec4(0.45f, 0.85f, 1, 1) : ImVec4(1, 1, 1, 1), "%s", i ? "B" : "A"); ImGui::SameLine(); }
    const ImVec4 c = q.marginDb >= 10 ? pal::okGreen() : q.marginDb >= 0 ? pal::warnAmber() : pal::badRed();
    ImGui::TextColored(c, "%s: SNR %.1f dB, margin %+.1f dB", pr::ratingName(pr::rating(q.marginDb)), q.snrDb, q.marginDb);
    ImGui::SameLine();
    ImGui::TextDisabled("%s, %.1f km at %.0f deg, %s, loss %.1f dB, %.1f dBm (%.1f dBuV/m), noise %.1f dBm", where.c_str(), q.distKm, q.bearingDeg,
                        pr::pointModelName(q.model), q.pathLossDb, q.rxDbm, q.fieldDbuvm, q.noiseDbm);
    if (p.fMHz < 30) {
        if (q.hops > 0) ImGui::TextDisabled("    skywave %d hop(s) at %.0f deg, MUF %.1f MHz, absorption %.1f dB%s", q.hops, q.elevDeg, q.muf, q.absorptionDb,
                                            q.aboveMufDb > 0.5 ? ", above the MUF (scatter)" : "");
        else ImGui::TextDisabled("    no skywave: %.4g MHz is above the MUF (%.1f MHz) of this path now", p.fMHz, q.muf);
    }
}

void rangeLine(const Result& r, int i) {
    const pr::Range& g = r.range[i];
    if (r.compare) { ImGui::TextColored(i ? ImVec4(0.45f, 0.85f, 1, 1) : ImVec4(1, 1, 1, 1), "%s", i ? "B" : "A"); ImGui::SameLine(); }
    const pr::Verdict v = pr::verdict(r.p[i], g, nullptr, nullptr);
    ImGui::TextWrapped("%s", v.headline.c_str());
    if (r.p[i].terrain && r.bestKm[i] > g.maxKm * 1.2) { ImGui::SameLine(); ImGui::TextDisabled("(median of 8 directions; best %s)", pr::kmText(r.bestKm[i]).c_str()); }
    else if (r.p[i].fMHz >= 30) { ImGui::SameLine(); ImGui::TextDisabled("(radio horizon %s)", pr::kmText(pr::radioHorizonKm(r.p[i].txHeightM, r.p[i].rxHeightM)).c_str()); }
}

void budgetTable(const Result& r) {
    pr::Point qa, qb; std::string wa, wb;
    budgetPoint(r, 0, qa, wa);
    const auto a = pr::budget(r.p[0], qa);
    std::vector<pr::BudgetRow> b;
    if (r.compare) { budgetPoint(r, 1, qb, wb); b = pr::budget(r.p[1], qb); }
    ImGui::TextDisabled("At %s. Hover a term for what it means.", wa.c_str());
    const int cols = r.compare ? 5 : 3;
    if (ImGui::BeginTable("##propbudget", cols, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
                          ImVec2(0, std::max(160 * gUi, ImGui::GetContentRegionAvail().y - 34 * gUi)))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Term", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn(r.compare ? "A" : "Value", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        if (r.compare) { ImGui::TableSetupColumn("B", ImGuiTableColumnFlags_WidthStretch, 1.0f); ImGui::TableSetupColumn("B - A", ImGuiTableColumnFlags_WidthStretch, 1.0f); }
        ImGui::TableSetupColumn("Unit", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableHeadersRow();
        for (const pr::BudgetRow& row : a) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (row.total) ImGui::TextColored(pal::heading(), "%s", row.label.c_str()); else ImGui::TextUnformatted(row.label.c_str());
            if (ImGui::IsItemHovered()) { ImGui::BeginTooltip(); ImGui::PushTextWrapPos(380 * gUi); ImGui::TextUnformatted(row.help.c_str()); ImGui::PopTextWrapPos(); ImGui::EndTooltip(); }
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", row.value);
            if (r.compare) {
                const pr::BudgetRow* m = nullptr;
                for (const auto& x : b) if (x.label == row.label) { m = &x; break; }
                ImGui::TableNextColumn();
                if (m) ImGui::Text("%.2f", m->value);
                ImGui::TableNextColumn();
                if (m) {
                    const double d = m->value - row.value;
                    if (std::fabs(d) < 0.005) ImGui::TextDisabled("0"); else ImGui::Text("%+.2f", d);
                }
            }
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", row.unit.c_str());
        }
        ImGui::EndTable();
    }
}

void exportCsv(const Result& r) {
    char nm[64];
    const std::time_t now = std::time(nullptr);
    std::strftime(nm, sizeof nm, "propagation_%Y%m%d_%H%M%S.csv", std::localtime(&now));
    const std::string path = saveFileDialog(nm);
    if (path.empty()) return;
    pr::Point qa, qb; std::string wa, wb;
    budgetPoint(r, 0, qa, wa);
    const auto a = pr::budget(r.p[0], qa);
    std::vector<pr::BudgetRow> b;
    if (r.compare) { budgetPoint(r, 1, qb, wb); b = pr::budget(r.p[1], qb); }
    std::string s = "# OnAir propagation: link budget at " + wa + "\n" + pr::budgetCsv(a, r.compare ? &b : nullptr);
    if (r.sweeps) {
        for (int i = 0; i < (r.compare ? 2 : 1); i++) {
            s += std::string("\n# sweeps") + (r.compare ? (i ? " B" : " A") : "") + "\n";
            s += pr::seriesCsv({r.sigVsDist[i], r.rangeVsPower[i], r.rangeVsFreq[i]});
            if (r.p[i].fMHz < 30 && !r.day[i].hour.empty()) s += "# HF over the day\n" + pr::hfDayCsv(r.day[i]);
        }
    }
    FILE* f = dect2::u8fopen(path, "wb");
    if (!f) { st().csvMsg = "could not write " + path; return; }
    std::fwrite(s.data(), 1, s.size(), f);
    std::fclose(f);
    st().csvMsg = "saved " + path;
}

void seriesPlot(const char* id, const Result& r, const pr::Series* sa, const pr::Series* sb, double hline, const char* hlabel) {
    if (!sa || sa->x.empty()) return;
    if (plt::BeginPlot(id, ImVec2(-1, 170 * gUi), plt::Flags_NoTitle)) {
        plt::SetupAxes(sa->xLabel.c_str(), sa->yLabel.c_str(), plt::AxisFlags_AutoFit, plt::AxisFlags_AutoFit);
        if (sa->logX) plt::SetupAxisScale(plt::X1, plt::Scale_Log10);
        plt::Spec s; s.LineColor = ImVec4(1, 1, 1, 1); s.LineWeight = 2;
        plt::PlotLine(r.compare ? "A" : sa->name.c_str(), sa->x.data(), sa->y.data(), (int)sa->x.size(), s);
        if (sb && !sb->x.empty()) {
            plt::Spec t; t.LineColor = ImVec4(0.45f, 0.85f, 1, 1); t.LineWeight = 2;
            plt::PlotLine("B", sb->x.data(), sb->y.data(), (int)sb->x.size(), t);
        }
        if (hlabel) {
            plt::Spec h; h.LineColor = pal::warnAmber(); h.Flags = plt::InfLines_Horizontal;
            plt::PlotInfLines(hlabel, &hline, 1, h);
        }
        plt::EndPlot();
    }
}

void sweepsTab(const Result& r) {
    if (!r.sweeps) { ImGui::TextDisabled("computing..."); return; }
    const bool cmp = r.compare;
    ImGui::TextDisabled("Toward %s (bearing %.0f deg); smooth earth. White: A%s.", st().havePoint || receiverLocationSet() ? "the receiver point" : "north",
                        r.sweepBearing, cmp ? ", blue: B" : "");
    ImGui::TextUnformatted("Signal against distance");
    seriesPlot("##sw1", r, &r.sigVsDist[0], cmp ? &r.sigVsDist[1] : nullptr, r.p[0].requiredSnrDb, "needed");
    ImGui::TextUnformatted("Range against transmitter power");
    seriesPlot("##sw2", r, &r.rangeVsPower[0], cmp ? &r.rangeVsPower[1] : nullptr, 0, nullptr);
    ImGui::TextUnformatted("Range against frequency (same antennas and power)");
    seriesPlot("##sw3", r, &r.rangeVsFreq[0], cmp ? &r.rangeVsFreq[1] : nullptr, 0, nullptr);
    if (r.p[0].fMHz < 30 && !r.day[0].hour.empty()) {
        const pr::HfDay& d = r.day[0];
        ImGui::Text("HF over the day (UTC), %s path: MUF, FOT (0.85 MUF) and LUF (dots: the lowest frequency that still meets the SNR)", pr::kmText(r.sweepKm).c_str());
        if (plt::BeginPlot("##swday", ImVec2(-1, 190 * gUi), plt::Flags_NoTitle)) {
            plt::SetupAxes("UTC hour", "MHz", 0, plt::AxisFlags_AutoFit);
            plt::SetupAxisLimits(plt::X1, 0, 24, plt::Cond_Always);
            plt::Spec m; m.LineColor = ImVec4(1, 1, 1, 1); m.LineWeight = 2;
            plt::PlotLine("MUF", d.hour.data(), d.muf.data(), (int)d.hour.size(), m);
            plt::Spec f; f.LineColor = pal::okGreen();
            plt::PlotLine("FOT", d.hour.data(), d.fot.data(), (int)d.hour.size(), f);
            std::vector<double> lh, lv;
            for (size_t i = 0; i < d.hour.size(); i++) if (d.luf[i] > 0) { lh.push_back(d.hour[i]); lv.push_back(d.luf[i]); }
            plt::Spec l; l.Marker = plt::Marker_Circle; l.MarkerFillColor = pal::badRed(); l.MarkerLineColor = pal::badRed(); l.MarkerSize = 3;
            if (!lh.empty()) plt::PlotScatter("LUF", lh.data(), lv.data(), (int)lh.size(), l);
            const double fo = r.p[0].fMHz;
            plt::Spec h; h.LineColor = pal::warnAmber(); h.Flags = plt::InfLines_Horizontal;
            plt::PlotInfLines("your frequency", &fo, 1, h);
            plt::EndPlot();
        }
        ImGui::TextDisabled("Usable when your frequency is between the LUF dots and the MUF line.");
    }
}

void modelsTab(const Result& r) {
    ImGui::PushTextWrapPos(0);
    ImGui::TextColored(pal::heading(), "Now: %s", pr::modelName(r.p[0]));
    ImGui::TextWrapped("%s", pr::modelLimits(r.p[0]).c_str());
    ImGui::Separator();
    ImGui::TextColored(pal::heading(), "30 MHz and up");
    ImGui::BulletText("Free space (ITU-R P.525): the spreading loss 32.45 + 20 log f + 20 log d. The upper bound of any real path.");
    ImGui::BulletText("Ground reflection (plane earth / two-ray): beyond 4 pi h1 h2 / lambda the reflected wave cancels the direct one and the loss "
                      "grows 40 dB per decade. Antenna height is worth 6 dB per doubling. Agrees with Okumura-Hata open-area within a few dB.");
    ImGui::BulletText("Smooth-earth diffraction (ITU-R P.526): the earth's bulge (4/3 radius for normal refraction) blocks the path; it blends in before "
                      "the radio horizon 4.12 (sqrt h1 + sqrt h2) km and grows fast beyond it.");
    ImGui::BulletText("Troposcatter (ITU-R P.452, 50 %% of the time): weak scatter from the troposphere, which carries VHF/UHF a few hundred km beyond the horizon.");
    ImGui::BulletText("Receiver clutter (ITU-R P.2108), rain (P.838 with a P.530 effective length), gases (a rough P.676 curve), polarisation mismatch.");
    ImGui::BulletText("Terrain (only with your elevation file): delta-Bullington knife-edge diffraction along the path profile (P.526 / P.1812).");
    ImGui::BulletText("Not modelled: buildings in detail, foliage depth, ducting and anomalous propagation, sporadic E, aircraft scatter, time and location statistics.");
    ImGui::TextColored(pal::heading(), "Below 30 MHz (an estimate)");
    ImGui::BulletText("Ground wave: Norton's surface wave over the chosen ground (Terman's form), with the earth's curvature from P.526 far out. "
                      "Vertical antennas over sea carry farthest; horizontal ones barely have a ground wave.");
    ImGui::BulletText("Skywave: one F2 layer at 300 km. foF2 from the sun's height at the path midpoint, the season, latitude, the SFI and K index you enter. "
                      "MUF = foF2 x sec(angle at the layer); hops of at most about 3200 km. D-layer absorption in the CCIR / P.533 form, an above-MUF scatter "
                      "loss, 2 dB per extra ground reflection and P.533's 8.7 dB excess loss.");
    ImGui::BulletText("Not modelled: the E layer and sporadic E, monthly ionospheric maps, tilts, the equatorial anomaly, polar-cap events, fading statistics.");
    ImGui::TextColored(pal::heading(), "Noise and the receiver");
    ImGui::BulletText("ITU-R P.372: man-made noise by environment, galactic noise above about 10 MHz, atmospheric noise (rough fit: night, summer, tropics). "
                      "Never below 290 K at the antenna.");
    ImGui::BulletText("The receive chain by Friis: preamplifier, feeder, radio (its noise figure at the gain setting). Radio figures are typical, a unit can differ by a few dB.");
    ImGui::TextDisabled("All formulas are written from the public ITU-R texts; the details are in docs/propagation.md.");
    ImGui::PopTextWrapPos();
}

} // namespace

void advancedInputs(App&) {
    State& S = st();
    if (S.compare) {
        for (int i = 0; i < 2; i++) {
            if (i) ImGui::SameLine(0, 0);
            const bool on = S.editing == i;
            ImGui::PushStyleColor(ImGuiCol_Button, on ? (i ? ImVec4(0.2f, 0.5f, 0.7f, 1) : pal::accent(0.55f)) : ImVec4(0.16f, 0.17f, 0.18f, 1));
            if (ImGui::Button(i ? "Edit B" : "Edit A", ImVec2(70 * gUi, 0))) S.editing = i;
            ImGui::PopStyleColor();
        }
        ImGui::SameLine(0, 8 * gUi);
        if (ImGui::SmallButton(S.editing ? "Copy A here" : "Copy B here")) {
            const int o = 1 - S.editing;
            S.sc[S.editing] = S.sc[o];
            S.pattern[S.editing] = S.pattern[o];
            S.patternMsg[S.editing] = S.patternMsg[o];
        }
        ImGui::SameLine(0, 8 * gUi);
        ImGui::TextDisabled("map:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(50 * gUi);
        ImGui::Combo("##advshow", &S.show, "A\0B\0");
    } else S.editing = 0;
    section("Transmitter", true, txSection);
    section("Signal", true, signalSection);
    section("Receiver and radio", true, rxSection);
    section("Path and surroundings", false, pathSection);
    section("Ionosphere and time (HF)", cur().fMHz < 30, ionoSection);
    section("Rain and gases", cur().fMHz > 5000, atmoSection);
    section("Terrain (your elevation file)", false, terrainSection);
    section("Map and charts", false, viewSection);
    S.labelCol = 0;
    issuesUi(cur());
}

void advancedResults(App& a, const std::shared_ptr<const Result>& r) {
    State& S = st();
    if (r) for (int i = 0; i < (r->compare ? 2 : 1); i++) rangeLine(*r, i);
    else ImGui::TextDisabled("computing...");
    if (ImGui::BeginTabBar("##proptabs")) {
        if (ImGui::BeginTabItem("Map")) {
            const float lines = r ? (float)(r->compare ? 2 : 1) * (r->p[0].fMHz < 30 ? 2.f : 1.f) : 1.f;
            mapView(a, (lines + 0.5f) * ImGui::GetFrameHeightWithSpacing());
            if (r) for (int i = 0; i < (r->compare ? 2 : 1); i++) probeLine(*r, i);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Link budget")) {
            if (r) {
                if (ImGui::SmallButton("Export CSV")) exportCsv(*r);
                ImGui::SetItemTooltip("%s", "The link budget, the sweeps and (HF) the MUF over the day, in one CSV file.");
                if (!S.csvMsg.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%s", S.csvMsg.c_str()); }
                budgetTable(*r);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Sweeps")) {
            if (r) {
                ImGui::BeginChild("##propsweeps", ImVec2(0, 0));
                sweepsTab(*r);
                ImGui::EndChild();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Models")) {
            if (r) {
                ImGui::BeginChild("##propmodels", ImVec2(0, 0));
                modelsTab(*r);
                ImGui::EndChild();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

} // namespace propui
