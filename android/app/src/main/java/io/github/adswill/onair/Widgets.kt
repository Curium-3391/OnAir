package io.github.adswill.onair

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

private val Shape = RoundedCornerShape(3.dp)

@Composable
fun Label(text: String, modifier: Modifier = Modifier, color: Color = LocalPalette.current.muted, size: Int = 12) =
    Text(text, modifier, color = color, fontFamily = Mono, fontSize = size.sp)

@Composable
fun SectionLabel(text: String) =
    Text(text, Modifier.padding(top = 10.dp, bottom = 3.dp), color = LocalPalette.current.muted, fontFamily = Mono, fontSize = 11.sp)

@Composable
fun Lamp(color: Color, size: Int = 10) = Box(Modifier.size(size.dp).clip(CircleShape).background(color))

/** A flat instrument button: charcoal face, thin rule; [on] fills it with the accent. */
@Composable
fun PanelButton(text: String, on: Boolean = false, modifier: Modifier = Modifier, onClick: () -> Unit) {
    val p = LocalPalette.current
    Box(
        modifier.heightIn(min = 36.dp).clip(Shape).background(if (on) p.accent else p.button).border(BorderStroke(1.dp, p.border), Shape)
            .clickable(onClick = onClick).padding(horizontal = 10.dp, vertical = 6.dp),
        contentAlignment = Alignment.Center,
    ) { Text(text, color = if (on) p.plate else p.text, fontFamily = Mono, fontSize = 13.sp, maxLines = 1) }
}

@Composable
fun CheckRow(text: String, checked: Boolean, onChange: (Boolean) -> Unit) {
    val p = LocalPalette.current
    Row(Modifier.fillMaxWidth().clickable { onChange(!checked) }.heightIn(min = 40.dp), verticalAlignment = Alignment.CenterVertically) {
        Box(Modifier.size(16.dp).clip(RoundedCornerShape(2.dp)).background(if (checked) p.accent else p.field).border(1.dp, p.border, RoundedCornerShape(2.dp)))
        Spacer(Modifier.width(8.dp))
        Text(text, color = p.text, fontFamily = Mono, fontSize = 13.sp)
    }
}

@Composable
fun TextBox(value: String, onValue: (String) -> Unit, modifier: Modifier = Modifier, numeric: Boolean = false, label: String? = null) {
    val p = LocalPalette.current
    Column(modifier) {
        if (label != null) Label(label)
        BasicTextField(
            value, onValue, singleLine = true,
            textStyle = TextStyle(color = p.text, fontFamily = Mono, fontSize = 14.sp),
            cursorBrush = SolidColor(p.accent),
            keyboardOptions = KeyboardOptions(keyboardType = if (numeric) KeyboardType.Decimal else KeyboardType.Text),
            modifier = Modifier.fillMaxWidth().heightIn(min = 36.dp).clip(Shape).background(p.field).border(1.dp, p.border, Shape).padding(horizontal = 8.dp, vertical = 8.dp),
        )
    }
}

/** A closed drop-down in the instrument look: shows the chosen entry, opens a list. */
@Composable
fun Dropdown(items: List<String>, selected: Int, onSelect: (Int) -> Unit, modifier: Modifier = Modifier, label: String? = null) {
    val p = LocalPalette.current
    var open by remember { mutableStateOf(false) }
    Column(modifier) {
        if (label != null) Label(label)
        Box {
            Row(
                Modifier.fillMaxWidth().heightIn(min = 36.dp).clip(Shape).background(p.field).border(1.dp, p.border, Shape).clickable { open = true }.padding(horizontal = 8.dp),
                verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                Text(items.getOrElse(selected) { "" }, color = p.text, fontFamily = Mono, fontSize = 13.sp, maxLines = 1, modifier = Modifier.weight(1f))
                Text("v", color = p.muted, fontFamily = Mono, fontSize = 11.sp)
            }
            DropdownMenu(open, { open = false }) {
                items.forEachIndexed { i, t -> DropdownMenuItem(text = { Text(t, fontFamily = Mono, fontSize = 13.sp) }, onClick = { open = false; onSelect(i) }) }
            }
        }
    }
}

@Composable
fun SliderRow(label: String, value: Float, range: ClosedFloatingPointRange<Float>, valueText: String, onChange: (Float) -> Unit, onDone: () -> Unit = {}) {
    val p = LocalPalette.current
    Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()) {
        Label(label, Modifier.width(56.dp))
        Slider(
            value, onChange, valueRange = range, onValueChangeFinished = onDone, modifier = Modifier.weight(1f),
            colors = SliderDefaults.colors(thumbColor = p.accent, activeTrackColor = p.accent, inactiveTrackColor = p.border),
        )
        Label(valueText, Modifier.width(64.dp), color = p.text)
    }
}

@Composable
fun Card(title: String, value: String, modifier: Modifier = Modifier) {
    val p = LocalPalette.current
    Column(modifier.clip(Shape).background(p.panel).border(1.dp, p.border, Shape).padding(horizontal = 10.dp, vertical = 6.dp)) {
        Label(title, size = 11)
        Text(value, color = p.text, fontFamily = Mono, fontSize = 14.sp)
    }
}
