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
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import au.chrisomaney.kidsadmin.data.AdminKids
import au.chrisomaney.kidsadmin.data.AdminUiState
import au.chrisomaney.kidsadmin.data.AdminViewModel

@Composable
fun ConnectScreen(state: AdminUiState, vm: AdminViewModel) {
    val focus = LocalFocusManager.current
    val s = state.settings
    val drafts = remember {
        mutableStateMapOf<String, String>().apply {
            AdminKids.all.forEach { put(it.slug, state.pins[it.slug].orEmpty()) }
        }
    }
    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("Connect", style = MaterialTheme.typography.headlineMedium)
        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    Text(
                        state.connectionLabel,
                        color = if (state.connected) Color(0xFF34D399) else MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Box(
                        Modifier
                            .size(10.dp)
                            .clip(CircleShape)
                            .background(if (state.connected) Color(0xFF34D399) else Color(0xFFFB7185)),
                    )
                }
                state.lastError?.let {
                    Text(it, color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodySmall)
                }
            }
        }
        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Text("Mosquitto WebSockets", style = MaterialTheme.typography.titleMedium)
                OutlinedTextField(
                    value = s.broker,
                    onValueChange = { v -> vm.updateSettings { it.copy(broker = v) } },
                    label = { Text("Broker") },
                    placeholder = { Text("ws://homeassistant.local:1884") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = s.username,
                    onValueChange = { v -> vm.updateSettings { it.copy(username = v) } },
                    label = { Text("Username") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = s.password,
                    onValueChange = { v -> vm.updateSettings { it.copy(password = v) } },
                    label = { Text("Password") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                Row(
                    Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text("Remember on this phone")
                    Switch(
                        checked = s.remember,
                        onCheckedChange = { v -> vm.updateSettings { it.copy(remember = v) } },
                    )
                }
                Text(
                    "Same login as Rewards HQ / iOS Kids Admin. Cleartext ws:// is allowed for home LAN.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
        Button(
            onClick = {
                focus.clearFocus()
                vm.connect()
            },
            enabled = s.broker.isNotBlank(),
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text(if (state.connected) "Reconnect" else "Connect")
        }
        if (state.connected) {
            TextButton(onClick = { vm.disconnect() }, modifier = Modifier.fillMaxWidth()) {
                Text("Disconnect")
            }
        }
        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Text("Kid unlock PINs", style = MaterialTheme.typography.titleMedium)
                Text(
                    "4 digits opens that kid on the panel. Leave blank for no PIN.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                AdminKids.all.forEach { kid ->
                    val current = drafts[kid.slug] ?: state.pins[kid.slug].orEmpty()
                    Row(
                        Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Text(kid.name, modifier = Modifier.weight(1f))
                        OutlinedTextField(
                            value = current,
                            onValueChange = { v ->
                                drafts[kid.slug] = v.filter(Char::isDigit).take(4)
                            },
                            label = { Text("PIN") },
                            singleLine = true,
                            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.NumberPassword),
                            modifier = Modifier.weight(1f),
                        )
                        Button(
                            onClick = {
                                focus.clearFocus()
                                vm.setPin(kid.slug, drafts[kid.slug] ?: "")
                            },
                            enabled = state.connected && (current.isEmpty() || current.length == 4),
                        ) {
                            Text("Set")
                        }
                    }
                }
            }
        }
    }
}
