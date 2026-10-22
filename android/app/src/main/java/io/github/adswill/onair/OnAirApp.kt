package io.github.adswill.onair

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectVerticalDragGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.res.stringArrayResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.adswill.onair.data.Phase
import io.github.adswill.onair.model.LockState
import io.github.adswill.onair.ui.AppPermission
import io.github.adswill.onair.ui.ReceiverViewModel
import io.github.adswill.onair.ui.rememberPermissionRequest
import java.util.Locale
import kotlin.math.roundToLong

private fun freqText(mhz: Double, dim: Color, bright: Color): AnnotatedString {
    val hz = (mhz * 1e6).roundToLong().coerceAtLeast(0)
    val digits = String.format(Locale.US, "%012d", hz)
    val s = digits.chunked(3).joinToString(".")
    val firstNonZero = digits.indexOfFirst { it != '0' }.let { if (it < 0) 11 else minOf(it, 8) }   // keep the MHz digits and below bright
    return buildAnnotatedString {
        var d = 0
        for (c in s) {
            if (c == '.') { withStyle(SpanStyle(color = if (d <= firstNonZero) dim else bright)) { append(c) }; continue }
            withStyle(SpanStyle(color = if (d < firstNonZero) dim else bright)) { append(c) }
            d++
        }
    }
}

@Composable
fun OnAirApp(vm: ReceiverViewModel, pickFile: () -> Unit) {
    val p = LocalPalette.current
    val ctx = LocalContext.current
    val st by vm.state.collectAsStateWithLifecycle()
    val settings by vm.settings.collectAsStateWithLifecycle()
    val ask = rememberPermissionRequest(ctx)
    // the play button: the first start asks for the notification permission (the foreground service shows one); it starts either way
    val toggle = { if (!st.active && vm.shouldAskNotifications()) ask(listOf(AppPermission.NOTIFICATIONS)) { vm.toggle() } else vm.toggle() }
    var tab by rememberSaveable { mutableIntStateOf(0) }
    var fraction by remember(settings.sheetFraction) { mutableFloatStateOf(settings.sheetFraction) }
    BoxWithConstraints(Modifier.fillMaxSize().background(p.window).windowInsetsPadding(WindowInsets.safeDrawing)) {
        val wide = maxWidth > maxHeight && maxWidth >= 600.dp
        Column(Modifier.fillMaxSize()) {
            TopBar(vm, compact = wide, onModeClick = { tab = 1 }, onToggle = toggle)
            if (st.error.isNotEmpty()) Text(st.error, Modifier.fillMaxWidth().background(p.window).clickable { vm.clearError(); tab = 3 }.padding(horizontal = 12.dp, vertical = 6.dp),
                color = p.bad, fontFamily = Mono, fontSize = 12.sp, maxLines = 3)
            if (wide) {
                Row(Modifier.weight(1f).fillMaxWidth()) {
                    SpectrumWaterfall(vm, Modifier.weight(0.56f).fillMaxHeight())
                    Column(Modifier.weight(0.44f).fillMaxHeight()) { Panel(vm, tab, { tab = it }, pickFile) }
                }
            } else {
                BoxWithConstraints(Modifier.weight(1f).fillMaxWidth()) {
                    val total = constraints.maxHeight.toFloat()
                    Column(Modifier.fillMaxSize()) {
                        SpectrumWaterfall(vm, Modifier.weight(1f - fraction.coerceIn(0.2f, 0.75f)).fillMaxWidth())
                        Box(
                            Modifier.fillMaxWidth().height(24.dp).background(p.window).pointerInput(total) {
                                detectVerticalDragGestures(onDragEnd = { vm.setSheetFraction(fraction) }) { _, dy -> fraction = (fraction - dy / total).coerceIn(0.2f, 0.75f) }
                            },
                            contentAlignment = Alignment.Center,
                        ) { Box(Modifier.width(40.dp).height(4.dp).clip(RoundedCornerShape(2.dp)).background(p.border)) }
                        Column(Modifier.weight(fraction.coerceIn(0.2f, 0.75f)).fillMaxWidth()) { Panel(vm, tab, { tab = it }, pickFile) }
                    }
                }
            }
        }
    }
}

@Composable
private fun ColumnScope.Panel(vm: ReceiverViewModel, tab: Int, onTab: (Int) -> Unit, pickFile: () -> Unit) {
    val p = LocalPalette.current
    val tabs = listOf(R.string.tab_receiver, R.string.tab_mode, R.string.tab_results, R.string.tab_log)
    Row(Modifier.fillMaxWidth().background(p.window)) {
        tabs.forEachIndexed { i, t ->
            Column(Modifier.weight(1f).heightIn(min = 48.dp).clickable { onTab(i) }.padding(top = 8.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                Text(stringResource(t), color = if (i == tab) p.text else p.muted, fontFamily = Mono, fontSize = 13.sp)
                Box(Modifier.padding(top = 6.dp).fillMaxWidth().height(2.dp).background(if (i == tab) p.accent else Color.Transparent))
            }
        }
    }
    Box(Modifier.fillMaxWidth().weight(1f).background(p.window)) {
        when (tab) {
            0 -> ReceiverTab(vm, pickFile, Modifier.fillMaxSize())
            1 -> ModeTab(vm, { onTab(0) }, Modifier.fillMaxSize())
            2 -> ResultsTab(vm, Modifier.fillMaxSize())
            else -> LogTab(vm, Modifier.fillMaxSize())
        }
    }
}

@Composable
private fun TopBar(vm: ReceiverViewModel, compact: Boolean, onModeClick: () -> Unit, onToggle: () -> Unit) {
    val p = LocalPalette.current
    val names = stringArrayResource(R.array.mode_names)
    var editing by remember { mutableStateOf(false) }
    val st by vm.state.collectAsStateWithLifecycle()
    val t by vm.telemetry.collectAsStateWithLifecycle()
    val running = st.running
    val lamp = when {
        !running -> p.border
        t?.lock == LockState.LOCKED -> p.ok
        t?.lock == LockState.CONVERGING -> p.warn
        else -> p.warn.copy(alpha = 0.55f)
    }
    val stateText = when {
        st.phase == Phase.STARTING -> R.string.state_starting
        st.phase == Phase.STOPPING -> R.string.state_stopping
        !running -> R.string.state_off
        t?.lock == LockState.LOCKED -> R.string.state_locked
        t?.lock == LockState.CONVERGING -> R.string.state_converging
        else -> R.string.state_searching
    }
    Row(Modifier.fillMaxWidth().background(p.window).padding(horizontal = 12.dp, vertical = 8.dp), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Row(verticalAlignment = Alignment.Bottom) {
                Text(
                    freqText(st.freqMhz, p.muted.copy(alpha = 0.5f), p.text), fontFamily = Mono, fontSize = if (compact) 26.sp else 30.sp,
                    modifier = Modifier.clickable { editing = true },
                )
                Text(" " + stringResource(R.string.hz), color = p.muted, fontFamily = Mono, fontSize = 11.sp, modifier = Modifier.padding(bottom = 5.dp))
            }
            Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.padding(top = 4.dp)) {
                val m = st.mode
                Text(
                    names[st.modeIndex], color = Color(m.accent), fontFamily = Mono, fontSize = 13.sp,
                    modifier = Modifier.clip(RoundedCornerShape(3.dp)).border(1.dp, Color(m.accent).copy(alpha = 0.6f), RoundedCornerShape(3.dp)).clickable(onClick = onModeClick).padding(horizontal = 8.dp, vertical = 3.dp),
                )
                Box(Modifier.padding(start = 10.dp)) { Lamp(lamp, 10) }
                Text(" " + stringResource(stateText), color = p.muted, fontFamily = Mono, fontSize = 12.sp)
                t?.snrDb?.let { if (running) Text("  " + stringResource(R.string.snr_db, String.format(Locale.US, "%.1f", it)), color = p.muted, fontFamily = Mono, fontSize = 12.sp) }
            }
        }
        Box(
            Modifier.size(52.dp).clip(RoundedCornerShape(4.dp)).background(p.button).border(1.dp, p.border, RoundedCornerShape(4.dp)).clickable(onClick = onToggle),
            contentAlignment = Alignment.Center,
        ) {
            Text(if (st.active) "■" else "▶", color = if (st.active) p.bad else p.ok, fontSize = 22.sp)
        }
    }
    if (editing) {
        var text by remember { mutableStateOf(String.format(Locale.US, "%.6f", st.freqMhz).trimEnd('0').trimEnd('.')) }
        AlertDialog(
            onDismissRequest = { editing = false },
            title = { Text(stringResource(R.string.enter_frequency), fontFamily = Mono) },
            text = { TextBox(text, { text = it }, numeric = true, label = stringResource(R.string.frequency_mhz)) },
            confirmButton = { TextButton({ text.replace(',', '.').toDoubleOrNull()?.let { vm.tune(it) }; editing = false }) { Text(stringResource(R.string.ok)) } },
            dismissButton = { TextButton({ editing = false }) { Text(stringResource(R.string.cancel)) } },
        )
    }
}
