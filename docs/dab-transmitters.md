# DAB transmitter lists (Map tab)

Every DAB transmitter sends its own identification (TII: MainId / SubId) in the null symbol. OnAir lists every one it hears, on every
channel, on the DAB **Map** tab. To put a transmitter on the map it needs to know where the transmitter stands. That comes from, in order:

1. **Your own list**: `dab-transmitters.csv` in OnAir's data folder (the **open folder** button). It wins over everything else.
2. **The shared list**: `dab-tii/<country>.csv` in [adswill/OnAir-channels](https://github.com/adswill/OnAir-channels), crowd-sourced and
   public domain (CC0). Set the country (`DE`, `GB`, `NO`, ...) on the Map tab; it downloads once a day and is kept for offline use.
3. **The ensemble itself**: FIG 0/22 (EN 300 401 up to v1.4.1) carries the position of the main transmitters and offsets for the others.
   Few networks still send it (the clause was removed from later editions), so for most countries a list is needed.

OnAir does not ship a transmitter database of its own: the lists that exist for some countries come with unclear or non-free licences.

## The format

One transmitter per line, comma separated (a tab-separated file works too). A line starting with `#` is a comment; a first line starting
with `eid` is the header. Columns after `channel_mhz` are ignored, so an export of heard transmitters can be used as it is.

```
eid,main,sub,lat,lon,site,power_kw,channel_mhz
10BC,1,3,47.70361,12.01194,Wendelstein,10,178.352
```

| column | meaning |
| --- | --- |
| `eid` | ensemble id, hexadecimal (the export of heard transmitters has it) |
| `main` | TII MainId, 0 to 69 (the pattern) |
| `sub` | TII SubId, 0 to 23 (the comb) |
| `lat`, `lon` | position in decimal degrees, WGS 84 (north and east positive). A row without both is skipped |
| `site` | the site's name; in double quotes when it holds a comma |
| `power_kw` | ERP in kW, optional |
| `channel_mhz` | the channel's centre frequency in MHz, optional (for people reading the list) |

TII codes are unique within an ensemble, not worldwide, so a transmitter is found by **(eid, main, sub)**. A site that carries several
ensembles needs one line per ensemble, each with that ensemble's id and the codes heard on it.

## Filling a list from what you hear

1. Run a band scan on the Scan tab (it waits for TII on every channel) or leave the receiver on the channels you want.
2. **export heard** on the Map tab writes `dab-heard-<time>.csv` to the data folder: every transmitter heard, in the columns above plus
   `ensemble` and `level_db`, with the position already known where there is one.
3. Fill in `lat`, `lon` and `site` for the transmitters you know (a spreadsheet works; keep it as CSV).
4. **import list...** adds every row with a position to your own list: a transmitter already there is updated, rows without a position are
   skipped, the other lines of your file stay as they were.

To share them, press **submit** on a row of the Map tab (it opens the OnAir-channels issue form with the ids filled in) or attach the file
to an issue there. Only add positions you know yourself or take from a source that allows it (your own survey, an operator's public
press release, an open data portal with a free licence); submissions become public domain.
