// The list of datasets OnAir ships or can download (see datasets.h). Kept apart from datasets.cpp so the logic there links without the data tables.
#include "dect2/packs/datasets.h"
#include "dect2/airports/airports.h"
#include "dect2/bandplan/bandplan.h"
#include "dect2/channels/freq_db.h"
#include "dect2/packs/packs.h"

namespace dect2::datasets {

// ---------------------------------------------------------------- inventory
std::vector<Descriptor> inventory() {
    std::vector<Descriptor> v;
    {
        Descriptor d;
        d.id = "frequencies"; d.name = "Frequency lists (HF, VHF, marine, aviation, amateur, satellite, PMR, broadcast, data, QO-100)";
        d.source = "data/frequencies/*.tsv; each group names its sources (WSJT-X tables, IARU band plans, ITU-R, national publications)";
        d.licence = "Facts compiled by the OnAir project; same licence as OnAir (GPL-3.0-or-later)"; d.attribution = "See the @source line of each group";
        d.version = "bundled"; d.format = Format::FreqTsv; d.bundled = true;
        d.note = "Your records win over the bundled one with the same frequency, mode and name; other records are kept.";
        d.bundledText = [] { std::string t; for (size_t i = 0; i < freqDataFileCount(); i++) { t += freqDataFile(i); t += '\n'; } return t; };
        v.push_back(d);
    }
    {
        Descriptor d;
        d.id = "airports"; d.name = "Airports and their radio frequencies";
        d.source = "OurAirports (ourairports.com/data), built by tools/gen_airports.py"; d.licence = "Public domain (OurAirports)"; d.attribution = "OurAirports, David Megginson";
        d.version = "bundled"; d.format = Format::AirportsTsv; d.bundled = true;
        d.note = "Your airports replace the bundled airport with the same code (with its frequencies); new codes are added.";
        d.bundledText = [] { return std::string(builtinAirportText()); };
        v.push_back(d);
    }
    {
        Descriptor d;
        d.id = "bandplans"; d.name = "Band plans (TV channel plans, amateur, broadcast)";
        d.source = "data/bandplans/builtin.json: CEPT, ITU-R, IARU and national band plans, each with its source"; d.licence = "Facts compiled by the OnAir project (GPL-3.0-or-later)";
        d.attribution = "Each template names its source"; d.version = "bundled"; d.format = Format::BandplanJson; d.bundled = true;
        d.note = "A plan with the same id as a bundled one replaces it (Band plans window > import).";
        d.bundledText = [] { return std::string(bandplan::builtinBandPlanText()); };
        v.push_back(d);
    }
    {
        Descriptor d;
        d.id = "marine-mid"; d.name = "Marine MMSI country codes (MID) and flags";
        d.source = "ITU Maritime Identification Digits list (MARS)"; d.licence = "Public list; table written by the OnAir project"; d.attribution = "ITU";
        d.version = "compiled in"; d.bundled = true; d.replaceable = false; d.note = "A code table in the program (core/src/marine/marine_mmsi.cpp); updates come with a new OnAir version.";
        v.push_back(d);
    }
    // the downloadable packs (View > Data packs): source and licence only
    for (const packs::Pack& p : packs::builtinCatalog()) {
        if (p.id.compare(0, 6, "tiles-") == 0) continue;
        Descriptor d;
        d.id = p.id; d.name = p.title; d.source = p.source; d.licence = p.licence; d.attribution = p.attribution;
        d.version = "download"; d.downloadable = true; d.replaceable = false; d.note = "Downloaded on your click in View > Data packs.";
        v.push_back(d);
    }
    for (Descriptor& d : v) {
        if (d.id == "frequencies") d.schema = Schema{Format::FreqTsv, '|', {}};
        else if (d.id == "airports") d.schema = Schema{Format::AirportsTsv, '\t', {}};
        else if (d.id == "bandplans") d.schema = Schema{Format::BandplanJson, ',', {}};
    }
    return v;
}

}  // namespace dect2::datasets
