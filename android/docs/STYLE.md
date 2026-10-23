# OnAir for Android: style

The phone app must look and feel like the desktop program. This page maps the desktop's names (ImGui style, `pal::`, the new
interface's "instrument" palette, `typo::`) to the Compose tokens in `app/src/main/java/io/github/adswill/onair/Theme.kt`, and lists the
rules every screen and mode panel follows.

## Colours

The default is the desktop's default look: the new interface (`App::newUi = true`) with the **instrument** palette
(`ui2::loadTheme(1)` in `app/ui/ui2_theme.cpp`) and the classic accent (`pal::accent`, also the user-accent default).

| Desktop name | Value (dark) | Compose token | Use |
| --- | --- | --- | --- |
| `ui2 T.bg` | `#2A2B2C` | `LocalPalette.current.window`, `MaterialTheme.colorScheme.background` / `surface` | screen background |
| `ui2 T.rail` | `#222324` | `palette.rail`, `colorScheme.surfaceContainer` | bottom bar, navigation rail |
| `ui2 T.frame` | `#18191A` | `palette.panel`, `colorScheme.surfaceVariant` / `surfaceContainerLow` | cards, sections, tables, selected row |
| `ImGuiCol_FrameBg` | `#0E0F10` | `palette.field`, `colorScheme.surfaceContainerLowest` | text fields, drop-downs, the log |
| `ui2 T.button` | `#3C3D3F` | `palette.button`, `colorScheme.surfaceContainerHigh` | button faces |
| `ImGuiCol_Border`, `ui2 T.edge` | `#3A3C3E` | `palette.border`, `colorScheme.outline` | 1 dp rules |
| `ui2 T.text` / `ImGuiCol_Text` | `#DBDEE0` | `palette.text`, `colorScheme.onSurface` | text |
| `ui2 T.dim` / `ImGuiCol_TextDisabled` | `#999B9E` | `palette.muted`, `colorScheme.onSurfaceVariant` | labels, units, captions |
| `ui2 T.bright` | `#FFFFFF` | `palette.bright` | lit digits of the frequency readout |
| `pal::accent()` (0.45 0.72 0.82) | `#73B8D1` | `palette.accent`, `colorScheme.primary` | the one accent: selection, markers, switches |
| `ImGuiCol_ButtonActive` / `HeaderActive` | `#4D758C` | `palette.selected`, `colorScheme.primaryContainer` | pressed / selected fills, nav indicator |
| `pal::okGreen()` | `#66C285` | `palette.ok` | locked, play button |
| `pal::warnAmber()` | `#E6B347` | `palette.warn` | converging, "needs a wider radio", favourites star |
| `pal::badRed()` | `#E06157` | `palette.bad`, `colorScheme.error` | errors, stop, clipping |
| `ui2 T.series[0..7]` | see Theme.kt | `palette.series` | data series in panels; settings group bars |
| `plt::Style PlotBg` / `ui2 T.plotBg` | `#040405` | `palette.plate` | spectrum and waterfall background |
| `plt::Style Border` | `#3D4042` | `palette.plotBorder` | plot frame |
| `plt::Style Grid` (0.60 0.62 0.64, alpha 0.16) | `#29999EA3` | `palette.grid` | grid lines |
| `plt::Style Text` | `#9EA3A8` | `palette.plotText` | axis labels, chips on the plot |
| spectrum trace | `#F2F3F4` | `palette.trace` | the live trace (peak hold: accent at 55 %) |
| mode group colours (`app/ui/mode_table.cpp`, generated into `Modes.groupColors`) | per group | `groupAccent(group)` | mode chip, group headers, the selected mode's bar (the new interface's per-mode accent) |

Light theme: `LightPalette` (the same roles; the plot stays dark, as on the desktop's light theme). Dark is the default; Settings has
"Dark theme" and "Follow the system theme".

Status colours appear only where they mean a status (desktop rule in `theme.h`): never as decoration.

## Waterfall colour maps

`Colormaps.kt` is the Kotlin twin of `app/gfx/colormap.h` (`cmap::color`): the same six maps in the same order (Classic, Viridis,
Inferno, Grey, Turbo, Cividis), the same stops and the same Turbo polynomial with the smoothstep to black. A change there is a change
here. Default: Turbo (`Settings.colormap = 4`).

## Type

`typo::` (`app/ui/typography.h`) has three looks; Android has the same three (Settings > Font look, `UiPrefs.typeLook`):

| Desktop | Compose | Fonts |
| --- | --- | --- |
| Current (default) | `TypeLook.CURRENT` | JetBrains Mono for everything (the desktop uses the system monospace font; JetBrains Mono is the monospace the desktop bundles) |
| Clean | `TypeLook.CLEAN` | Inter for text, JetBrains Mono for numbers and the readout |
| Compact | `TypeLook.COMPACT` | as Clean, one size step smaller |

The fonts are the repository's own (`third_party/fonts`, SIL OFL 1.1, subset), copied to `app/src/main/res/font/`; the OFL text is
shown in About > Licences (copied from `LICENSES/OFL-1.1.txt` at build time).

Size scale (`typo::kSizes`, px at scale 1 → sp): Caption 11, Small 12, Body 13, BodyLarge 14, Title 16, Display 20, Hero 32 =
`TypeScale.*`, and in `MaterialTheme.typography`: `labelSmall` 11, `bodySmall`/`labelMedium` 12, `bodyMedium`/`labelLarge` 13,
`bodyLarge` 14, `titleMedium` 16, `titleLarge` 20, `displaySmall` 32. Text sizes are always `sp` (they follow the phone's text size);
`LocalTextFamily` gives the text family of the current look, `Mono` the monospace.

## Shapes and spacing

| Desktop | Compose |
| --- | --- |
| `FrameRounding 3`, `TabRounding 3` | `shapes.extraSmall` (3 dp): buttons, fields, chips |
| `ChildRounding 4`, `PopupRounding 4` | `shapes.small` (4 dp): sections, source rows, run button |
| sheets and dialogs | `shapes.medium`/`large`/`extraLarge` (6/8/12 dp) |
| `FrameBorderSize 1`, `ChildBorderSize 1` | 1 dp borders in `palette.border` |
| `ItemSpacing 6x3`, `WindowPadding 8x6`, `FramePadding 6x2` | `LocalSpacing.current`: xs 3, s 4, m 6, l 8, xl 12, xxl 16 dp |
| (touch) | `Spacing.touch` = 48 dp: the minimum for every tappable thing |

## Icons

`OnAirIcons` (`OnAirIcons.kt`): line icons on a 24-unit grid with 1.8-unit round strokes, the thin-rule look of the desktop's icon set;
`OnAirIcons.Glyph` is the launcher glyph (dot and two pairs of arcs). No icon font, no Material icon pack needed. Tinted by `Icon()`.

## Widgets (`Widgets.kt`)

`PanelButton`, `OnAirIconButton`, `Chip`, `CheckRow`, `SwitchRow`, `TextBox`, `Dropdown`, `SliderRow`, `Card`, `Section`,
`GroupHeader`, `SectionLabel`, `MeterBar`, `Lamp`. Use them instead of raw Material components so a screen looks like OnAir. Each has a
48 dp target and tells TalkBack its role and state.

## Rules for screens and mode panels

1. Colours from `LocalPalette` or `MaterialTheme.colorScheme`, sizes from `TypeScale`/typography, shapes from `MaterialTheme.shapes`,
   never hard-coded hex values in a screen.
2. 48 dp targets; `contentDescription` on every icon-only control; `heading()` on section titles; `stateDescription` on toggles and
   sliders.
3. No fixed heights around text (text goes to 200 %); long rows scroll or wrap; the frequency readout shrinks to fit instead.
4. Animations only through `LocalReducedMotion` (snap when it is true); no decorative motion.
5. Performance: hoist state, read fast-changing values (spectrum frames) in the draw phase, use `derivedStateOf` for values computed
   from fast flows, never allocate per frame in a draw (reuse `Path`s and bitmaps).
6. Strings in `res/values/strings.xml`, written like the desktop's: plain, short, sentence case.
