// Reference datasets (Tools > Data packs): the list of every dataset OnAir uses, what the user has layered over it, and the opt-in update check.
// Everything here is a pure function or a plain file operation; nothing fetches by itself. The window is app/packs/datasets_ui.cpp.
//
// Manifest (JSON, one file per feed; its sha256 is shown before use):
//   {"format":"onair-datasets-1","entries":[{"id":"frequencies","version":"2026-10-01","url":"https://host.example/frequencies.tsv",
//     "sha256":"<64 hex>","licence":"CC0-1.0","size":12345}, ...]}
// Rules: id = [a-z0-9._-]{1,48}; url is https and must start with an allow-listed source the user added; sha256 is 64 hex; size is in bytes (max 64 MB).
#pragma once
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace dect2::datasets {

// ---------------------------------------------------------------- versions
// Dotted numbers or dates ("1.2.10", "2026-10-01", "20261001"); a trailing "-text" (rc1, beta) sorts before the plain version. -1, 0, 1.
int compareVersions(const std::string& a, const std::string& b);

// ---------------------------------------------------------------- manifest
struct ManifestEntry { std::string id, version, url, sha256, licence; unsigned long long size = 0; };
struct Manifest { std::string format; std::vector<ManifestEntry> entries; };
bool validDatasetId(const std::string& id);
bool parseManifest(const std::string& text, Manifest& out, std::string& err);   // one bad entry fails the whole manifest (all or nothing)
// The url must be https and start with one of the allow-listed sources (matched on a path boundary: "https://h/a" allows "https://h/a/x", not "https://h/ab").
bool urlAllowed(const std::string& url, const std::vector<std::string>& allowList);
bool validAllowListEntry(const std::string& src);   // https://host[/path], no user info, no spaces; example.invalid is accepted (it never resolves)

struct UpdateOffer { ManifestEntry entry; std::string installed; bool isNew = false; };   // isNew: not installed at all
// Entries newer than the installed version (id -> version); entries whose url is outside the allow-list are dropped and their ids listed in rejected.
std::vector<UpdateOffer> selectUpdates(const Manifest& m, const std::map<std::string, std::string>& installed, const std::vector<std::string>& allowList,
                                       std::vector<std::string>* rejected = nullptr);
// Checks a downloaded body against its manifest entry: size, then sha256 (hex, any case).
bool verifyDownload(const ManifestEntry& e, const std::string& body, std::string& err);

// ---------------------------------------------------------------- formats, validation, layering
enum class Format { FreqTsv, AirportsTsv, BandplanJson, Delimited, PlainText };
struct Issue { int line = 0; std::string msg; bool error = true; };
struct Validation {
    size_t records = 0;
    std::vector<Issue> issues;                 // all of them (the window shows the first ones)
    std::vector<std::string> preview;          // the first lines that were accepted
    bool ok() const { for (const Issue& i : issues) if (i.error) return false; return records > 0; }
    size_t errors() const { size_t n = 0; for (const Issue& i : issues) if (i.error) n++; return n; }
};
struct Schema { Format format = Format::PlainText; char delim = '\t'; std::vector<std::string> keyColumns; };   // Delimited: header row names
Validation validateText(const Schema& s, const std::string& text, size_t previewLines = 8);

// Layering: the user's records win over the bundled ones with the same key; the others are kept; new user records are appended.
// Delimited: the header row of the bundled file is kept; user rows are matched by keyColumns (names in the header); the user's header may list the columns in another order.
std::string layerDelimited(const std::string& bundled, const std::string& user, char delim, const std::vector<std::string>& keyColumns, std::string* err = nullptr);
// Grouped records (airports.tsv): a record starts at a line whose first field is startTag and continues until the next such line; key = field keyField of that line.
std::string layerGrouped(const std::string& bundled, const std::string& user, const std::string& startTag, size_t keyField, char delim = '\t');
// Typed records (frequency entries): same rule on any vector.
template <class T, class KeyFn> std::vector<T> layerByKey(std::vector<T> base, const std::vector<T>& over, KeyFn key) {
    std::unordered_map<std::string, size_t> at;
    for (size_t i = 0; i < base.size(); i++) at[key(base[i])] = i;
    for (const T& o : over) {
        const std::string k = key(o);
        const auto it = at.find(k);
        if (it != at.end()) base[it->second] = o;
        else { at[k] = base.size(); base.push_back(o); }
    }
    return base;
}

// ---------------------------------------------------------------- the datasets
enum class Status { Bundled, UserInstalled, Outdated, Download };   // Download: a pack the user fetches in View > Data packs
struct Descriptor {
    std::string id, name, source, licence, attribution, version, note;
    Format format = Format::PlainText;
    Schema schema;
    bool bundled = false;          // compiled into the program
    bool replaceable = true;       // the user can layer a file over it
    bool downloadable = false;     // handled by the download window (View > Data packs); listed here for source and licence
    std::function<std::string()> bundledText;   // empty for code tables and downloads
};
std::vector<Descriptor> inventory();   // the datasets OnAir knows, stable order

// The user's layer on disk: <dir>/<id>/override.dat and <dir>/<id>/meta.txt (imported file, date, sha256, version).
struct UserMeta { std::string version, from, sha256; long long when = 0; };
void setUserDir(const std::string& dir);             // where overrides are read from (the loaders call userText); "" = none
const std::string& userDir();
std::string userText(const std::string& id);         // the override text, "" when none
bool hasUserCopy(const std::string& id);
bool readMeta(const std::string& id, UserMeta& m);
std::string userCopyPath(const std::string& id);
unsigned long long userCopyBytes(const std::string& id);
// Import: validate, then store. False (and err) when the file is damaged; validation is always filled (the window shows it as a preview before the user confirms).
bool importFile(const Descriptor& d, const std::string& path, Validation& v, std::string& err, const std::string& version = std::string());
bool importFolder(const Descriptor& d, const std::string& folder, Validation& v, std::string& err);   // every .tsv/.csv/.txt/.json in it, joined in name order
bool importText(const Descriptor& d, const std::string& text, const std::string& from, const std::string& version, Validation& v, std::string& err);
bool exportEffective(const Descriptor& d, const std::string& path, std::string& err);   // bundled text with the user layer applied
bool exportBundled(const Descriptor& d, const std::string& path, std::string& err);
bool resetToBundled(const std::string& id, std::string& err);   // removes the override and its meta
std::string effectiveText(const Descriptor& d);                 // bundled with the user layer applied
Status statusOf(const Descriptor& d, const std::map<std::string, std::string>& offeredNewer);   // offeredNewer: ids with a newer version in the last manifest

}  // namespace dect2::datasets
