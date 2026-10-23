// TETRA traffic-slot selection: the pure choice of which timeslot the voice path follows (tetra_slots.h).
#include "dect2/tetra/tetra_slots.h"
#include <cstring>
using namespace dect2::tetra;
#include "../common/test_util.h"

int main() {
    SlotInfo s[4];
    SlotSelect a;   // Auto
    CHECK(pickSlot(a, s, 100) == kFollowFirstClear, "auto follows the first clear voice");

    SlotSelect two; two.mode = SlotMode::Slot; two.slot = 2;
    CHECK(pickSlot(two, s, 100) == 2, "slot 2 chosen: asked for even while idle");
    s[1].state = SlotState::Encrypted; s[1].lastHeardSec = 99;
    CHECK(pickSlot(two, s, 100) == 2, "an encrypted chosen slot is still named (the voice gate refuses it, the UI says why)");
    SlotSelect bad; bad.mode = SlotMode::Slot; bad.slot = 9;
    CHECK(pickSlot(bad, s, 100) == kFollowFirstClear, "out of range slot falls back to auto");

    SlotSelect tg; tg.mode = SlotMode::Talkgroup; tg.id = 4001;
    CHECK(pickSlot(tg, s, 100) == kFollowNothing, "talkgroup not heard: nothing");
    s[2].state = SlotState::Voice; s[2].talkgroup = 4001; s[2].lastHeardSec = 95;
    CHECK(pickSlot(tg, s, 100) == 3, "talkgroup found on slot 3");
    s[3].state = SlotState::Voice; s[3].talkgroup = 4001; s[3].lastHeardSec = 98;
    CHECK(pickSlot(tg, s, 100) == 4, "two slots with the talkgroup: the most recently heard");
    s[3].state = SlotState::Encrypted;
    CHECK(pickSlot(tg, s, 100) == 3, "an encrypted slot is never followed by talkgroup");
    CHECK(pickSlot(tg, s, 200) == kFollowNothing, "stale talkgroup (older than hold): nothing");
    CHECK(pickSlot(tg, s, 90) == kFollowNothing, "clock before last heard: nothing");
    tg.id = 0;
    CHECK(pickSlot(tg, s, 100) == kFollowNothing, "talkgroup 0 means unknown: never matches");

    SlotSelect is; is.mode = SlotMode::Issi; is.id = 2345678;
    CHECK(pickSlot(is, s, 100) == kFollowNothing, "issi not heard: nothing");
    s[0].state = SlotState::Voice; s[0].issi = 2345678; s[0].lastHeardSec = 99;
    CHECK(pickSlot(is, s, 100) == 1, "issi found on slot 1");

    SlotInfo k;
    updateSlot(k, SlotState::Control, 10);
    CHECK(k.state == SlotState::Control && k.lastHeardSec < 0, "control: no last-heard time");
    updateSlot(k, SlotState::Voice, 11);
    CHECK(k.state == SlotState::Voice && k.lastHeardSec == 11, "voice sets the time");
    updateSlot(k, SlotState::Idle, 13);
    CHECK(k.state == SlotState::Voice && k.lastHeardSec == 11, "a short pause keeps the voice record");
    updateSlot(k, SlotState::Idle, 20);
    CHECK(k.state == SlotState::Idle && k.lastHeardSec == 11, "after the hold the slot is idle, last-heard kept");
    updateSlot(k, SlotState::Encrypted, 21);
    CHECK(k.state == SlotState::Encrypted, "encrypted recorded");
    CHECK(std::strcmp(slotStateText(SlotState::Encrypted), "encrypted, muted") == 0, "encrypted label says muted");
    CHECK(std::strcmp(slotStateText(SlotState::Voice), "voice") == 0, "voice label");
    return testSummary("tetra slots");
}
