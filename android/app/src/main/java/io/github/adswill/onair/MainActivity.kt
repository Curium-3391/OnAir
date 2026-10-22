package io.github.adswill.onair

import android.content.Intent
import android.hardware.usb.UsbManager
import android.os.Bundle
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.snapshotFlow
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.adswill.onair.ui.ReceiverViewModel
import io.github.adswill.onair.usb.AndroidUsbBackend

/**
 * The only activity: hosts the Compose UI and the system file picker. It owns no receiver state (that is in ReceiverRepository,
 * reached through [ReceiverViewModel]), so a rotation or a re-create changes nothing; the receive session itself lives in
 * ReceiverService and goes on with the screen off.
 */
class MainActivity : ComponentActivity() {
    private val vm: ReceiverViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (savedInstanceState == null) usbAttached(intent)
        setContent {
            val settings by vm.settings.collectAsStateWithLifecycle()
            val state by vm.state.collectAsStateWithLifecycle()
            LaunchedEffect(Unit) {
                // the screen stays on only while receiving
                snapshotFlow { state.active }.collect { on ->
                    if (on) window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON) else window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                }
            }
            val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri -> if (uri != null) vm.openFile(uri) }
            OnAirTheme(settings.dark) { OnAirApp(vm) { picker.launch(arrayOf("*/*")) } }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        usbAttached(intent)
    }

    /** Opened by the USB_DEVICE_ATTACHED filter (a radio plugged in, the user picked OnAir): hand the device to the radio layer. */
    private fun usbAttached(i: Intent?) {
        if (i?.action != UsbManager.ACTION_USB_DEVICE_ATTACHED) return
        val d = AndroidUsbBackend.deviceOf(i) ?: return
        AppGraph.repository.radios.onAttachIntent(AndroidUsbBackend.ref(d))
    }

    override fun onStart() { super.onStart(); vm.setUiVisible(true) }
    override fun onStop() { vm.setUiVisible(false); super.onStop() }
}
