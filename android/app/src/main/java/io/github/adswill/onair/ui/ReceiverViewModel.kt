package io.github.adswill.onair.ui

import android.app.Application
import android.graphics.Bitmap
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import io.github.adswill.onair.AppGraph
import io.github.adswill.onair.Colormaps
import io.github.adswill.onair.data.ReceiverState
import io.github.adswill.onair.diag.AppLog
import io.github.adswill.onair.model.Demod
import io.github.adswill.onair.model.SourceKind
import io.github.adswill.onair.model.SpectrumFrame
import io.github.adswill.onair.model.Telemetry
import io.github.adswill.onair.settings.Settings
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.launch

/**
 * The UI's view of the receiver. State comes from the repository as StateFlows (collect them with collectAsStateWithLifecycle);
 * user actions go back as the intent functions below. Composables keep nothing but UI state (open dialogs, text being typed, zoom).
 * Survives rotation; there is one per activity.
 */
class ReceiverViewModel(app: Application) : AndroidViewModel(app) {
    private val repo = AppGraph.repository

    val state: StateFlow<ReceiverState> = repo.state
    val settings: StateFlow<Settings> = repo.settings
    val telemetry: StateFlow<Telemetry?> = repo.telemetry
    val agcGain: StateFlow<Double> = repo.agcGain
    val log: StateFlow<List<AppLog.Entry>> = AppLog.entries

    /** The waterfall bitmap: written on the main thread only (here), drawn by SpectrumWaterfall. */
    val waterfall = Waterfall(SpectrumFrame.BINS, 220)
    private val _frame = MutableStateFlow(SpectrumFrame.empty)
    /** The newest spectrum line, after it went into [waterfall]: the spectrum view redraws on every new value. */
    val frame: StateFlow<SpectrumFrame> = _frame.asStateFlow()

    private var lut = Colormaps.lut(settings.value.colormap)
    private var askedNotifications = false

    init {
        viewModelScope.launch { settings.distinctUntilChangedBy { it.colormap }.collect { lut = Colormaps.lut(it.colormap) } }
        viewModelScope.launch {
            var last = -1L
            repo.spectrum.collect { f ->
                if (f.seq == last || f.seq == 0L) return@collect
                last = f.seq
                waterfall.push(f.db, f.wfFloorDb, f.wfCeilDb, lut)
                _frame.value = f
            }
        }
    }

    /** True once per process: the first start asks for the notification permission (the foreground service shows one). */
    fun shouldAskNotifications(): Boolean = !askedNotifications.also { askedNotifications = true }

    // receive session
    fun toggle() = repo.toggle()
    fun selectMode(idx: Int) = repo.selectMode(idx)
    fun tune(mhz: Double) = repo.tune(mhz)
    fun clearError() = repo.clearError()
    fun clearResults() = repo.clearResults()
    fun setUiVisible(v: Boolean) = repo.setUiVisible(v)

    // source and tuner
    fun chooseSource(kind: SourceKind) = repo.chooseSource(kind)
    fun setEndpoint(kind: SourceKind, host: String, port: Int) = repo.setEndpoint(kind, host, port)
    fun refreshCaps() = repo.refreshCaps()
    fun openFile(uri: Uri) = repo.openFile(uri)
    fun setFileFormat(code: Int) = repo.updateSettings { it.copy(fileFormat = io.github.adswill.onair.model.FileFormat.of(code)) }
    fun setFileRate(hz: Double) = repo.updateSettings { it.copy(fileRateHz = hz) }
    fun setRate(hz: Double) = repo.setRate(hz)
    fun setGain(db: Double, agc: Boolean) = repo.setGain(db, agc)
    fun setBiasTee(on: Boolean) = repo.setBiasTee(on)
    fun setRadioSetting(key: String, value: String) = repo.setRadioSetting(key, value)

    // sound, general receiver
    fun setSound(volume: Float, muted: Boolean) = repo.setSound(volume, muted)
    fun setGeneral(demod: Demod, squelch: Boolean, squelchAuto: Boolean, squelchDb: Float) = repo.setGeneral(demod, squelch, squelchAuto, squelchDb)

    // display
    fun setColormap(i: Int) = repo.updateSettings { it.copy(colormap = i) }
    fun setRange(lo: Float, hi: Float) = repo.updateSettings { it.copy(minDb = lo, maxDb = hi) }
    fun autoRange() = repo.autoRange()
    fun setPeakHold(on: Boolean) = repo.setPeakHold(on)
    fun setDark(on: Boolean) = repo.updateSettings { it.copy(dark = on) }
    fun setSheetFraction(f: Float) = repo.updateSettings { it.copy(sheetFraction = f) }
    fun setMapConsent(v: Int) = repo.updateSettings { it.copy(mapConsent = v) }

    fun clearLog() = AppLog.clear()
}

/** The waterfall: a ring of rows in one bitmap, newest row first. Main thread only. */
class Waterfall(val w: Int = 1024, val h: Int = 220) {
    val bmp: Bitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888).also { it.eraseColor(0xFF000000.toInt()) }
    var top = 0
        private set
    /** Seconds per row, smoothed: the time axis of the waterfall. */
    var lineSec = 0.1f
        private set
    private val row = IntArray(w)
    private var lastNs = 0L

    fun push(db: FloatArray, minDb: Float, maxDb: Float, lut: IntArray) {
        val inv = 255f / maxOf(1f, maxDb - minDb)
        for (x in 0 until minOf(w, db.size)) {
            val v = (db[x] - minDb) * inv
            row[x] = lut[if (v <= 0f) 0 else if (v >= 255f) 255 else v.toInt()]
        }
        val now = System.nanoTime()
        if (lastNs != 0L) { val d = (now - lastNs) / 1e9f; if (d in 0.001f..5f) lineSec += (d - lineSec) * 0.1f }
        lastNs = now
        top = (top - 1 + h) % h
        bmp.setPixels(row, 0, w, 0, top, w, 1)
    }
}
