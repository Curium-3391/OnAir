// Project Horus balloon telemetry, the protocol layer: CRC16-CCITT, the Golay (23,12) code, the Horus Binary layer 2 frame
// (unique word, Golay parity block, bit interleaver, additive scrambler), the v1 (22 byte) and v2 (32 byte) packets, the v2
// custom-field table, the payload ID table and the UKHAS "$$" sentence of Horus RTTY.
//
// Sources (docs/HORUS.md has the details): the horusdemodlib wiki pages "2 Modem Details", "4 Packet Format Details" and
// "5 Customising a Horus Binary v2 Packet" (packet layouts, test packet, custom field syntax, RTTY sentence); the layer 2
// parameters the wiki does not list (interleaver constants, scrambler seed and taps, parity packing) were read from
// horusdemodlib src/horus_l2.c and src/golay23.c (LGPL-2.1, David Rowe, Tomas Haerdin) and implemented again here
// (THIRD_PARTY.md). The CRC definitions were checked against the wiki's published test packet and test sentence.
#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace dect2::horus {

// CRC16-CCITT: polynomial 0x1021, initial value 0xFFFF, no final XOR (the binary packets and the RTTY sentences)
uint16_t crc16(const uint8_t* p, size_t n);
inline uint16_t crc16(const std::string& s) { return crc16(reinterpret_cast<const uint8_t*>(s.data()), s.size()); }

// Golay (23,12), systematic: data in bits 22..11, parity in bits 10..0 (generator 0xC75)
uint32_t golayEncode(uint32_t data12);
// The 12 data bits of the nearest codeword (up to three bit errors corrected); corrected = the number of bits flipped
uint32_t golayDecode(uint32_t word23, int* corrected = nullptr);

constexpr uint8_t kUw[2] = {0x24, 0x24};            // unique word '$$' before every binary frame
constexpr uint8_t kPreamble = 0x1B;                 // the preamble byte (0x1B1B1B1B before the unique word)
constexpr int kV1Bytes = 22, kV2Bytes = 32;

// The whole layer 2 frame for a payload of n bytes, unique word included: 45 bytes for v1, 65 for v2
int frameBytes(int payloadBytes);
// The interleaver constant for a frame of nbytes bytes after the unique word
int interleaverStep(int nbytes);
// Payload -> frame (unique word, data, Golay parity, interleaved and scrambled after the unique word)
std::vector<uint8_t> encodeFrame(const uint8_t* payload, int n);
// The bytes after the unique word (frameBytes(n) - 2 of them) -> the n payload bytes; corrected = Golay bit corrections
void decodeFrame(const uint8_t* body, int n, uint8_t* payload, int* corrected = nullptr);

// One custom field definition (v2 bytes 21..29): the documented struct syntax (< > B b H h f x) and post-processing
// functions (none, battery_5v_byte, divide_by_10, divide_by_100)
struct CustomFormat {
    std::string comment, structFmt;
    std::vector<std::pair<std::string, std::string>> fields;   // name, post-processing
    std::vector<std::string> sentence;                         // OnAir extension: names of the RTTY sentence fields after the altitude
};
// The user-editable tables: payload ID -> callsign, callsign -> custom fields
struct Tables {
    std::map<int, std::string> ids;
    std::map<std::string, CustomFormat> formats;
};
// payload_id_list style text: one "id, callsign" per line (comma, tab or spaces), '#' starts a comment. Returns the lines it could not read.
int parseIdList(const std::string& text, std::map<int, std::string>& out);
// custom_field_list.json style text: {"CALL": {"comment": "...", "struct": "<hhBHxx", "fields": [["name", "fn"], ...]}, ...}
bool parseCustomFields(const std::string& json, std::map<std::string, CustomFormat>& out, std::string* error = nullptr);
std::string defaultIdList();        // the one ID the wiki's test packet shows: 256 = 4FSKTEST-V2
std::string defaultCustomFields();  // the wiki's example entry for 4FSKTEST-V2 (RS41ng default fields)
// Applies a format to the custom bytes: name, value text. Empty when the struct does not fit the bytes.
std::vector<std::pair<std::string, std::string>> applyFormat(const CustomFormat& f, const uint8_t* bytes, int n);

// One decoded packet (binary or RTTY)
struct Packet {
    int kind = 2;                 // 1 Horus Binary v1, 2 Horus Binary v2, 0 RTTY sentence
    int payloadId = -1;           // binary: the payload ID (-1 for RTTY)
    std::string callsign;         // from the ID table, the sentence, or "ID <n>"
    uint32_t seq = 0;
    int hour = 0, minute = 0, second = 0;
    double lat = 0, lon = 0, altM = 0;
    int speedKmh = -1, sats = -1, tempC = -128;
    double battV = -1;
    std::vector<std::pair<std::string, std::string>> custom;   // custom / extra fields, named when the table knows them
    std::vector<uint8_t> raw;     // binary: the payload bytes; RTTY: the sentence text
    std::string sentence;         // the packet as a UKHAS sentence (the form horus_demod prints)
    int corrected = 0;            // Golay bit corrections
    int baud = 100, rttyBits = 0; // RTTY: 7 or 8 data bits
    float snrDb = 0;              // estimated SNR in 3 kHz
    double offsetHz = 0;          // the signal's centre relative to the dial
    double toneSpacingHz = 0;
    int channel = 0;
    int64_t wallTime = 0;         // receive time (Unix seconds)
};

// Binary payload of n (22 or 32) bytes with a good CRC -> packet. False when the CRC fails.
bool parseBinary(const uint8_t* p, int n, const Tables& t, Packet& out);
// Builds a binary payload (the generator and the tests); the CRC is filled in
std::vector<uint8_t> buildBinary(const Packet& p, int version, const uint8_t custom[9] = nullptr);
// "$$CALL,...*CRC" (with or without the leading $$ and the newline) -> packet. False when the CRC or the layout fails.
bool parseSentence(const std::string& line, const Tables& t, Packet& out);
// The UKHAS sentence of a packet, CRC included ("$$CALL,seq,HH:MM:SS,lat,lon,alt,...*XXXX")
std::string buildSentence(const Packet& p);

} // namespace dect2::horus
