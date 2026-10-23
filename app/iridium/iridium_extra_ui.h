// Iridium ACARS, packet dump and pager screens (tabs next to the Calls view, see iridium_ui.cpp).
#pragma once

struct App;

namespace iridiumui {

void acarsView(App& a);     // ACARS carried in short burst data: messages, aircraft, filters, copy / save
void packetsView(App& a);   // LAP / IDA traffic: joined packets and single frames, hex + ASCII, filters, copy / save
void pagerView(App& a);     // paging channel: pager messages and the paged identities of the ring alerts, search

} // namespace iridiumui
