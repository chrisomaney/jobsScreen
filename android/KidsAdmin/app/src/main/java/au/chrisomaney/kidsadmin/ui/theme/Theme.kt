package au.chrisomaney.kidsadmin.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val Cyan = Color(0xFF22D3EE)
private val Pink = Color(0xFFFB7185)
private val Green = Color(0xFF34D399)
private val Bg = Color(0xFF0B1220)
private val Card = Color(0xFF121C33)

private val scheme = darkColorScheme(
    primary = Cyan,
    onPrimary = Bg,
    secondary = Green,
    tertiary = Pink,
    background = Bg,
    surface = Card,
    onBackground = Color(0xFFF8FAFC),
    onSurface = Color(0xFFF8FAFC),
    onSurfaceVariant = Color(0xFF94A3B8),
    error = Pink,
)

@Composable
fun KidsAdminTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = scheme, content = content)
}
