// Icom IC-R8600 [I/Q OUT] over USB: the plain logic of the native driver (core/src/source/source_icom.cpp), kept free of libusb so it is unit-tested
// (tests/source/test_icom_r8600.cpp) without a radio.
//
// The protocol is that of the MIT-licensed ibelinp/ic-r8600-usb-iq (Pieter Ibelings, 2026) as used by the SoapySDR module of Pawel "Topol"
// Topolski ("Topol in collaboration with AI", MIT). Written again in C++ for OnAir, not copied: see docs/ICOM-R8600.md and THIRD_PARTY.md.
//
// The radio's I/Q port has a Cypress FX2 with no program of its own. It enumerates as 0c26:0022 until Icom's firmware (IC-R8600_usb_iq.spt, from
// Icom's "USB I/Q Package for HDSDR") is written into its RAM with vendor requests 0xA0, then as 0c26:0023 with the live bulk endpoints.
// The firmware is Icom's: OnAir never ships it. The user imports it once (Radio settings > Import Icom firmware); it is converted to the list of
// RAM writes, checked against the SHA-256 of the sequence of the verified Icom package, and stored in the user's codecs folder.
// Receive only: there is no transmit path.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace dect2 {
namespace icom {

constexpr uint16_t kVid = 0x0c26, kPidLoader = 0x0022, kPidStream = 0x0023;
constexpr double kMinHz = 10e3, kMaxHz = 3e9;   // the radio's receive range

// The six I/Q sample rates of the radio, lowest first. The CI-V rate code is 6 - index.
constexpr int kRateCount = 6;
extern const double kRates[kRateCount];
// The smallest rate at or above `wanted` (the fastest when none is); `wanted` <= 0 gives 960 kS/s.
double chooseRate(double wanted);
int rateIndex(double rate);   // -1 when it is none of the six
unsigned char rateCode(double rate);

// ---- the I/Q stream -------------------------------------------------------------------------------------------------------------------
// 16-bit little-endian words, I then Q. The word -32768 is a marker: every marker word is dropped, and the word after a marker is I. A single
// byte or word may be split across USB transfers, so the state lives across feed() calls.
class IqDecoder {
public:
    uint64_t markers = 0, realignments = 0;
    void reset();
    void word(int16_t v, std::vector<int16_t>& out);
    void feed(const unsigned char* b, size_t n, std::vector<int16_t>& out);   // appends I,Q,I,Q... to out
private:
    bool byteCarry_ = false, locked_ = false, marker_ = false, haveI_ = false;
    unsigned char carry_ = 0;
    int16_t pendingI_ = 0;
};

// ---- CI-V (the radio's control protocol, sent over the bulk OUT endpoint 2 and answered on 0x88) ----------------------------------------
// Controller address 0xE0, radio address 0x96.
std::vector<unsigned char> bcdFrequency(uint64_t hz);                         // 5 bytes, least significant digit pair first
std::vector<unsigned char> civTune(double hz);                                // the payload that sets the frequency (command 5)
std::vector<unsigned char> civFrame(const std::vector<unsigned char>& payload);   // FE FE 96 E0 <payload> FD, padded to an even length with FF
enum class CivReply { None, Ack, Nak };
// Looks for the radio's answer to a command in the bytes read so far
CivReply civParseReply(const std::vector<unsigned char>& bytes);

// The OnAir radio-setting keys of the driver (RadioSetting::key). "keep" leaves that parameter of the radio as it is.
//   rfgain 0..255 (Icom units, not dB; 255 = maximum), att keep|0|10|20|30 dB, preamp keep|0|1, ipplus keep|0|1, hfant keep|1|2|3 (below 30 MHz
//   only), hfbpf keep|0|1 (HF band-pass filter for the I/Q output)
bool validSettingKey(const std::string& key);
// The default of each setting
std::string defaultSetting(const std::string& key);
// The CI-V payload that applies a setting (empty: nothing to send, e.g. "keep", or the HF antenna above 30 MHz). false with err set when
// the key is unknown or the value is outside what the radio takes.
bool settingPayload(const std::string& key, const std::string& value, double frequencyHz, std::vector<unsigned char>& payload, std::string& err);
// The sequence the driver sends to start and to stop streaming
std::vector<std::vector<unsigned char>> civEnterStream();                       // before tuning
std::vector<unsigned char> civSetRate(double rate);
std::vector<std::vector<unsigned char>> civLeaveStream();                       // after the last sample

// ---- the firmware -----------------------------------------------------------------------------------------------------------------------
// One EZ-USB RAM write: vendor request 0xA0 (bmRequestType 0x40), wValue = address, wIndex 0.
struct FwRecord {
    uint16_t wValue = 0;
    std::vector<uint8_t> data;
};
// The SHA-256 of the sequence file (spt_seq.json) made from the Icom package IC-R8600_usb_iq.spt that the reference project verified:
// 39 records, 5650 bytes.
extern const char* const kVerifiedSequenceSha256;
// Tests only: another hash to accept (a synthetic firmware); "" goes back to the real one
void setVerifiedSequenceHashForTest(const std::string& sha256);
// Icom's .spt is a binary Cypress "CSPT" script: per record a 32-byte header and the payload. Parses it (bounds checked; only vendor 0x40/0xA0
// writes are taken, anything else is refused).
bool sptToRecords(const std::vector<uint8_t>& spt, std::vector<FwRecord>& out, std::string& err);
// The sequence as the reference project's spt_seq.json writes it: [{"wValue": 58880, "data": "01"}, ...] and a final newline
std::string recordsToJson(const std::vector<FwRecord>& recs);
bool recordsFromJson(const std::string& json, std::vector<FwRecord>& out, std::string& err);
// "" when the records are sane for an FX2 RAM load (first write holds the CPU: 0xE600 <- 1, last releases it: 0xE600 <- 0, every other write
// inside the 16 KiB of code RAM, at most 64 records, 64 KiB), else what is wrong
std::string recordsProblem(const std::vector<FwRecord>& recs);
// True when the sequence is the one of the verified Icom package (SHA-256 of recordsToJson with or without the final newline)
bool recordsVerified(const std::vector<FwRecord>& recs, std::string* sha256 = nullptr);
// Takes the bytes of an .spt (CSPT) or an spt_seq.json, converts and checks them. On success `records` is the firmware to load and `json` its
// canonical file content. err says in plain words what is wrong (not a CSPT file, damaged, a different firmware version, ...).
bool importFirmwareBytes(const std::vector<uint8_t>& file, std::vector<FwRecord>& records, std::string& json, std::string& err);

// Where the stored firmware is looked for, in order: ONAIR_ICOM_FIRMWARE (a file), then icr8600_spt_seq.json and IC-R8600_usb_iq.spt in the OnAir
// codecs folders (codecFolders()).
std::vector<std::string> firmwarePaths();
// The folder-and-file the import writes to (the user codecs folder)
std::string firmwareStorePath();
// Loads and checks the first firmware found; false with err ("no firmware found" when there is none, else what is wrong with the file)
bool loadFirmware(std::vector<FwRecord>& records, std::string& path, std::string& err);
bool firmwareInstalled();
// Reads the file the user chose, converts, checks and stores it. `where` is the stored path.
bool importFirmwareFile(const std::string& path, std::string& where, std::string& err);

// The plain-words state shown with the radio: "" when the firmware is there
inline constexpr const char* kNeedFirmwareText = "needs Icom firmware, see ?";

// ---- finding the radio ------------------------------------------------------------------------------------------------------------------
struct UsbSeen {
    uint16_t vid = 0, pid = 0;
    int bus = 0, address = 0;
    std::string ports;   // the physical route "1-2-3": stays the same when the radio re-enumerates after the firmware load
};
bool isIcomIq(uint16_t vid, uint16_t pid);
bool isLoader(const UsbSeen& u);
std::string usbIdentity(const UsbSeen& u);   // "usb-<bus>-<ports>"
// Every USB device libusb sees that is an IC-R8600 I/Q port (empty when none, or when libusb is not usable)
std::vector<UsbSeen> findUsb();

} // namespace icom
} // namespace dect2
