# Unified layout (Settings > Display > Layout)

1. The spectrum and waterfall are always on screen while decoding, in one region at the top of the central area (about 38 %), never a tab.
2. The mode's decoded views fill the region below it, so signal and decode are visible together.
3. A splitter (1 px line, 5 px hit area) resizes it; double click resets; the header bar or F6 folds it to a one-line strip.
4. The position, size and folded state are remembered per mode (prefs `uniSplit`).
5. A window 1500 px or wider puts the spectrum on the left and the decoded views on the right; the `[top]`/`[left]` toggle in the header overrides it.
6. The tabs of the decoded region stay (Receiver, History, mode tabs); "Overview" (it was only the spectrum) is not made.
7. The left controls panel folds to an icon rail (`<<` button; expand, start/stop, tune icons).
8. Modes keep drawing their own content; the shell lays out the central region once for all of them (`app/ui/main_tabs_unified.inc`).
9. Looks (Settings > Display > Look): Classic, Dev A (graphite) and Dev B (terminal green) are theme presets in `ui2_theme.cpp`: flat fills, 1 px lines, one accent for selection, colour only for meaning.
10. The status line at the bottom is unchanged. Dev flags: `--layout unified`, `--theme 3|4`.
