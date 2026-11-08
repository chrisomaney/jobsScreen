package au.chrisomaney.kidsadmin.data

import androidx.compose.ui.graphics.Color

data class KidInfo(
    val slug: String,
    val name: String,
    val color: Color,
)

enum class InternetMode(val raw: String, val title: String) {
    Auto("auto", "Auto"),
    On("on", "On"),
    Off("off", "Off");

    companion object {
        fun from(raw: String?): InternetMode =
            entries.firstOrNull { it.raw == raw } ?: Auto
    }
}

data class KidLiveStatus(
    val tally: Int = 0,
    val internetOn: Boolean = false,
    val jobsReady: Boolean = false,
    val override: InternetMode = InternetMode.Auto,
    val pct: Int = 0,
)

data class JobItem(
    val id: String,
    val label: String,
    val cat: String,
    val required: Boolean,
    val custom: Boolean,
    val detail: String,
    val kids: List<String>,
)

object AdminKids {
    val all = listOf(
        KidInfo("xavier", "Xavier", Color(0xFF3B82F6)),
        KidInfo("presley", "Presley", Color(0xFFEC4899)),
        KidInfo("jax", "Jax", Color(0xFFF59E0B)),
        KidInfo("banx", "Banx", Color(0xFF22C55E)),
        KidInfo("hazel", "Hazel", Color(0xFFA855F7)),
        KidInfo("woomba", "Woomba", Color(0xFF06B6D4)),
        KidInfo("wilma", "Wilma", Color(0xFFF43F5E)),
    )
}

data class ConnectionSettings(
    val broker: String = "ws://homeassistant.local:1884",
    val username: String = "",
    val password: String = "",
    val remember: Boolean = true,
)
