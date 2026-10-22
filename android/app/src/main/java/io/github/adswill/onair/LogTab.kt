package io.github.adswill.onair

import android.content.ActivityNotFoundException
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.adswill.onair.diag.AppLog
import io.github.adswill.onair.diag.CrashLog
import io.github.adswill.onair.ui.ReceiverViewModel

/** The in-app log viewer: the structured AppLog (app, engine, core, service ...), a level filter, sharing of the log and crash log. */
@Composable
fun LogTab(vm: ReceiverViewModel, modifier: Modifier = Modifier) {
    val p = LocalPalette.current
    val ctx = LocalContext.current
    val entries by vm.log.collectAsStateWithLifecycle()
    val st by vm.state.collectAsStateWithLifecycle()
    var minLevel by rememberSaveable { mutableStateOf(AppLog.Level.INFO) }
    var follow by rememberSaveable { mutableStateOf(true) }
    val shown = remember(entries, minLevel) { entries.filter { it.level >= minLevel } }
    val state = rememberLazyListState()
    LaunchedEffect(shown.size, follow) { if (follow && shown.isNotEmpty()) state.scrollToItem(shown.size - 1) }
    Column(modifier.fillMaxSize().background(p.field).padding(8.dp)) {
        Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            for (l in AppLog.Level.entries) PanelButton(l.name.lowercase(), on = l == minLevel) { minLevel = l }
            PanelButton(stringResource(R.string.log_follow), on = follow) { follow = !follow }
            PanelButton(stringResource(R.string.log_share)) {
                try { ctx.startActivity(CrashLog.shareIntent(ctx)) } catch (e: ActivityNotFoundException) { AppLog.w("app", "no app to share with", e) }
                catch (e: Exception) { AppLog.e("app", "share failed", e) }
            }
            PanelButton(stringResource(R.string.log_clear)) { vm.clearLog() }
        }
        if (st.error.isNotEmpty()) Text(stringResource(R.string.log_error, st.error), Modifier.padding(top = 6.dp), color = p.bad, fontFamily = Mono, fontSize = 12.sp)
        if (shown.isEmpty() && st.error.isEmpty()) Label(stringResource(R.string.log_empty))
        LazyColumn(state = state, modifier = Modifier.padding(top = 6.dp)) {
            items(shown) { e ->
                val c = when (e.level) { AppLog.Level.ERROR -> p.bad; AppLog.Level.WARN -> p.warn; AppLog.Level.DEBUG -> p.muted; else -> p.text }
                Text(e.format(), color = c, fontFamily = Mono, fontSize = 11.sp)
            }
        }
    }
}
