# GUI audit (0.3.0, branch gui-audit-030)

A static audit of the whole interface: every `.cpp` / `.mm` under `app/` (265 files, all panels and all mode screens), one rule at a time,
with throw-away scanners over the sources and a read of every hit. The rules that can be checked mechanically are now a test:
`tests/app/test_gui_rules.cpp` (ctest `gui_rules`). Nothing here was checked by running the program (no screen capture); the layout check
build (`-DDECT2_UI_LAYOUT_CHECK=ON`, `tools/ui_layout_check.sh`) remains the run-time check for overlaps and clipping.

Legend: **fixed** = changed in this branch, **ok** = checked, nothing wrong, **left** = known, not changed (reason given).

## Panel families

| Family | Files |
|---|---|
| Shell | `app/ui/` (classic shell, ui2 new interface, menus, toolbar, status bar, tour, wizard, command palette), `app/main*.cpp` |
| Spectrum and plots | `app/gfx/` (plots, waterfall, overlays, export) |
| Windows | `app/panels/` (Messages, Protocol lab, Credits, Diagnostics, TX, TV, analysis tabs, TV recorder, automation, radio calibration ...), `app/state/` (workspaces, receivers strip, results export), `app/memory/`, `app/freqdb/`, `app/watchlist/`, `app/transcribe/`, `app/packs/` |
| Maps | `app/map/`, `app/adsb/adsb_map*.cpp` and the 26 mode screens that draw a map |
| Mode screens | every other folder of `app/` (one per mode: adsb ... zwave) |
| Touch | `app/ui/touch*.cpp` |
| Translations | `app/i18n/*.txt`, `tr()` / `trw()` / `N_()` use |

## Checklist

### 1. `SetCursorPos()` to extend window boundaries (the `##main2` error) - fixed, tested
* Cause: the spectrum overlay buttons (`bwButton`, `wideSwitch`, `multiSwitch`, tuning mode, hints in `app/gfx/plots.cpp`) and the map full-screen
  button (`adsb_map.cpp`, `df_ui.cpp`) put the cursor back with `SetCursorScreenPos(keep)` at the end of the pane. `keep` is below the last
  item, so the following `EndChild()` of `##main2` saw a cursor past the content and ImGui logged the error.
* Fix: `restoreCursor()` (widgets.cpp) restores the spot and clears ImGui's set-position flag (nothing is extended); the TX banner
  (`tx_panel.cpp`) submits a `Dummy` of its size instead of moving the cursor; Globalstar's Doppler view drops a pointless trailing move.
* Rule 1 of `gui_rules`: no cursor move right before `End` / `EndChild` / `EndGroup` / `endPane` or a function's last line.

### 2. Windows that open off-screen or too big - fixed, tested
* A floating window whose title bar left the display (a position saved on a larger or a second monitor, a DPI change) could not be grabbed.
  `keepWindowsReachable()` runs once a frame (main loop) and moves such a window back; it leaves child windows, popups, tooltips and
  windows without a title bar or move alone, and never runs while the mouse drags.
* 24 windows asked for first-use sizes up to 980 x 780 at the interface scale: at 150 % on a 1366 x 768 laptop their bottom edge and the
  resize grip were off the screen. `firstWindowSize()` fits them to the display (`app/ui/window_fit.h`, tested).
* Messages and Protocol lab had first-use sizes in bare pixels (tiny at 200 %).
* Share-your-scan dialog: fixed 620 px width and wrap position now fit a narrow display.
* Touch: every popup is placed as a bottom sheet by `touch.cpp` (ok).

### 3. HiDPI / font scaling (`gUi`) - fixed, tested
* Bare pixel widths: Messages (search, identity, mode, range, storage fields, detail box, table height and columns), results export,
  workspace name, waterfall export, multi-band list filter.
* `WidthFixed` table columns in bare pixels: MPT 1327, scan outputs, FM, analysis, DAB, TV, MIL-HF (32 columns).
* Pills (`pillButton`, `subNav`): padding was unscaled; the classic mode switch (`standardSwitch`): segment padding, group gaps, knob inset.
* Rule 3 of `gui_rules`: no `SetNextItemWidth(<number>)`, `WidthFixed, <number>` or `SetNextWindowSize(ImVec2(<n>, <n>))`, and first-use
  sizes only through `firstWindowSize()`.
* left: drawing offsets of 1-3 px without `gUi` inside custom canvases (maps, plots, Globalstar legend) - cosmetic.

### 4. Text clipped by fixed widths (the RX / TX / Ham class) - fixed, tested
* RX / TX / Ham selector: already sized from its labels (ok).
* Dialog buttons with a fixed width and a translated label (memory edit, frequency database, map and Live rooms consent, transcription
  download / delete): `fitButton()` makes them at least as wide as the label.
* Translation budget, rule 6 of `gui_rules`: the translation of a label of up to 14 characters is at most 3.5 times its English length or
  24 columns longer (CJK counts 2), a label of up to 4 characters at most 18 columns. All 17 334 short translations pass today; the
  longest are Lithuanian ("big endian", "Remove DC") and fit the places they are drawn (text, not fixed-width segments).

### 5. ImGui ID collisions - fixed, tested
* Diagnostics: the IQ and the audio "Show recording" buttons had one ID (both shown after recording both) - now `##iq` / `##aud`.
* Every other function with two widgets of one literal label was read: the pairs are in branches that never run together (switch cases,
  if / else, a disabled and a live form); they are the allow list of rule 4. Loops with widgets of literal labels and no `PushID`: none.
* Rule 2: windows, tabs, tree nodes, collapsing headers and modal popups used `tr()` in 44 places, so their open state, selected tab and
  saved place changed with the language; they use `trw()` now (fixed ID).

### 6. Popups that never close or cannot be reached - fixed (Escape), tested (opened)
* Every modal with a literal name is opened with the same name (rule 5, 17 modals). The touch sheets go through one helper (ok).
  Every modal has a way out (`CloseCurrentPopup` path) (ok).
* Escape now answers Cancel / No in: workspace save, scan sharing (all stages), TV recorder delete, transcription download / delete,
  automation "Run a program?", add receiver, Live rooms. The memory editor's Escape no longer closes it under its own duplicate question.
* left: the codec notice class (a modal opened inside a collapsed tree node) was fixed in 0.2.9; no other `OpenPopup` sits in a different
  ID scope from its `BeginPopup` (every pair was read).

### 7. `SameLine` misuse - fixed
* A `SameLine()` before an item that is only sometimes drawn pulled the next item onto the line: Messages (Newer / Older, the table then
  started beside the counts), VOR (no fix yet: the map started beside the dwell slider), SARSAT (stopped: the view tabs), Marine (database
  note). The `SameLine` now sits inside the condition.

### 8. Buttons that do nothing or give no feedback, missing disabled states - fixed
* Scan (NR, LTE cells, TETRA, GSM-R, NB-IoT), New picture (Meteor, HRPT), the DAB analyser's channel buttons and Identify again / all
  ignored a click while the receiver was stopped. `runningButton()` greys them out with the reason as the tooltip.
* TV: "On top" only acts on the pop-out window, now disabled without it.
* ok: the hold buttons (push to talk, carrier tune) read `IsItemActive` on purpose.

### 9. Keyboard and focus - fixed
* New interface: Space started / stopped the receiver even when the TV picture (pause) or the transmitter (start / stop TX) answered the
  same press: two actions for one key. Those panels claim Space for the frame (`gSpaceClaimFrame`).
* ok: number keys and Space are ignored while a text field has the keyboard.
* left: ImGui keyboard navigation (`NavEnableKeyboard`) stays off: with it on, arrows and Space would also drive the focused widget and clash
  with the app's own keys. A decision for the owner.

### 10. Tooltips, labels, units, terminology - fixed
* Glyph-only buttons without a tooltip: the +/- zoom of 26 map screens, teletext page and sub-page arrows.
* Labels: TV controls were lower case and untranslated ("pop out", "on top", "deinterlace", "Fullscreen"); now "Pop out", "On top",
  "Full screen", "Deinterlace" (with a tooltip), "Volume", "Mute", "Audio", NOW / NEXT, all translated with stable IDs.
* Untranslated dialogs: workspaces (menu, save, restore last session), share your scan (with a real plural instead of "channel%s",
  no space-padded labels).
* Word combo boxes that bypassed `tr()`: protocol lab demodulator, ham TX slot, propagation (click, environment, ground, resolution),
  meteor enhancement, general squelch mode, ADS-B units, constellation view (also its pills), DAB recording format, SAME country,
  Messages time range, results export scope (`comboTr()`).
* Units: Wideband's low colour limit had no unit (the high one had dB).
* Terminology scan (Full screen / Fullscreen, Colour / Color, Wi-Fi, kHz / MHz case, Sample rate, Settings): consistent apart from the TV
  "Fullscreen", fixed.
* left: many mode screens still have English-only text (each mode's own strings); translating them is catalog work, not an interface bug.

### 11. Light / dark theme contrast (hard-coded colours) - ok
* The Light and High contrast themes recolour the finished draw lists (`light_theme.cpp`: lightness turned over, text forced to 5:1 / 7:1,
  `tests/app/test_theme.cpp`), so hard-coded `IM_COL32` panel colours follow the theme. Textures (maps, pictures) keep their colours and the
  text drawn on them keeps its own contrast (map colours: `test_map_colours`).

### 12. Tables: scroll, minimum widths, slow drawing - ok / left
* 315 tables read by scanner: the long lists use `ScrollY` with a frozen header; their sources are capped in the decoders (200-400 rows
  for messages and aircraft), so drawing them without a clipper costs little. The Messages window pages (`kPage`).
* left: per-frame copies of small telemetry structs and the tile log's `logSnapshot` copy; not measurable at those sizes.

### 13. State read before it is initialised - ok
* Every POD member of the app's structs has an initialiser or is set by aggregate initialisation (scanned `app/**/*.h`).

## Tests added
`tests/app/test_gui_rules.cpp` (ctest `gui_rules`): rules 1-6 above on the sources of `app/`, and the window placement / first-use size
logic of `app/ui/window_fit.h`. A new duplicate label in exclusive branches goes into its allow list with the reason.
