package io.github.adswill.onair

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.widget.Toast
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import java.util.Locale
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableFloatStateOf
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.adswill.onair.model.RadioOptions
import io.github.adswill.onair.model.RadioSetting
import io.github.adswill.onair.model.SourceKind
import io.github.adswill.onair.ui.ReceiverViewModel
import kotlinx.coroutines.delay

const val DRIVER_PACKAGE = "marto.rtl_tcp_andro"

private fun openUrl(ctx: Context, vararg urls: String) {
    for (u in urls) {
        try { ctx.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(u)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)); return } catch (_: ActivityNotFoundException) {}
    }
}

private fun openDriver(ctx: Context, port: Int): Boolean = try {
    ctx.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse("iqsrc://-a 127.0.0.1 -p $port -n 1")).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)); true
} catch (_: ActivityNotFoundException) { false }

private fun rateText(hz: Double) = String.format(Locale.US, "%.3f", hz / 1e6).trimEnd('0').trimEnd('.') + " Msps"

@Composable
fun ReceiverTab(vm: ReceiverViewModel, pickFile: () -> Unit, modifier: Modifier = Modifier) {
    val ctx = LocalContext.current
    val st by vm.state.collectAsStateWithLifecycle()
    val s by vm.settings.collectAsStateWithLifecycle()
    val agcGain by vm.agcGain.collectAsStateWithLifecycle()
    val sources = listOf(R.string.source_synth, R.string.source_rtltcp, R.string.source_spy, R.string.source_file)
    val kinds = listOf(SourceKind.TEST_SIGNAL, SourceKind.RTL_TCP, SourceKind.SPYSERVER, SourceKind.IQ_FILE)
    Column(modifier.verticalScroll(rememberScrollState()).padding(horizontal = 12.dp, vertical = 4.dp)) {
        SectionLabel(stringResource(R.string.section_source))
        Dropdown(sources.map { stringResource(it) }, kinds.indexOf(s.source).coerceAtLeast(0), { vm.chooseSource(kinds[it]) })
        Spacer(Modifier.height(6.dp))
        when (s.source) {
            SourceKind.TEST_SIGNAL -> Label(stringResource(R.string.source_synth_note))
            SourceKind.RTL_TCP -> {
                Endpoint(vm, SourceKind.RTL_TCP, s.rtlHost, s.rtlPort)
                Spacer(Modifier.height(6.dp))
                Label(stringResource(R.string.rtltcp_hint))
                Spacer(Modifier.height(6.dp))
                val missing = stringResource(R.string.driver_missing)
                Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    PanelButton(stringResource(R.string.open_play_store)) { openUrl(ctx, "market://details?id=$DRIVER_PACKAGE", "https://play.google.com/store/apps/details?id=$DRIVER_PACKAGE") }
                    PanelButton(stringResource(R.string.open_fdroid)) { openUrl(ctx, "https://f-droid.org/packages/$DRIVER_PACKAGE/") }
                    PanelButton(stringResource(R.string.open_driver)) { if (!openDriver(ctx, s.rtlPort)) Toast.makeText(ctx, missing, Toast.LENGTH_SHORT).show() }
                }
            }
            SourceKind.SPYSERVER -> {
                Endpoint(vm, SourceKind.SPYSERVER, s.spyHost, s.spyPort)
                Spacer(Modifier.height(6.dp))
                Label(stringResource(R.string.spy_hint))
            }
            SourceKind.IQ_FILE -> {
                PanelButton(stringResource(R.string.choose_file), modifier = Modifier.fillMaxWidth()) { pickFile() }
                Spacer(Modifier.height(4.dp))
                Label(st.file?.name ?: stringResource(R.string.no_file), color = LocalPalette.current.text)
                Spacer(Modifier.height(6.dp))
                val fmts = listOf(R.string.format_cs8, R.string.format_cu8, R.string.format_cf32, R.string.format_ci16, R.string.format_cu16)
                Dropdown(fmts.map { stringResource(it) }, s.fileFormat.code, { vm.setFileFormat(it) }, label = stringResource(R.string.file_format))
                Spacer(Modifier.height(6.dp))
                var rateStr by remember(s.fileRateHz) { mutableStateOf(String.format(Locale.US, "%.4f", s.fileRateHz / 1e6).trimEnd('0').trimEnd('.')) }
                TextBox(rateStr, { rateStr = it; it.toDoubleOrNull()?.let { v -> if (v > 0) vm.setFileRate(v * 1e6) } }, numeric = true, label = stringResource(R.string.file_rate))
                Spacer(Modifier.height(4.dp))
                Label(stringResource(R.string.file_hint))
            }
            // USB, network list, sound card: usb/UsbDevicePicker.kt and the repository's RadioRepository (wired by the UI work, android-ui-030)
            SourceKind.USB, SourceKind.NETWORK, SourceKind.SOUND_CARD -> {}
        }

        if (s.source != SourceKind.IQ_FILE) {
            SectionLabel(stringResource(R.string.section_tuner))
            // sample rate: Auto (the mode's own) or one the source can run
            val rates = remember(s.source) {
                when (s.source) {
                    SourceKind.RTL_TCP -> listOf(0.25e6, 0.9e6, 1.024e6, 1.4e6, 1.8e6, 2.048e6, 2.4e6, 2.56e6)
                    SourceKind.SPYSERVER -> listOf(0.5e6, 1e6, 2e6, 4e6, 6e6, 10e6)
                    else -> listOf(0.5e6, 1e6, 2e6, 2.4e6, 4e6, 8e6, 10e6, 20e6)
                }
            }
            val items = listOf(stringResource(R.string.rate_auto)) + rates.map { rateText(it) }
            val sel = if (s.rateHz <= 0) 0 else rates.indexOfFirst { it == s.rateHz } + 1
            Dropdown(items, sel.coerceAtLeast(0), { i -> vm.setRate(if (i == 0) 0.0 else rates[i - 1]) }, label = stringResource(R.string.rate))
            Spacer(Modifier.height(4.dp))
            val gmax = if (st.caps.gainMax > 0) st.caps.gainMax else 50.0
            // the slider moves a local value; the gain is set when the finger lifts. With AGC on it follows the AGC's choice.
            val shown = if (s.agc && !agcGain.isNaN()) agcGain else s.gainDb
            var gain by remember(shown) { mutableFloatStateOf(shown.toFloat()) }
            SliderRow(
                stringResource(R.string.gain), gain.coerceIn(0f, gmax.toFloat()), 0f..gmax.toFloat(),
                stringResource(R.string.gain_db, String.format(Locale.US, "%.0f", gain)),
                { gain = it }, { vm.setGain(gain.toDouble(), s.agc) },
            )
            CheckRow(stringResource(R.string.agc), s.agc) { vm.setGain(gain.toDouble(), it) }
            if (st.caps.biasTee) CheckRow(stringResource(R.string.bias_tee), s.biasTee) { vm.setBiasTee(it) }
            val radio = s.radioFor(s.source)
            for (rs in st.caps.settings) SettingRow(vm, s.source, radio, rs)
        }

        SectionLabel(stringResource(R.string.section_sound))
        var vol by remember(s.volume) { mutableFloatStateOf(s.volume) }
        SliderRow(stringResource(R.string.volume), vol, 0f..1f, "${(vol * 100).toInt()} %", { vol = it }, { vm.setSound(vol, s.muted) })
        CheckRow(stringResource(R.string.mute), s.muted) { vm.setSound(vol, it) }

        SectionLabel(stringResource(R.string.section_display))
        Dropdown(Colormaps.names, s.colormap, { vm.setColormap(it) }, label = stringResource(R.string.colours))
        Spacer(Modifier.height(4.dp))
        var lo by remember(s.minDb) { mutableFloatStateOf(s.minDb) }
        var hi by remember(s.maxDb) { mutableFloatStateOf(s.maxDb) }
        SliderRow(stringResource(R.string.range_min), lo, -140f..-20f, "${lo.toInt()}", { lo = it }, { vm.setRange(lo, hi) })
        SliderRow(stringResource(R.string.range_max), hi, -80f..10f, "${hi.toInt()}", { hi = it }, { vm.setRange(lo, hi) })
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
            PanelButton(stringResource(R.string.auto)) { vm.autoRange() }
            Column(Modifier.weight(1f)) { CheckRow(stringResource(R.string.peak_hold), s.peakHold) { vm.setPeakHold(it) } }
        }
        CheckRow(stringResource(R.string.dark_theme), s.dark) { vm.setDark(it) }

        SectionLabel(stringResource(R.string.section_about))
        Label(stringResource(R.string.about_text, st.coreVersion.ifEmpty { BuildConfig.VERSION_NAME }))
        Spacer(Modifier.height(24.dp))
    }
}

/** Host and port of a network source. Typed text is UI state; it is stored (and the source asked again) a moment after typing stops. */
@Composable
private fun Endpoint(vm: ReceiverViewModel, kind: SourceKind, host: String, port: Int) {
    var h by remember(kind) { mutableStateOf(host) }
    var pt by remember(kind) { mutableStateOf(port.toString()) }
    LaunchedEffect(kind, h, pt) {
        val pn = pt.toIntOrNull() ?: return@LaunchedEffect
        if (h == host && pn == port) return@LaunchedEffect
        delay(600)
        vm.setEndpoint(kind, h.trim(), pn)
        vm.refreshCaps()
    }
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        TextBox(h, { h = it }, Modifier.weight(2f), label = stringResource(R.string.host))
        TextBox(pt, { pt = it.filter(Char::isDigit).take(5) }, Modifier.weight(1f), numeric = true, label = stringResource(R.string.port))
    }
}

@Composable
private fun SettingRow(vm: ReceiverViewModel, kind: SourceKind, radio: RadioOptions, s: RadioSetting) {
    when (s.type) {
        "bool" -> CheckRow(s.label, radio.value(s.key, s.def) == "1") { vm.setRadioSetting(s.key, if (it) "1" else "0") }
        "choice" -> {
            val cur = radio.value(s.key, s.def)
            Spacer(Modifier.height(4.dp))
            Dropdown(s.names.ifEmpty { s.values }, s.values.indexOf(cur).coerceAtLeast(0), { vm.setRadioSetting(s.key, s.values[it]) }, label = s.label)
        }
        else -> {
            var text by remember(s.key, kind) { mutableStateOf(radio.value(s.key, s.def)) }
            Spacer(Modifier.height(4.dp))
            TextBox(text, { text = it; if (it.toDoubleOrNull() != null) vm.setRadioSetting(s.key, it) }, numeric = true, label = s.label + if (s.unit.isNotEmpty()) " (" + s.unit + ")" else "")
        }
    }
}
