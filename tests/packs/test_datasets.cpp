// Reference datasets: versions, manifest parsing, the source allow-list, update selection, checksum check, validation with error lines, layering
// (delimited, grouped, typed), import / export / reset on disk, and the bundled inventory.
#include "dect2/packs/datasets.h"
#include "dect2/engine/updater.h"
#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace dect2::datasets;
namespace fs = std::filesystem;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static std::string manifest(const std::string& entries) { return "{\"format\":\"onair-datasets-1\",\"entries\":[" + entries + "]}"; }
static std::string entry(const std::string& id, const std::string& ver, const std::string& url, const std::string& sha, const std::string& size = "10") {
    return "{\"id\":\"" + id + "\",\"version\":\"" + ver + "\",\"url\":\"" + url + "\",\"sha256\":\"" + sha + "\",\"licence\":\"CC0-1.0\",\"size\":" + size + "}";
}

int main() {
    // versions
    CHECK(compareVersions("1.2.10", "1.2.9") > 0, "numeric not text");
    CHECK(compareVersions("2026-10-01", "2026-09-30") > 0, "dates");
    CHECK(compareVersions("20261001", "2026-10-01") == 0, "compact date equals dashed");
    CHECK(compareVersions("1.0", "1.0.0") == 0, "trailing zero");
    CHECK(compareVersions("1.0-rc1", "1.0") < 0, "pre-release older");
    CHECK(compareVersions("v2", "1.9.9") > 0, "v prefix");
    CHECK(compareVersions("", "1") < 0, "empty is oldest");

    // allow-list
    CHECK(validAllowListEntry("https://feed.example.invalid/onair"), "valid source");
    CHECK(!validAllowListEntry("http://feed.example.invalid"), "http refused");
    CHECK(!validAllowListEntry("https://user@host/x"), "user info refused");
    CHECK(!validAllowListEntry("https://host/a b"), "space refused");
    const std::vector<std::string> allow = {"https://feed.example.invalid/onair/"};
    CHECK(urlAllowed("https://feed.example.invalid/onair/f.tsv", allow), "inside");
    CHECK(urlAllowed("https://FEED.example.invalid/onair/f.tsv", allow), "host case");
    CHECK(!urlAllowed("https://feed.example.invalid/onairx/f.tsv", allow), "path boundary");
    CHECK(!urlAllowed("https://feed.example.invalid.evil.net/onair/f.tsv", allow), "other host");
    CHECK(!urlAllowed("http://feed.example.invalid/onair/f.tsv", allow), "http");
    CHECK(!urlAllowed("https://feed.example.invalid@evil.net/onair/f.tsv", allow), "user info trick");
    CHECK(!urlAllowed("https://feed.example.invalid/onair/f.tsv", {}), "empty list allows nothing");

    // manifest
    const std::string sha(64, 'a');
    Manifest m;
    std::string err;
    CHECK(parseManifest(manifest(entry("frequencies", "2026-10-01", "https://feed.example.invalid/onair/f.tsv", sha)), m, err) && m.entries.size() == 1, "good: %s", err.c_str());
    CHECK(m.entries[0].size == 10 && m.entries[0].licence == "CC0-1.0", "fields");
    CHECK(!parseManifest("{\"format\":\"other\",\"entries\":[]}", m, err), "wrong format");
    CHECK(!parseManifest("not json", m, err), "garbage");
    CHECK(!parseManifest(manifest(entry("Bad Id", "1", "https://h/x", sha)), m, err), "bad id");
    CHECK(!parseManifest(manifest(entry("a", "1", "http://h/x", sha)), m, err), "http url");
    CHECK(!parseManifest(manifest(entry("a", "1", "https://h/x", "abc")), m, err), "short sha");
    CHECK(!parseManifest(manifest(entry("a", "1", "https://h/x", sha, "0")), m, err), "zero size");
    CHECK(!parseManifest(manifest(entry("a", "1", "https://h/x", sha, "99999999999")), m, err), "huge size");
    CHECK(!parseManifest(manifest(entry("a", "1", "https://h/x", sha) + "," + entry("a", "2", "https://h/y", sha)), m, err), "duplicate id");
    CHECK(!parseManifest(manifest(entry("a", "1", "https://h/x", sha) + ",{\"id\":\"b\"}"), m, err), "one bad entry fails all");
    CHECK(!parseManifest(manifest(entry("a", "1", "https://h/x", sha)) + "x", m, err), "trailing text");
    CHECK(parseManifest(manifest(""), m, err) && m.entries.empty(), "empty list is valid");

    // update selection
    CHECK(parseManifest(manifest(entry("frequencies", "2026-10-01", "https://feed.example.invalid/onair/f.tsv", sha) + "," +
                                 entry("airports", "2026-01-01", "https://feed.example.invalid/onair/a.tsv", sha) + "," +
                                 entry("evil", "9", "https://evil.example/x", sha) + "," +
                                 entry("newone", "1", "https://feed.example.invalid/onair/n.tsv", sha)), m, err), "manifest: %s", err.c_str());
    std::vector<std::string> rej;
    const auto up = selectUpdates(m, {{"frequencies", "2026-05-01"}, {"airports", "2026-06-01"}, {"evil", ""}}, allow, &rej);
    CHECK(up.size() == 2 && up[0].entry.id == "frequencies" && up[1].entry.id == "newone" && up[1].isNew, "offers: %zu", up.size());
    CHECK(rej.size() == 1 && rej[0] == "evil", "rejected outside the allow list");
    CHECK(selectUpdates(m, {}, {}).empty(), "no sources, no offers");

    // checksum
    const std::string body = "hello world";
    ManifestEntry e;
    e.size = body.size();
    e.sha256 = dect2::sha256Bytes(body.data(), body.size());
    CHECK(verifyDownload(e, body, err), "good body: %s", err.c_str());
    for (char& c : e.sha256) c = (char)toupper((unsigned char)c);
    CHECK(verifyDownload(e, body, err), "hex case");
    CHECK(!verifyDownload(e, body + "x", err), "size differs");
    CHECK(!verifyDownload(e, "hello worle", err), "content differs");

    // validation: delimited
    Schema ds{Format::Delimited, ',', {"id"}};
    Validation v = validateText(ds, "id,name,khz\nA,Alpha,100\nB,Bravo,200\n");
    CHECK(v.ok() && v.records == 2 && v.preview.size() == 3, "csv ok");
    v = validateText(ds, "id,name,khz\nA,Alpha,100\nB,Bravo\nA,Again,5\n,x,1\n");
    CHECK(!v.ok() && v.errors() == 2, "csv errors %zu", v.errors());
    bool at3 = false, at5 = false, dupWarn = false;
    for (const Issue& i : v.issues) { if (i.line == 3 && i.error) at3 = true; if (i.line == 5 && i.error) at5 = true; if (i.line == 4 && !i.error) dupWarn = true; }
    CHECK(at3 && at5 && dupWarn, "issue lines");
    CHECK(!validateText(ds, "name,khz\nA,1\n").ok(), "missing key column");
    CHECK(!validateText(ds, "").ok(), "empty");
    CHECK(!validateText(ds, std::string("id\nA\0B\n", 8)).ok(), "NUL byte");

    // validation: airports and band plans
    Schema as{Format::AirportsTsv, '\t', {}};
    CHECK(validateText(as, "A\tEGLL\tLHR\tHeathrow\tLondon\tGB\t51.47\t-0.45\t83\tL\t1\nF\tTWR\t118700\tTower\n").ok(), "airports ok");
    v = validateText(as, "F\tTWR\t118700\nA\tXXXX\tX\tN\tC\tGB\t99\t0\t1\tS\t0\nA\tYYYY\tY\tN\tC\tGB\t10\t10\t1\tS\t0\nF\tTWR\t-5\n");
    CHECK(!v.ok() && v.errors() == 3 && v.records == 1, "airport errors %zu records %zu", v.errors(), v.records);
    Schema bs{Format::BandplanJson, ',', {}};
    CHECK(validateText(bs, "{\"format\":\"onair-bandplans-1\",\"plans\":[{\"id\":\"x\"}]}").ok(), "bandplan ok");
    CHECK(!validateText(bs, "{\"format\":\"onair-bandplans-1\",\"plans\":[").ok(), "truncated json");
    CHECK(!validateText(bs, "{\"format\":\"nope\",\"plans\":[1]}").ok(), "wrong bandplan format");

    // layering
    std::string lerr;
    const std::string lay = layerDelimited("id,name,khz\nA,Alpha,100\nB,Bravo,200\n", "khz,id\n999,B\n5,C\n", ',', {"id"}, &lerr);
    CHECK(lay == "id,name,khz\nA,Alpha,100\nB,,999\nC,,5\n", "layered: [%s] %s", lay.c_str(), lerr.c_str());
    CHECK(layerDelimited("id,x\nA,1\n", "name\nz\n", ',', {"id"}, &lerr) == "id,x\nA,1\n" && !lerr.empty(), "no key column in the user file keeps the bundled text");
    const std::string g = layerGrouped("# head\nA\tONE\tx\nF\t1\nA\tTWO\ty\nF\t2\nF\t3\n", "A\tTWO\tz\nF\t9\nA\tTHREE\tw\nF\t7\n", "A", 1);
    CHECK(g == "# head\nA\tONE\tx\nF\t1\nA\tTWO\tz\nF\t9\nA\tTHREE\tw\nF\t7\n", "grouped: [%s]", g.c_str());
    struct R { std::string k; int v; };
    const auto lr = layerByKey(std::vector<R>{{"a", 1}, {"b", 2}}, std::vector<R>{{"b", 9}, {"c", 3}, {"c", 4}}, [](const R& r) { return r.k; });
    CHECK(lr.size() == 3 && lr[1].v == 9 && lr[2].v == 4, "typed layering");

    // files: import, status, export, reset
    const fs::path T = fs::temp_directory_path() / "onair_test_datasets";
    fs::remove_all(T);
    fs::create_directories(T / "in");
    setUserDir((T / "user").string());
    Descriptor d;
    d.id = "stations"; d.name = "Stations"; d.licence = "CC0"; d.format = Format::Delimited; d.schema = ds; d.bundled = true;
    d.bundledText = [] { return std::string("id,name,khz\nA,Alpha,100\nB,Bravo,200\n"); };
    CHECK(statusOf(d, {}) == Status::Bundled, "bundled at first");
    CHECK(effectiveText(d) == d.bundledText(), "no layer");
    { std::ofstream(T / "in" / "bad.csv") << "id,name,khz\nA,x\n"; }
    { std::ofstream(T / "in" / "good.csv") << "id,name,khz\nB,Bee,222\nC,Charlie,300\n"; }
    CHECK(!importFile(d, (T / "in" / "bad.csv").string(), v, err) && v.errors() == 1 && !hasUserCopy("stations"), "a damaged file imports nothing");
    CHECK(importFile(d, (T / "in" / "good.csv").string(), v, err, "2026-10-22") && hasUserCopy("stations"), "good import: %s", err.c_str());
    UserMeta um;
    CHECK(readMeta("stations", um) && um.version == "2026-10-22" && um.from == "good.csv" && um.sha256.size() == 64 && um.when > 0, "meta");
    CHECK(effectiveText(d) == "id,name,khz\nA,Alpha,100\nB,Bee,222\nC,Charlie,300\n", "effective layer");
    CHECK(statusOf(d, {}) == Status::UserInstalled && statusOf(d, {{"stations", "9"}}) == Status::Outdated, "status");
    CHECK(exportEffective(d, (T / "out.csv").string(), err) && fs::file_size(T / "out.csv") == effectiveText(d).size(), "export");
    CHECK(exportBundled(d, (T / "out2.csv").string(), err) && fs::file_size(T / "out2.csv") == d.bundledText().size(), "export bundled");
    CHECK(resetToBundled("stations", err) && !hasUserCopy("stations") && statusOf(d, {}) == Status::Bundled, "reset");
    CHECK(userText("../etc") .empty() && !resetToBundled("../x", err), "path traversal refused");
    // folder import
    CHECK(importFolder(d, (T / "in").string(), v, err) == false, "folder with two files for a one-file dataset is refused");
    fs::remove(T / "in" / "bad.csv");
    CHECK(importFolder(d, (T / "in").string(), v, err) && v.records == 2, "folder import: %s", err.c_str());
    d.replaceable = false;
    CHECK(!importFile(d, (T / "in" / "good.csv").string(), v, err), "not replaceable");

    // the bundled inventory
#ifndef DATASETS_NO_INVENTORY
    const auto inv = inventory();
    bool freq = false, ap = false, bp = false, mid = false;
    for (const Descriptor& x : inv) {
        CHECK(!x.licence.empty() && !x.source.empty() && validDatasetId(x.id), "inventory entry %s has source, licence and a valid id", x.id.c_str());
        if (x.id == "frequencies") freq = x.bundledText && x.bundledText().size() > 1000;
        if (x.id == "airports") ap = x.bundledText && x.bundledText().size() > 1000;
        if (x.id == "bandplans") bp = x.bundledText && x.bundledText().size() > 1000;
        if (x.id == "marine-mid") mid = !x.replaceable;
    }
    CHECK(freq && ap && bp && mid, "frequencies, airports, bandplans and the MID table are listed");
    for (const Descriptor& x : inv) {
        if (!x.bundledText) continue;
        const Validation bv = validateText(x.schema, x.bundledText());
        CHECK(bv.ok(), "the bundled %s passes its own schema (%zu errors, first: %s)", x.id.c_str(), bv.errors(), bv.issues.empty() ? "" : bv.issues[0].msg.c_str());
    }

#endif
    fs::remove_all(T);
    if (fails) { printf("%d failure(s)\n", fails); return 1; }
    printf("datasets: all passed\n");
    return 0;
}
