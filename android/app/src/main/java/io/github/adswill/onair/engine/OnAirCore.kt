package io.github.adswill.onair.engine

import io.github.adswill.onair.OnAirNative
import io.github.adswill.onair.model.DeviceCaps
import io.github.adswill.onair.model.Demod
import io.github.adswill.onair.model.ModeTable
import io.github.adswill.onair.model.SourceKind
import io.github.adswill.onair.model.StartConfig
import io.github.adswill.onair.model.Telemetry
import io.github.adswill.onair.model.Tuning

/**
 * The JNI API ([OnAirNative]) with typed arguments and results. Pure translation, no state, no threads.
 *
 * THREADING RULE: call these only from the receiver thread, i.e. inside [ReceiverEngine.call] / [ReceiverEngine.post] blocks. The C++
 * bridge has one engine per process and a control mutex: a call from the main thread could block it for seconds (nativeStart waits
 * for a network radio). Every new native entry point gets a typed method here and is called the same way.
 */
class OnAirCore internal constructor() {
    val libraryLoaded: Boolean get() = OnAirNative.loaded

    fun init(resultsDir: String): Boolean = OnAirNative.nativeInit(resultsDir)
    fun modeTable(): ModeTable = ModeTable.parse(OnAirNative.nativeModeTable())
    fun setMode(family: Int, subId: String?): Tuning = Tuning.parse(OnAirNative.nativeSetMode(family, subId ?: ""))
    fun deviceInfo(kind: SourceKind, host: String, port: Int): DeviceCaps = DeviceCaps.parse(OnAirNative.nativeDeviceInfo(kind.code, host, port))

    /** Starts the source and the receiver; "" when it runs, else the reason it did not start. Blocks while a network radio answers. */
    fun start(c: StartConfig): String = OnAirNative.nativeStart(
        c.source.code, c.host, c.port, c.filePath, c.fileFormat.code, c.fileRate, c.freqHz, c.rateHz, c.gainDb, c.agc, c.biasTee, c.radio.encoded,
    )

    fun stop() = OnAirNative.nativeStop()
    fun running(): Boolean = OnAirNative.nativeRunning()
    fun note(): String = OnAirNative.nativeNote()
    fun retune(hz: Double) = OnAirNative.nativeRetune(hz)
    fun setGain(db: Double, agc: Boolean) = OnAirNative.nativeSetGain(db, agc)
    fun setRadio(radio: String, biasTee: Boolean) = OnAirNative.nativeSetRadio(radio, biasTee)

    /** Fills [out] (dB per bin) and [meta] (centre Hz, span Hz, rms dB, clip fraction); false when there is no new line. */
    fun spectrum(out: FloatArray, meta: DoubleArray): Boolean = OnAirNative.nativeSpectrum(out, meta)
    fun gainNow(): Double = OnAirNative.nativeGainNow()

    /** null when nothing changed since the last call (unless [force]). */
    fun telemetry(force: Boolean): Telemetry? = OnAirNative.nativeTelemetry(force).takeIf { it.isNotEmpty() }?.let { Telemetry.parse(it) }

    /** New lines of the C++ log since [since]; returns the new total and the lines. */
    fun log(since: Long): Pair<Long, List<String>> {
        val parts = OnAirNative.nativeLog(since).split('\n')
        val total = parts.firstOrNull()?.toLongOrNull() ?: since
        return total to parts.drop(1).filter { it.isNotEmpty() }
    }

    fun clearResults() = OnAirNative.nativeClearResults()
    fun setVolume(v: Float, mute: Boolean) = OnAirNative.nativeSetVolume(v, mute)
    fun general(demod: Demod, widthHz: Double, squelch: Boolean, squelchAuto: Boolean, squelchDb: Float) =
        OnAirNative.nativeGeneral(demod.ordinal, widthHz, squelch, squelchAuto, squelchDb)
    fun fmDeemphasis(us: Double) = OnAirNative.nativeFmDeemphasis(us)
    fun version(): String = OnAirNative.nativeVersion()
}
