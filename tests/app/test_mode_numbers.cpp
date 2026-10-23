// Mode numbers stay unique (docs/MODE-NUMBERS.md). Modes were added by separate branches that picked their numbers on their own, so
// two of them could take the same family, engine standard or poll slot and still compile (a duplicate mode-table row or seqs[] slot is
// not a compile error). This test fails on any such clash:
//   - every family in the mode table (app/ui/mode_table.cpp) is unique and below kNumFamilies (999 Plugins and the Mode Builder row
//     19999 are the only numbers allowed above it);
//   - every engine standard in the tuning registry (modeTunings()) is unique, and each family's engine standard (family + 2) has at
//     most one registry entry;
//   - every poll slot (seqs[N] in core/src/engine/engine_telemetry.cpp) belongs to one receiver only, and is below kPollSeqSlots.
// Run with --markdown to print the table docs/MODE-NUMBERS.md is made from.
#include "dect2/engine/mode_registry.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

static int fails = 0;
#define CHECK(c, fmt, ...) do { if (!(c)) { std::printf("FAIL line %d: " fmt "\n", __LINE__ __VA_OPT__(,) __VA_ARGS__); fails++; } } while (0)   // Flawfinder: ignore (the format is a literal)

static std::string readFile(const std::string& rel) {
    std::ifstream in(std::string(DECT2_SOURCE_DIR) + "/" + rel);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// named constants used as family numbers or slots: "inline constexpr int kName = 146;" or "= kOther + 2;" / "- 1"
static std::map<std::string, std::string> gConstText;
static void scanConstants(const std::string& text) {
    static const std::regex re(R"(constexpr\s+int\s+(k[A-Za-z0-9_]+)\s*=\s*([^;]+);)");
    for (auto it = std::sregex_iterator(text.begin(), text.end(), re); it != std::sregex_iterator(); ++it)
        gConstText[(*it)[1].str()] = (*it)[2].str();
}
static bool evalExpr(std::string e, int& out, int depth = 0) {
    if (depth > 8) return false;
    while (!e.empty() && (e.back() == ' ' || e.back() == '\t')) e.pop_back();
    while (!e.empty() && (e.front() == ' ' || e.front() == '\t')) e.erase(e.begin());
    const size_t colons = e.rfind("::");
    if (colons != std::string::npos) e = e.substr(colons + 2);
    const size_t op = e.find_last_of("+-");
    if (op != std::string::npos && op > 0) {
        int a = 0, b = 0;
        if (!evalExpr(e.substr(0, op), a, depth + 1) || !evalExpr(e.substr(op + 1), b, depth + 1)) return false;
        out = e[op] == '+' ? a + b : a - b;
        return true;
    }
    if (!e.empty() && std::isdigit((unsigned char)e[0])) { out = std::atoi(e.c_str()); return true; }
    auto it = gConstText.find(e);
    return it != gConstText.end() && evalExpr(it->second, out, depth + 1);
}

static int numberAfter(const std::string& text, const char* key) {
    const size_t q = text.find(key);
    return q == std::string::npos ? -1 : std::atoi(text.c_str() + q + std::strlen(key));
}

int main(int argc, char** argv) {
    const bool markdown = argc > 1 && std::strcmp(argv[1], "--markdown") == 0;
    // the headers that hold the named family numbers
    for (const char* h : {"core/include/dect2/railway/railway_modes.h", "core/include/dect2/horus/horus_modes.h",
                          "core/include/dect2/lora/lora_modes.h", "core/include/dect2/nfc/nfc_modes.h"})
        scanConstants(readFile(h));
    const int numFamilies = numberAfter(readFile("app/app.h"), "constexpr int kNumFamilies = ");
    const int pollSlots = numberAfter(readFile("core/include/dect2/engine/engine.h"), "kPollSeqSlots = ");
    CHECK(numFamilies > 0 && pollSlots > 0, "kNumFamilies (%d) or kPollSeqSlots (%d) not found", numFamilies, pollSlots);

    // 1. families: the rows of the mode table
    const std::string table = readFile("app/ui/mode_table.cpp");
    CHECK(!table.empty(), "app/ui/mode_table.cpp not read");
    std::map<int, std::vector<std::string>> fam;
    {
        static const std::regex row(R"(\n\s*\{\s*([A-Za-z0-9_:]+(?:\s*[+-]\s*[0-9]+)?)\s*,\s*\"([^\"]*)\")");
        for (auto it = std::sregex_iterator(table.begin(), table.end(), row); it != std::sregex_iterator(); ++it) {
            int f = 0;
            const std::string expr = (*it)[1].str(), name = (*it)[2].str();
            if (!evalExpr(expr, f)) { CHECK(false, "mode row \"%s\": cannot read the family '%s'", name.c_str(), expr.c_str()); continue; }
            fam[f].push_back(name);
        }
    }
    CHECK(fam.size() > 100, "only %zu families read from the mode table", fam.size());
    for (const auto& [f, names] : fam) {
        if (names.size() > 1) {
            std::string all; for (const auto& n : names) all += " \"" + n + "\"";
            CHECK(false, "family %d is used by %zu mode rows:%s", f, names.size(), all.c_str());
        }
        if (f != 999 && f != 19999) CHECK(f >= 0 && f < numFamilies, "family %d (%s) is outside 0 .. kNumFamilies-1 (%d)", f, names[0].c_str(), numFamilies);
    }

    // 2. engine standards: the tuning registry
    std::map<int, std::vector<std::string>> std2;
    for (const auto& m : dect2::modeTunings()) std2[m.stdMode].push_back(m.id ? m.id : "?");
    for (const auto& [s, ids] : std2) {
        if (ids.size() > 1) {
            std::string all; for (const auto& n : ids) all += " " + n;
            CHECK(false, "engine standard %d is registered %zu times:%s", s, ids.size(), all.c_str());
        }
    }

    // 3. poll slots: seqs[N] in pollMode() (owner = the case label on the line, else the pollNN() helper around it)
    const std::string tel = readFile("core/src/engine/engine_telemetry.cpp");
    CHECK(!tel.empty(), "engine_telemetry.cpp not read");
    std::map<int, std::set<std::string>> slots;
    {
        std::istringstream in(tel);
        std::string line, fn;
        static const std::regex fnRe(R"(^int\s+([A-Za-z0-9_:]+)\s*\()");
        static const std::regex caseRe(R"(case\s+([A-Za-z0-9_:+ -]+?)\s*:)");
        static const std::regex seqRe(R"(seqs\[([A-Za-z0-9_: +-]+)\])");
        while (std::getline(in, line)) {
            std::smatch m;
            if (std::regex_search(line, m, fnRe)) fn = m[1].str();
            std::string owner = fn;
            if (std::regex_search(line, m, caseRe)) owner = "case " + m[1].str();
            for (auto it = std::sregex_iterator(line.begin(), line.end(), seqRe); it != std::sregex_iterator(); ++it) {
                int s = 0;
                if (!evalExpr((*it)[1].str(), s)) { CHECK(false, "cannot read the poll slot '%s'", (*it)[1].str().c_str()); continue; }
                slots[s].insert(owner);
            }
        }
    }
    CHECK(slots.size() > 100, "only %zu poll slots read", slots.size());
    for (const auto& [s, owners] : slots) {
        if (owners.size() > 1) {
            std::string all; for (const auto& o : owners) all += " [" + o + "]";
            CHECK(false, "poll slot %d is used by %zu receivers:%s", s, owners.size(), all.c_str());
        }
        CHECK(s >= 0 && s < pollSlots, "poll slot %d is outside 0 .. kPollSeqSlots-1 (%d)", s, pollSlots);
    }

    if (markdown) {
        std::printf("| family | mode (mode table) | engine standard | active standard | registry id |\n|---|---|---|---|---|\n");
        for (const auto& [f, names] : fam) {
            const int s = f + 2;
            const auto it = std2.find(s);
            std::printf("| %d | %s | %d | %d | %s |\n", f, names[0].c_str(), s, s - 1, it == std2.end() ? "-" : it->second[0].c_str());
        }
        std::printf("\n| poll slot | used by |\n|---|---|\n");
        for (const auto& [s, owners] : slots) std::printf("| %d | %s |\n", s, owners.begin()->c_str());
    }
    if (fails) std::printf("mode numbers: %d failure(s)\n", fails);
    else std::printf("mode numbers: ok (%zu families, %zu engine standards, %zu poll slots)\n", fam.size(), std2.size(), slots.size());
    return fails ? 1 : 0;
}
