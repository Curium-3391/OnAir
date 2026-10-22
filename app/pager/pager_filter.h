// Pagers: the "Decoded only" table filter. Pure predicate so a test can check it; the UI applies it to the message table, the Copy export and the side list.
#pragma once
#include "dect2/pager/pager_tel.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace dect2 {

// true when the page carries readable content: not a tone-only page and not one whose text came out empty (FLEX has no tone type; an empty text there is a failed decode).
inline bool pagerIsDecoded(const PagerMessage& m) {
    if (m.type == kPagerTone) return false;
    for (char c : m.text)
        if (!(c == ' ' || c == '\t' || c == '\r' || c == '\n')) return true;
    return false;
}

// One row of the "Capcodes" view: an address (POCSAG RIC or FLEX capcode) heard, with how often and when last.
struct PagerHeard {
    uint32_t address = 0;
    bool flex = false;
    int count = 0;
    int64_t lastWall = 0;        // wall clock of the last page
    uint64_t lastSerial = 0;
    int speed = 0;               // PagerSpeed of the last page
    double carrierHz = 0;        // whole-span: the carrier of the last page
    int carrierId = 0;
    std::string lastText;        // the last page that had text (may be empty)
};

// Counts the pages per (protocol, address); busiest first, then the most recent. The same RIC on POCSAG and FLEX stays two rows.
inline std::vector<PagerHeard> pagerHeardTable(const std::vector<PagerMessage>& msgs) {
    std::vector<PagerHeard> rows;
    for (const PagerMessage& m : msgs) {
        auto it = std::find_if(rows.begin(), rows.end(), [&](const PagerHeard& r) { return r.address == m.address && r.flex == m.flex; });
        if (it == rows.end()) { rows.emplace_back(); it = rows.end() - 1; it->address = m.address; it->flex = m.flex; }
        it->count++;
        if (m.serial >= it->lastSerial) {
            it->lastSerial = m.serial; it->lastWall = m.wallTime; it->speed = m.speed; it->carrierHz = m.carrierHz; it->carrierId = m.carrierId;
            if (pagerIsDecoded(m)) it->lastText = m.text;
        }
    }
    std::sort(rows.begin(), rows.end(), [](const PagerHeard& a, const PagerHeard& b) { return a.count != b.count ? a.count > b.count : a.lastSerial > b.lastSerial; });
    return rows;
}

}  // namespace dect2
