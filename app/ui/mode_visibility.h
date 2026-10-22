// Which modes the mode lists show: the rules are plain strings and sets (no ImGui, no mode table), so they are unit-tested.
// Modes and groups are named by their text, never by number. Hidden modes stay reachable: by search (tagged "hidden"), --mode, --pick and deep links.
#pragma once
#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <vector>

namespace modevis {

struct Rules {
    std::set<std::string> hiddenModes, hiddenGroups, favs;
    bool favsOnly = false;
};

// "a|b|c" <-> set (names never hold a |)
inline std::set<std::string> parseList(const std::string& s) {
    std::set<std::string> out;
    size_t b = 0;
    while (b <= s.size()) {
        size_t e = s.find('|', b);
        if (e == std::string::npos) e = s.size();
        if (e > b) out.insert(s.substr(b, e - b));
        b = e + 1;
    }
    return out;
}
inline std::string joinList(const std::set<std::string>& v) {
    std::string out;
    for (const std::string& s : v) { if (!out.empty()) out += "|"; out += s; }
    return out;
}

// hidden by the rules, ignoring that the mode may be the active one
inline bool hiddenByRules(const Rules& r, const std::string& group, const std::string& mode) {
    if (r.hiddenModes.count(mode) || r.hiddenGroups.count(group)) return true;
    return r.favsOnly && !r.favs.count(mode);
}
// hidden from the lists now: the active mode is never hidden
inline bool hidden(const Rules& r, const std::string& group, const std::string& mode, bool active) {
    return !active && hiddenByRules(r, group, mode);
}
// listed in a menu, picker or palette: a search lists the hidden ones too (the caller tags them)
inline bool listed(const Rules& r, const std::string& group, const std::string& mode, bool active, bool searching) {
    return searching || !hidden(r, group, mode, active);
}

// a group's tick in the settings tree: shown, hidden, or some of its modes hidden
enum class GroupState { Shown, Hidden, Mixed };
inline GroupState groupState(const Rules& r, const std::string& group, const std::vector<std::string>& modes) {
    int shown = 0;
    for (const std::string& m : modes) if (!hiddenByRules(r, group, m)) shown++;
    return shown == 0 ? GroupState::Hidden : shown == (int)modes.size() ? GroupState::Shown : GroupState::Mixed;
}
// tick or untick a whole group: the group flag goes and the single hides of its modes with it
inline void setGroupShown(Rules& r, const std::string& group, const std::vector<std::string>& modes, bool on) {
    for (const std::string& m : modes) r.hiddenModes.erase(m);
    if (on) r.hiddenGroups.erase(group); else r.hiddenGroups.insert(group);
}
// tick or untick one mode; ticking one in a hidden group opens the group and hides its other modes singly
inline void setModeShown(Rules& r, const std::string& group, const std::string& mode, const std::vector<std::string>& siblings, bool on) {
    if (on) {
        if (r.hiddenGroups.erase(group)) for (const std::string& s : siblings) if (s != mode) r.hiddenModes.insert(s);
        r.hiddenModes.erase(mode);
    } else r.hiddenModes.insert(mode);
}
inline void showAll(Rules& r) { r.hiddenModes.clear(); r.hiddenGroups.clear(); r.favsOnly = false; }
inline void hideAll(Rules& r, const std::vector<std::string>& groups) { r.hiddenModes.clear(); r.hiddenGroups.insert(groups.begin(), groups.end()); }
inline void reset(Rules& r) { showAll(r); r.favs.clear(); }
inline bool isDefault(const Rules& r) { return r.hiddenModes.empty() && r.hiddenGroups.empty() && !r.favsOnly; }

inline std::string lowerAscii(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }

// presets are lists of words found in group names, so groups other agents add are covered by their names
inline const std::vector<std::string>& presetNames() {
    static const std::vector<std::string> n = {"Ham", "Broadcast", "Aviation", "Marine", "Everything"};
    return n;
}
inline std::vector<std::string> presetWords(const std::string& preset) {
    if (preset == "Ham") return {"amateur", "general", "plugins", "kris"};
    if (preset == "Broadcast") return {"tv", "broadcast", "general", "plugins"};
    if (preset == "Aviation") return {"aviation", "general", "plugins"};
    if (preset == "Marine") return {"maritime", "satellite", "weather", "general", "plugins"};
    return {};
}
// the preset's hidden groups, among all group names; Everything (or an unknown preset) hides none
inline std::set<std::string> presetHiddenGroups(const std::string& preset, const std::vector<std::string>& groups) {
    std::set<std::string> out;
    const std::vector<std::string> w = presetWords(preset);
    if (w.empty()) return out;
    for (const std::string& g : groups) {
        const std::string l = lowerAscii(g);
        bool keep = false;
        for (const std::string& k : w) keep |= l.find(k) != std::string::npos;
        if (!keep) out.insert(g);
    }
    return out;
}
// applying a preset keeps the favourites and the Favourites-only switch
inline void applyPreset(Rules& r, const std::string& preset, const std::vector<std::string>& groups) {
    r.hiddenModes.clear();
    r.hiddenGroups = presetHiddenGroups(preset, groups);
}

// the settings tree's search: a group matches by its name or by any of its modes
inline bool containsNoCase(const std::string& hay, const std::string& needle) { return needle.empty() || lowerAscii(hay).find(lowerAscii(needle)) != std::string::npos; }

}  // namespace modevis
