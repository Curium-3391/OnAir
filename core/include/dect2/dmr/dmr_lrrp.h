// Motorola LRRP (Location Request/Response Protocol): the GPS position reports a DMR radio sends in
// "unconfirmed data, IP" packets. The payload OnAir assembles is a plain IPv4 + UDP packet; the UDP
// payload is an LRRP (MBXML) document. See dmr_lrrp.cpp for the token layout.
#pragma once
#include "dect2/dmr/dmr_tel.h"
#include <cstddef>
#include <cstdint>

namespace dect2 {

// Parses one IPv4/UDP/LRRP packet. Returns true when `data` holds a valid IPv4 packet (protocol UDP)
// whose payload is an LRRP document; fills `out` with the position or the NoFix status. False for
// everything else (short data, voice, other IP traffic). Never reads past `size`.
bool parseDmrLrrp(const uint8_t* data, size_t size, DmrLocation& out);

} // namespace dect2
