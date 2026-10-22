package io.github.adswill.onair

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.draw.clip
import androidx.compose.ui.Alignment
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.gestures.detectTransformGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.adswill.onair.model.SpectrumFrame
import io.github.adswill.onair.ui.ReceiverViewModel
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.log10
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow

/** The visible part of the capture as fractions of the span, centred on the dial frequency, for a zoom of 1 or more. */
private fun window(zoom: Float, fr: SpectrumFrame, freqMhz: Double): Pair<Float, Float> {
    val z = zoom.coerceAtLeast(1f)
    if (fr.spanHz <= 0) return 0f to 1f
    val cf = (0.5 + (freqMhz * 1e6 - fr.centreHz) / fr.spanHz).toFloat().coerceIn(0f, 1f)
    var lo = cf - 0.5f / z
    var hi = cf + 0.5f / z
    if (lo < 0f) { hi -= lo; lo = 0f }
    if (hi > 1f) { lo -= hi - 1f; hi = 1f }
    return max(0f, lo) to min(1f, hi)
}

@Composable
private fun Chip(t: String, p: Palette) {
    Text(t, Modifier.clip(RoundedCornerShape(3.dp)).background(Color(0x99000000)).border(1.dp, p.border, RoundedCornerShape(3.dp)).padding(horizontal = 6.dp, vertical = 1.dp),
        color = p.text, fontFamily = Mono, fontSize = 10.sp)
}

val PlotGrey = Color(0xFF4A4B4C)

private fun niceStep(span: Double, targetTicks: Int): Double {
    val raw = span / targetTicks
    val p = 10.0.pow(floor(log10(raw)))
    val m = raw / p
    return p * (if (m < 1.5) 1.0 else if (m < 3.5) 2.0 else if (m < 7.5) 5.0 else 10.0)
}

/** Spectrum (top) and waterfall (below), one touch surface: drag tunes, pinch zooms, a tap tunes to the touched frequency. */
@Composable
fun SpectrumWaterfall(vm: ReceiverViewModel, modifier: Modifier = Modifier) {
    val p = LocalPalette.current
    val measurer = rememberTextMeasurer()
    val wf = vm.waterfall
    val bmp = remember(wf) { wf.bmp.asImageBitmap() }
    // State objects, read where they are needed: in the draw phase a new spectrum line only redraws the canvases
    val frame = vm.frame.collectAsStateWithLifecycle()
    val st = vm.state.collectAsStateWithLifecycle()
    val settings = vm.settings.collectAsStateWithLifecycle()
    var zoom by rememberSaveable { mutableFloatStateOf(1f) }
    val label = TextStyle(color = Color(0xFFC9CDD0), fontSize = 10.sp, fontFamily = Mono)
    Column(
        modifier.background(p.plate)
            .pointerInput(Unit) {
                detectTransformGestures { _, pan, pinch, _ ->
                    if (pinch != 1f) zoom = (zoom * pinch).coerceIn(1f, 32f)
                    val fr = frame.value
                    val mhz = st.value.freqMhz
                    if (abs(pan.x) > 0f && fr.spanHz > 0) {
                        val (lo, hi) = window(zoom, fr, mhz)
                        vm.tune(mhz - pan.x / size.width * (hi - lo) * fr.spanHz / 1e6)
                    }
                }
            }
            .pointerInput(Unit) {
                detectTapGestures(onDoubleTap = { zoom = 1f }) { o ->
                    val fr = frame.value
                    if (fr.spanHz > 0) {
                        val (lo, hi) = window(zoom, fr, st.value.freqMhz)
                        vm.tune((fr.centreHz + (lo + o.x / size.width * (hi - lo) - 0.5) * fr.spanHz) / 1e6)
                    }
                }
            },
    ) {
        Box(Modifier.weight(0.5f).fillMaxWidth().background(PlotGrey)) {
            Canvas(Modifier.fillMaxSize()) {
                val fr = frame.value                    // read in the draw phase: a new line invalidates only this draw
                val s0 = st.value
                val w = size.width; val h = size.height
                val freqMhz = s0.freqMhz
                val (lo, hi) = window(zoom, fr, freqMhz)
                val lo0 = settings.value.minDb; val hi0 = settings.value.maxDb
                // the tuned channel: a light band around the dial frequency, like the desktop
                val bwHz = s0.channelHz(maxMhz = 0.5).let { if (it >= 1e3 || s0.mode.family == 5) it else 0.0 }
                if (fr.spanHz > 0 && bwHz > 0) {
                    val c = (((freqMhz * 1e6 - fr.centreHz) / fr.spanHz + 0.5 - lo) / (hi - lo) * w).toFloat()
                    val half = (bwHz / 2 / fr.spanHz / (hi - lo) * w).toFloat().coerceAtLeast(2f)
                    drawRect(Color(0x33FFFFFF), Offset(c - half, 0f), androidx.compose.ui.geometry.Size(2 * half, h))
                }
                // grid: every 20 dB, and the frequency ticks
                var db = ceil(lo0 / 20f) * 20f
                while (db <= hi0) {
                    val y = h - (db - lo0) / (hi0 - lo0) * h
                    drawLine(p.grid, Offset(0f, y), Offset(w, y), 1f)
                    drawText(measurer, "%.0f".format(db), Offset(3f, y - 12f), label)
                    db += 20f
                }
                if (fr.spanHz > 0) {
                    val vspan = (hi - lo) * fr.spanHz
                    val step = niceStep(vspan, 5)
                    val f0 = fr.centreHz + (lo - 0.5) * fr.spanHz
                    var f = ceil(f0 / step) * step
                    while (f <= f0 + vspan) {
                        val x = ((f - f0) / vspan * w).toFloat()
                        drawLine(p.grid, Offset(x, 0f), Offset(x, h), 1f)
                        if (x > 0f && x < w - 60f) drawText(measurer, "%.3f".format(f / 1e6), Offset(x + 3f, h - 14.dp.toPx()), label)
                        f += step
                    }
                    val bins = fr.bins
                    val i0 = (lo * bins).toInt().coerceIn(0, bins - 1)
                    val i1 = (hi * bins).toInt().coerceIn(i0 + 1, bins)
                    fun trace(a: FloatArray): Path {
                        val path = Path()
                        for (i in i0 until i1) {
                            val x = (i - i0).toFloat() / (i1 - i0 - 1).coerceAtLeast(1) * w
                            val y = (h - (a[i] - lo0) / (hi0 - lo0) * h).coerceIn(0f, h)
                            if (i == i0) path.moveTo(x, y) else path.lineTo(x, y)
                        }
                        return path
                    }
                    if (settings.value.peakHold) drawPath(trace(fr.peak), p.accent.copy(alpha = 0.55f), style = Stroke(1.2f))
                    drawPath(trace(fr.db), Color(0xFFF2F3F4), style = Stroke(1.2f, cap = StrokeCap.Round))
                    val xd = (((freqMhz * 1e6 - fr.centreHz) / fr.spanHz + 0.5 - lo) / (hi - lo) * w).toFloat()
                    drawLine(Color(0x99FFFFFF), Offset(xd, 0f), Offset(xd, h), 1.2f)
                }
            }
            Row(Modifier.align(Alignment.TopEnd).padding(4.dp), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                Chip(stringResource(R.string.spectrum_title), p)
                val bw = st.value.channelHz()
                if (bw > 0) Chip("BW " + (if (bw >= 1e6) "%.1f MHz".format(bw / 1e6) else "%.0f kHz".format(bw / 1e3)) + " auto", p)
                if (zoom > 1.01f) Chip("x%.1f".format(zoom), p)
            }
        }
        Box(Modifier.weight(0.5f).fillMaxWidth()) {
            Canvas(Modifier.fillMaxSize()) {
                val fr = frame.value                    // read in the draw phase: a new line invalidates only this draw
                val freqMhz = st.value.freqMhz
                val (lo, hi) = window(zoom, fr, freqMhz)
                val W = wf.w; val H = wf.h
                val x0 = (lo * W).toInt().coerceIn(0, W - 1)
                val x1 = (hi * W).toInt().coerceIn(x0 + 1, W)
                val top = wf.top
                val dw = size.width.toInt(); val dh = size.height.toInt()
                val upper = ((H - top).toFloat() / H * dh).toInt().coerceIn(0, dh)
                drawImage(bmp, IntOffset(x0, top), IntSize(x1 - x0, H - top), IntOffset(0, 0), IntSize(dw, upper), filterQuality = FilterQuality.Low)
                if (top > 0 && dh - upper > 0)
                    drawImage(bmp, IntOffset(x0, 0), IntSize(x1 - x0, top), IntOffset(0, upper), IntSize(dw, dh - upper), filterQuality = FilterQuality.Low)
                if (fr.spanHz > 0) {
                    val xd = (((freqMhz * 1e6 - fr.centreHz) / fr.spanHz + 0.5 - lo) / (hi - lo) * size.width).toFloat()
                    drawLine(p.accent.copy(alpha = 0.6f), Offset(xd, 0f), Offset(xd, size.height), 1.5f)
                }
            }
            Canvas(Modifier.fillMaxSize()) {
                frame.value                             // the time axis follows the line rate
                val total = wf.h * wf.lineSec
                val step = niceStep(total.toDouble(), 5).toFloat()
                var t = 0f
                while (t < total) {
                    val y = t / total * size.height
                    drawLine(Color(0x55FFFFFF), Offset(0f, y), Offset(10f, y), 1f)
                    drawText(measurer, if (t == 0f) "now" else "%.0f s ago".format(t), Offset(12f, (y - 6f).coerceAtLeast(0f)), label)
                    t += step
                }
            }
            if (!st.value.running) Text(
                stringResource(R.string.not_running_hint), Modifier.padding(12.dp), color = p.muted, fontFamily = Mono, fontSize = 12.sp,
            )
        }
    }
}
