package io.github.adswill.onair.ui.modes

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.selection.toggleable
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringArrayResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.selected
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.adswill.onair.Chip
import io.github.adswill.onair.LocalPalette
import io.github.adswill.onair.LocalSpacing
import io.github.adswill.onair.LocalTextFamily
import io.github.adswill.onair.ModeDef
import io.github.adswill.onair.Modes
import io.github.adswill.onair.OnAirIconButton
import io.github.adswill.onair.OnAirIcons
import io.github.adswill.onair.R
import io.github.adswill.onair.TextBox
import io.github.adswill.onair.TypeScale
import io.github.adswill.onair.groupAccent
import io.github.adswill.onair.model.SourceKind
import io.github.adswill.onair.ui.ReceiverViewModel
import java.util.Locale

/** The string resource of a mode group's name (group_0 .. group_13, generated with the mode list). */
fun groupNameRes(g: Int): Int = when (g) {
    0 -> R.string.group_0; 1 -> R.string.group_1; 2 -> R.string.group_2; 3 -> R.string.group_3; 4 -> R.string.group_4; 5 -> R.string.group_5
    6 -> R.string.group_6; 7 -> R.string.group_7; 8 -> R.string.group_8; 9 -> R.string.group_9; 10 -> R.string.group_10; 11 -> R.string.group_11
    12 -> R.string.group_12; else -> R.string.group_13
}

/**
 * The mode picker: every mode of the desktop table, grouped like the desktop's mode tree (collapsible groups, the group colour bar),
 * favourites first, a search over names and descriptions, and the two filters: hide the modes I do not use, only the modes my radio
 * can run. "Manage" shows a hide switch on each row.
 */
@Composable
fun ModePickerScreen(vm: ReceiverViewModel, onPicked: () -> Unit, modifier: Modifier = Modifier) {
    val st by vm.state.collectAsStateWithLifecycle()
    val s by vm.settings.collectAsStateWithLifecycle()
    val ui by vm.ui.collectAsStateWithLifecycle()
    ModePickerContent(
        selected = st.modeIndex, source = s.source, favourites = ui.favourites, hidden = ui.hidden, hideUnused = ui.hideUnused,
        onlyCapable = ui.onlyCapable,
        needsWide = { m -> st.needsWideRadio(m, s.source) }, minRateMsps = { m -> (st.tuningFor(m)?.minRate ?: 0.0) / 1e6 },
        onPick = { vm.selectMode(it); onPicked() }, onFavourite = vm::toggleFavourite, onHide = vm::setHidden,
        onHideUnused = vm::setHideUnused, onOnlyCapable = vm::setOnlyCapable, modifier = modifier,
    )
}

@Composable
fun ModePickerContent(
    selected: Int, source: SourceKind, favourites: Set<Int>, hidden: Set<Int>, hideUnused: Boolean, onlyCapable: Boolean,
    needsWide: (ModeDef) -> Boolean, minRateMsps: (ModeDef) -> Double,
    onPick: (Int) -> Unit, onFavourite: (Int) -> Unit, onHide: (Int, Boolean) -> Unit, onHideUnused: (Boolean) -> Unit, onOnlyCapable: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
) {
    val p = LocalPalette.current
    val names = stringArrayResource(R.array.mode_names).toList()
    val blurbs = stringArrayResource(R.array.mode_subs).toList()
    var query by rememberSaveable { mutableStateOf("") }
    var manage by rememberSaveable { mutableStateOf(false) }
    var collapsed by rememberSaveable { mutableStateOf(emptyList<Int>()) }
    val filter = PickerFilter(query, favourites, hidden, hideUnused && !manage, onlyCapable, collapsed.toSet())
    // ~120 modes: cheap enough to filter on every recomposition (which happens on typing and on a mode or source change, not per frame)
    val rows = ModeFilter.rows(Modes.all, Modes.GROUPS, names, blurbs, filter, selected) { !needsWide(it) }
    Column(modifier.background(p.window)) {
        Row(Modifier.padding(horizontal = 12.dp, vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
            TextBox(query, { query = it }, Modifier.weight(1f), placeholder = stringResource(R.string.search_modes))
            if (query.isNotEmpty()) OnAirIconButton(OnAirIcons.Close, stringResource(R.string.cd_clear_search), framed = false) { query = "" }
        }
        Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(horizontal = 12.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Chip(stringResource(R.string.filter_hide_unused), color = p.accent, selected = hideUnused) { onHideUnused(!hideUnused) }
            Chip(stringResource(R.string.filter_only_capable), color = p.accent, selected = onlyCapable) { onOnlyCapable(!onlyCapable) }
            Chip(stringResource(R.string.filter_manage), color = p.text, selected = manage) { manage = !manage }
        }
        if (rows.isEmpty()) Text(stringResource(R.string.no_modes_match), Modifier.padding(16.dp), color = p.muted, fontFamily = LocalTextFamily.current)
        LazyColumn(Modifier.fillMaxSize()) {
            items(rows, key = { it.key }) { row ->
                when (row) {
                    is PickerRow.Header -> GroupRow(row) {
                        collapsed = if (row.group in collapsed) collapsed - row.group else collapsed + row.group
                    }
                    is PickerRow.Mode -> ModeRow(
                        row.def, names.getOrElse(row.def.index) { "" }, blurbs.getOrElse(row.def.index) { "" }, selected == row.def.index,
                        favourite = row.def.index in favourites, hidden = row.def.index in hidden, manage = manage,
                        wideNote = if (needsWide(row.def)) stringResource(R.string.needs_wide_radio, String.format(Locale.US, "%.1f", minRateMsps(row.def))) else null,
                        onPick = { onPick(row.def.index) }, onFavourite = { onFavourite(row.def.index) }, onHide = { onHide(row.def.index, it) },
                    )
                }
            }
        }
    }
}

@Composable
private fun GroupRow(h: PickerRow.Header, onToggle: () -> Unit) {
    val p = LocalPalette.current
    val c = if (h.group < 0) p.warn else groupAccent(h.group)
    val name = if (h.group < 0) stringResource(R.string.favourites) else stringResource(groupNameRes(h.group))
    val state = stringResource(if (h.collapsed) R.string.cd_collapsed else R.string.cd_expanded)
    Row(
        Modifier.fillMaxWidth().heightIn(min = LocalSpacing.current.touch).clickable(role = Role.Button, onClick = onToggle)
            .semantics(mergeDescendants = true) { heading(); stateDescription = state }.padding(horizontal = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.width(3.dp).height(14.dp).background(c))
        Text(name, Modifier.padding(start = 8.dp).weight(1f), color = c, fontFamily = LocalTextFamily.current, fontSize = TypeScale.SMALL.sp, fontWeight = FontWeight.Medium)
        Text("${h.count}", color = p.muted, fontFamily = LocalTextFamily.current, fontSize = TypeScale.CAPTION.sp)
        Icon(if (h.collapsed) OnAirIcons.ChevronRight else OnAirIcons.ChevronDown, contentDescription = null, tint = p.muted, modifier = Modifier.padding(start = 6.dp).size(18.dp))
    }
}

@Composable
private fun ModeRow(
    m: ModeDef, name: String, blurb: String, sel: Boolean, favourite: Boolean, hidden: Boolean, manage: Boolean, wideNote: String?,
    onPick: () -> Unit, onFavourite: () -> Unit, onHide: (Boolean) -> Unit,
) {
    val p = LocalPalette.current
    val accent = groupAccent(m.group)
    Row(
        Modifier.fillMaxWidth().height(IntrinsicSize.Min).background(if (sel) p.panel else Color.Transparent),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.width(3.dp).fillMaxHeight().background(if (sel) accent else Color.Transparent))
        Column(
            Modifier.weight(1f).heightIn(min = 56.dp).clickable(role = Role.Button, onClick = onPick).semantics { selected = sel }
                .padding(start = 12.dp, top = 6.dp, bottom = 6.dp),
            verticalArrangement = Arrangement.Center,
        ) {
            Text(name, color = if (sel) accent else if (hidden) p.muted else p.text, fontFamily = LocalTextFamily.current, fontSize = TypeScale.BODY_LARGE.sp)
            if (blurb.isNotEmpty()) Text(blurb, color = p.muted, fontFamily = LocalTextFamily.current, fontSize = TypeScale.SMALL.sp)
            if (wideNote != null) Text(wideNote, color = p.warn, fontFamily = LocalTextFamily.current, fontSize = TypeScale.SMALL.sp)
        }
        if (manage) {
            val hideDesc = stringResource(R.string.cd_hide_mode, name)
            Box(
                Modifier.heightIn(min = LocalSpacing.current.touch).width(56.dp).toggleable(hidden, role = Role.Switch, onValueChange = onHide)
                    .semantics { stateDescription = hideDesc },
                contentAlignment = Alignment.Center,
            ) { Icon(OnAirIcons.Hide, contentDescription = hideDesc, tint = if (hidden) p.warn else p.border) }
        }
        OnAirIconButton(
            if (favourite) OnAirIcons.Star else OnAirIcons.StarOutline,
            stringResource(if (favourite) R.string.cd_unfavourite else R.string.cd_favourite, name),
            tint = if (favourite) p.warn else p.muted, framed = false, onClick = onFavourite,
        )
    }
}
