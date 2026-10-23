package io.github.adswill.onair

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringArrayResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import java.util.Locale
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.adswill.onair.ui.ReceiverViewModel

private sealed class Row2 {
    class Header(val group: Int) : Row2()
    class Mode(val def: ModeDef) : Row2()
}

/** Every mode of the desktop table, grouped like its mode list; a note where the source is too slow for the mode. */
@Composable
fun ModeTab(vm: ReceiverViewModel, onPicked: () -> Unit, modifier: Modifier = Modifier) {
    val p = LocalPalette.current
    val st by vm.state.collectAsStateWithLifecycle()
    val source = vm.settings.collectAsStateWithLifecycle().value.source
    val names = stringArrayResource(R.array.mode_names)
    val blurbs = stringArrayResource(R.array.mode_subs)
    var query by remember { mutableStateOf("") }
    val rows = remember(query) {
        val out = ArrayList<Row2>()
        for (g in 0 until Modes.GROUPS) {
            val ms = Modes.all.filter { it.group == g && (query.isBlank() || names[it.index].contains(query, true) || blurbs[it.index].contains(query, true)) }
            if (ms.isEmpty()) continue
            out.add(Row2.Header(g)); ms.forEach { out.add(Row2.Mode(it)) }
        }
        out
    }
    Column(modifier) {
        TextBox(query, { query = it }, Modifier.padding(horizontal = 12.dp, vertical = 6.dp), label = stringResource(R.string.search_modes))
        LazyColumn(Modifier.fillMaxWidth()) {
            items(rows) { row ->
                when (row) {
                    is Row2.Header -> {
                        val gc = Color(Modes.groupColors[row.group])
                        val gname = stringResource(groupName(row.group))
                        Row(Modifier.fillMaxWidth().padding(start = 12.dp, end = 12.dp, top = 12.dp, bottom = 3.dp)) {
                            Box(Modifier.width(3.dp).height(12.dp).background(gc))
                            Text(gname, Modifier.padding(start = 8.dp), color = gc, fontFamily = Mono, fontSize = 11.sp)
                        }
                    }
                    is Row2.Mode -> {
                        val m = row.def
                        val sel = st.modeIndex == m.index
                        val wide = st.needsWideRadio(m, source)
                        Row(
                            Modifier.fillMaxWidth().height(IntrinsicSize.Min).background(if (sel) p.panel else Color.Transparent).clickable { vm.selectMode(m.index); onPicked() }.padding(horizontal = 12.dp, vertical = 8.dp),
                        ) {
                            Box(Modifier.width(3.dp).fillMaxHeight().background(if (sel) Color(m.accent) else Color.Transparent))
                            Column(Modifier.padding(start = 9.dp)) {
                                Text(names[m.index], color = if (sel) Color(m.accent) else p.text, fontFamily = Mono, fontSize = 14.sp)
                                if (blurbs[m.index].isNotEmpty()) Text(blurbs[m.index], color = p.muted, fontFamily = Mono, fontSize = 11.sp)
                                if (wide) {
                                    val t = st.tuningFor(m)
                                    Text(
                                        stringResource(R.string.needs_wide_radio, String.format(Locale.US, "%.1f", (t?.minRate ?: 0.0) / 1e6)),
                                        color = p.warn, fontFamily = Mono, fontSize = 11.sp,
                                    )
                                }
                            }
                        }
                    }
                }
            }
            item { Spacer(Modifier.height(24.dp)) }
        }
    }
}

@Composable
private fun groupName(g: Int): Int = when (g) {
    0 -> R.string.group_0; 1 -> R.string.group_1; 2 -> R.string.group_2; 3 -> R.string.group_3; 4 -> R.string.group_4; 5 -> R.string.group_5
    6 -> R.string.group_6; 7 -> R.string.group_7; 8 -> R.string.group_8; 9 -> R.string.group_9; 10 -> R.string.group_10; 11 -> R.string.group_11
    12 -> R.string.group_12; else -> R.string.group_13
}
