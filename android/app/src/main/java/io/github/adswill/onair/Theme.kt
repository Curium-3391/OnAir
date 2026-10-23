package io.github.adswill.onair

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.sp

/** The instrument palette of the desktop (app/theme.h, the pal:: namespace) and of the browser UI. */
class Palette(
    val window: Color, val plate: Color, val panel: Color, val border: Color, val field: Color, val button: Color,
    val text: Color, val muted: Color, val accent: Color, val ok: Color, val warn: Color, val bad: Color, val trace: Color, val grid: Color,
)

val DarkPalette = Palette(
    window = Color(0xFF2B2C2D), plate = Color(0xFF050606), panel = Color(0xFF0E0F10), border = Color(0xFF3A3C3E), field = Color(0xFF090A0A),
    button = Color(0xFF3B3C3E), text = Color(0xFFD1D6D9), muted = Color(0xFF8C9095), accent = Color(0xFF73B8D1),
    ok = Color(0xFF66C285), warn = Color(0xFFE6B347), bad = Color(0xFFE06157), trace = Color(0xFFE3E5E6), grid = Color(0x29999EA3),
)
val LightPalette = Palette(
    window = Color(0xFFE4E6E8), plate = Color(0xFF14171A), panel = Color(0xFFF4F5F6), border = Color(0xFFB8BCC0), field = Color(0xFFFFFFFF),
    button = Color(0xFFD2D5D8), text = Color(0xFF1B1E20), muted = Color(0xFF5E6368), accent = Color(0xFF1F7A99),
    ok = Color(0xFF2E8B57), warn = Color(0xFFB07A00), bad = Color(0xFFC0392B), trace = Color(0xFFE3E5E6), grid = Color(0x29999EA3),
)

val LocalPalette = compositionLocalOf { DarkPalette }
val Mono = FontFamily.Monospace

private fun scheme(p: Palette, dark: Boolean) =
    (if (dark) darkColorScheme() else lightColorScheme()).copy(
        background = p.window, surface = p.window, surfaceVariant = p.panel, primary = p.accent, onPrimary = p.plate,
        onBackground = p.text, onSurface = p.text, onSurfaceVariant = p.muted, outline = p.border, secondary = p.accent,
    )

private fun type(p: Palette) = Typography(
    bodyLarge = TextStyle(fontFamily = Mono, fontSize = 14.sp, color = p.text),
    bodyMedium = TextStyle(fontFamily = Mono, fontSize = 13.sp, color = p.text),
    bodySmall = TextStyle(fontFamily = Mono, fontSize = 12.sp, color = p.muted),
    labelLarge = TextStyle(fontFamily = Mono, fontSize = 13.sp, color = p.text),
    labelMedium = TextStyle(fontFamily = Mono, fontSize = 12.sp, color = p.text),
    labelSmall = TextStyle(fontFamily = Mono, fontSize = 11.sp, color = p.muted),
    titleMedium = TextStyle(fontFamily = Mono, fontSize = 14.sp, color = p.text),
)

@Composable
fun OnAirTheme(dark: Boolean = true, content: @Composable () -> Unit) {
    val p = if (dark) DarkPalette else LightPalette
    androidx.compose.runtime.CompositionLocalProvider(LocalPalette provides p) {
        MaterialTheme(colorScheme = scheme(p, dark), typography = type(p), content = content)
    }
}
