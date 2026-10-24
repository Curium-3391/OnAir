# OnAir UI kit (app/ui/kit)

A small set of components on top of Dear ImGui (ImGui stays). One theme file, one set of components, so a layout fix lands everywhere.

## Rules
1. No raw `ImGui::SameLine` chains for toolbars: use `kit::beginFlow` / `kit::flowItem(width)` so items wrap and are never clipped.
2. No literal colours or pixel sizes in panels: read `kit::tokens()` (colour roles bg0..bg3, border, text, textDim, accent, ok, warn, error, selection, hover; spacing s2..s16; rowH, controlH, headerH) and scale with `kit::px()`.
3. The presets (Classic, Dev A, Dev B) fill the colour roles in `ui2_theme.cpp` (`kit::syncTokens()` follows `ui2::T`). Panels never pick a preset.
4. Accent means selected or active, nothing else. ok / warn / error mean status, nothing else.

## Components (kit.h)
| Component | Use |
|---|---|
| `beginTabs` / `tab` / `endTab` / `endTabs` | flat tab strip, selected tab filled |
| `button`, `toggleButton`, `checkbox`, `sw`, `iconButton` | controls; `toggleButton` looks pressed while on |
| `sliderValue`, `combo` | value on the track; combo from an array |
| `panelHeader(title, &open, rightText)` | tool-window header: fold arrow, title, right text, 1 px rule |
| `sectionHeader`, `collapsingSection` | small heading with a rule; collapsible block |
| `beginFlow` / `flowItem(w)` / `flowRight(w)` / `endFlow` | wrapping row |
| `beginTable` / `endTable` | thin row separators, `rowH` rows |
| `statusBarBegin` / `statusBarEnd` | bottom bar, 1 px rule on top |
| `tooltip(text, keyHint)` | hover text with a `[key]` hint |
| `meter(frac, width)`, `labelValue` | level bar (ok, warn, error); dim label plus value |

## Converting a file
Run `tools/lint_ui.sh` for the count of raw calls per file, convert one file, run it again. Behaviour must stay identical.
