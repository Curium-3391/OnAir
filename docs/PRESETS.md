# Per-mode presets

A preset is a named set of the settings of one mode: frequency, sample rate, bandwidth, gain (and auto gain), antenna power and
port, squelch, audio, spectrum and the mode's own options (`opt.<name>`). The toolbar **Presets** button saves the mode on screen
under a name, loads, favourites, renames and deletes presets, sets one as the mode's default (applied when the mode is entered), and
**Reset mode to defaults** goes back to the mode's ModeTuning values. Built-in presets: FM broadcast, airband AM, marine channel 16,
ADS-B, ham SSB 20 m and 40 m, NOAA APT, DAB 5C and DVB-T.

- Stored in `presets.json` in the data directory (own file, so the settings backup does not need to know about it).
- Export is a small `.onairpreset` JSON (`format`, `version`, `name`, `mode`, `values`). Unknown fields and unknown setting keys of a
  newer version are kept and written back. Anything named like a key, token, password or secret is never written or read. Location
  (`loc.lat`, `loc.lon`) is left out unless "Copy with my location" is used.
- Applying validates every value (range, type); values outside the radio's range, auto gain or antenna power on a radio without them,
  and an antenna the radio lacks are skipped with a note. A preset's `radio` is a hint only.
- Other modules (audio popup, antenna choice, mode options) join with `presetsapp::registerProvider`.
- Code: `core/src/presets/preset.cpp` (pure, tested by `tests/presets/test_presets.cpp`), `app/state/presets_app.cpp`.
