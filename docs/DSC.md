# Marine DSC (ITU-R M.493)

Receive only. Choose the Marine DSC mode: MF/HF (100 bd FSK, 2187.5, 4207.5, 6312, 8414.5, 12577, 16804.5 kHz) or VHF channel 70 (156.525 MHz, 1200 bd AFSK, 1300 / 2100 Hz).

## What is decoded
- Formats: distress alert, all ships, group, geographic area, individual, individual (automatic).
- Kinds shown by name: distress alert, distress alert acknowledgement, distress alert relay (and its acknowledgement), test call / acknowledgement, position request (polling) / reply / update, safety and urgency calls, call acknowledgements.
- Category, nature of distress, position (quadrant, degrees, minutes), time, proposed channel or frequency (kHz, MF/HF channel, VHF channel), telecommands, end of sequence (acknowledgement requested / acknowledgement / other).
- MMSI: country from the MID (ITU list) and the kind of station (ship, coast station 00MID, group 0MID, SAR aircraft 111MID, handheld 8MID, AIS-SART 970, MOB 972, EPIRB-AIS 974, AtoN 99MID). Digits that could not be read show as `~`.
- Ship names: put a file `mmsi_names.tsv` in the data folder, one `MMSI<TAB>name` per line (`#` for comments).

## Error handling
Every symbol is sent twice (time diversity, five places apart). The receiver takes the copy that passes the 10-bit check; if the copies disagree the error check character (even parity) decides, trying the least certain symbols first (up to three changed). A symbol lost in both copies is rebuilt from the check character when it is the only one; the call is then marked "rebuilt". More damage than that is shown with "check failed" and the symbols that were read (unreadable ones as `---`).

## Window
Search box over all fields, Export CSV (all kept calls incl. raw symbols), red banner for a checked distress call, detail pane with the raw symbols, repair note and a map of the position (no position is sent to a tile server).

## Alerts
Watchlists: in a Ships list, the entry `DSC distress` raises an alert on every DSC distress alert, relay or acknowledgement; an MMSI entry matches the caller, the called station and the station in distress. DSC calls are also automation events (`dsc.call`) and are kept in the Messages store.

## Ship and coast station lists, and the DSC map (0.3.0)
"Import list..." (DSC view, DSC map, Bands) reads a ship list or coast station list you saved yourself (YADDNET or any CSV / TSV / semicolon / pipe / fixed-width text; nothing is downloaded). Delimiter, header and columns (MMSI, name, call sign, country, type, location, latitude + longitude or one position string, decimal or degrees / minutes / seconds, hemisphere letters or signs) are detected and shown as a preview before the merge. Rules: the MMSI is the key (one record per number, spreadsheet-cut leading zeros of coast station / group numbers are restored), a non-empty cell replaces the stored value, an empty cell never erases one, a position is replaced only by a new position, AIS never overwrites an imported name. The MID country is shown for every MMSI with or without a list. The table is kept in `ship_names.tsv` in the data folder.
The DSC map plots every station with a known position (list or heard): distress red, coast station blue (grey until heard, then full colour with a ring), ships green, groups yellow, SAR / other orange; trails of past positions, lines from the caller to the called station when both have a position, a table (heard first, last-heard age, click to centre) and type / heard-only / text filters.
