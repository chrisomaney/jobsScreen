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
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import au.chrisomaney.kidsadmin.data.AdminKids
import au.chrisomaney.kidsadmin.data.AdminUiState
import au.chrisomaney.kidsadmin.data.AdminViewModel
import au.chrisomaney.kidsadmin.data.InternetMode
import au.chrisomaney.kidsadmin.data.KidLiveStatus
import au.chrisomaney.kidsadmin.data.UnlockStatus

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
            "Force On / Off overrides jobs. Auto waits for your Approve after required jobs are done.",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        state.pendingUnlockSlug?.let { slug ->
            val name = AdminKids.all.firstOrNull { it.slug == slug }?.name ?: slug
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    Text("$name wants internet", style = MaterialTheme.typography.titleMedium)
                    Text(
                        "Required jobs look done. Approve for today, or decline if they skipped the work.",
                        style = MaterialTheme.typography.bodySmall,
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(
                            onClick = { vm.decideUnlock(slug, true) },
                            enabled = state.connected,
                            colors = ButtonDefaults.buttonColors(
                                containerColor = Color(0xFF34D399),
                                contentColor = Color(0xFF0B1220),
                            ),
                        ) { Text("Approve") }
                        Button(
                            onClick = { vm.decideUnlock(slug, false) },
                            enabled = state.connected,
                            colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFFB7185)),
                        ) { Text("Decline") }
                        TextButton(onClick = { vm.clearBanner() }) { Text("Later") }
                    }
                }
            }
        }
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
                        buildString {
                            append("Override ${live.override.title}")
                            append(" · jobs ${if (live.jobsReady) "ready" else "locked"}")
                            if (live.unlock != UnlockStatus.None) append(" · ${live.unlock.title}")
                            append(" · ${live.pct}%")
                        },
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    if (live.unlock == UnlockStatus.Pending) {
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            Button(
                                onClick = { vm.decideUnlock(kid.slug, true) },
                                enabled = state.connected,
                            ) { Text("Approve internet") }
                            Button(
                                onClick = { vm.decideUnlock(kid.slug, false) },
                                enabled = state.connected,
                                colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFFB7185)),
                            ) { Text("Decline") }
                        }
                    }
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
