package au.chrisomaney.kidsadmin.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier.modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import au.chrisomaney.kidsadmin.data.AdminKids
import au.chrisomaney.kidsadmin.data.AdminUiState
import au.chrisomaney.kidsadmin.data.AdminViewModel
import au.chrisomaney.kidsadmin.data.InternetMode
import au.chrisomaney.kidsadmin.data.KidLiveStatus

@Composable
fun InternetScreen(state: AdminUiState, vm: AdminViewModel) {
    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("Internet", style = MaterialTheme.typography.headlineMedium)
        Text(
            "Force On / Off overrides jobs. Auto follows required jobs on the panel.",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        for (kid in AdminKids.all) {
            val live = state.status[kid.slug] ?: KidLiveStatus()
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    Row(
                        Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            Box(Modifier.size(12.dp).clip(CircleShape).background(kid.color))
                            Text(kid.name, style = MaterialTheme.typography.titleMedium)
                        }
                        Text(
                            if (live.internetOn) "ON" else "OFF",
                            color = if (live.internetOn) Color(0xFF34D399) else Color(0xFFFB7185),
                            style = MaterialTheme.typography.labelLarge,
                        )
                    }
                    Text(
                        "Override ${live.override.title} · jobs ${if (live.jobsReady) "ready" else "locked"} · ${live.pct}%",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
                        InternetMode.entries.forEachIndexed { index, mode ->
                            SegmentedButton(
                                selected = live.override == mode,
                                onClick = { if (state.connected) vm.setInternet(kid.slug, mode) },
                                shape = SegmentedButtonDefaults.itemShape(index, InternetMode.entries.size),
                                enabled = state.connected,
                            ) { Text(mode.title) }
                        }
                    }
                }
            }
        }
        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Text("Common profile", style = MaterialTheme.typography.titleMedium)
                Text(
                    "kids/common/internet · ${if (state.commonInternetOn) "ON" else "OFF"}",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(
                        onClick = { vm.setCommonInternet(true) },
                        enabled = state.connected,
                        colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF34D399), contentColor = Color(0xFF0B1220)),
                    ) { Text("Force On") }
                    Button(
                        onClick = { vm.setCommonInternet(false) },
                        enabled = state.connected,
                        colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFFB7185)),
                    ) { Text("Force Off") }
                }
            }
        }
    }
}
