// The DMR Map and Table sub-tabs: LRRP positions of the radios on a slippy map (tracks, filters)
// and a table of every position received, with CSV export. See dmr_map.cpp.
#pragma once

struct App;

void dmrMap(App& a);
void dmrTable(App& a);
