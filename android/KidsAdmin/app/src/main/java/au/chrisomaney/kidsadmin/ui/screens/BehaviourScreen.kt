package au.chrisomaney.kidsadmin.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier.modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import au.chrisomaney.kidsadmin.data.AdminKids
import au.chrisomaney.kidsadmin.data.AdminUiState
import au.chrisomaney.kidsadmin.data.AdminViewModel
import androidx.compose.foundation.layout.Row
import androidx.compose.ui.Alignment

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun BehaviourScreen(state: AdminUiState, vm: AdminViewModel) {
    var kid by rememberSaveable { mutableStateOf(AdminKids.all.first().slug) }
    var points by rememberSaveable { mutableStateOf("1") }
    var title by rememberSaveable { mutableStateOf("Great behaviour!") }
    var message by rememberSaveable { mutableStateOf("") }
    var seconds by rememberSaveable { mutableStateOf("15") }
    var panel by rememberSaveable { mutableStateOf(true) }
    var tv by rememberSaveable { mutableStateOf(true) }
    var expanded by rememberSaveable { mutableStateOf(false) }
    val focus = LocalFocusManager.current

    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("Behaviour", style = MaterialTheme.typography.headlineMedium)
        state.banner?.let {
            Text(it, color = Color(0xFF34D399))
        }
        ExposedDropdownMenuBox(expanded = expanded, onExpandedChange = { expanded = it }) {
            OutlinedTextField(
                value = AdminKids.all.first { it.slug == kid }.name,
                onValueChange = {},
                readOnly = true,
                label = { Text("Kid") },
                trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded) },
                modifier = Modifier.menuAnchor().fillMaxWidth(),
            )
            ExposedDropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                AdminKids.all.forEach { k ->
                    DropdownMenuItem(
                        text = { Text(k.name) },
                        onClick = {
                            kid = k.slug
                            expanded = false
                        },
                    )
                }
            }
        }
        OutlinedTextField(
            value = points,
            onValueChange = { points = it.filter { ch -> ch.isDigit() || ch == '-' } },
            label = { Text("Points (+ / −)") },
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )
        OutlinedTextField(
            value = title,
            onValueChange = { title = it },
            label = { Text("Title") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )
        OutlinedTextField(
            value = message,
            onValueChange = { message = it },
            label = { Text("Message for panel & TV") },
            minLines = 3,
            modifier = Modifier.fillMaxWidth(),
        )
        OutlinedTextField(
            value = seconds,
            onValueChange = { seconds = it.filter { ch -> ch.isDigit() } },
            label = { Text("Seconds on panel") },
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween) {
            Text("Show on panel")
            Switch(checked = panel, onCheckedChange = { panel = it })
        }
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween) {
            Text("Show on TV")
            Switch(checked = tv, onCheckedChange = { tv = it })
        }
        Button(
            onClick = {
                focus.clearFocus()
                val n = (points.toIntOrNull() ?: 0).let { if (it == 0) 0 else kotlin.math.abs(it) }
                if (n == 0) {
                    vm.clearBanner()
                    return@Button
                }
                if (message.isBlank()) return@Button
                vm.sendBehaviour(kid, n, title.trim(), message.trim(), panel, tv, (seconds.toIntOrNull() ?: 15).coerceAtLeast(3))
            },
            enabled = state.connected,
            modifier = Modifier.fillMaxWidth(),
            colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF34D399), contentColor = Color(0xFF0B1220)),
        ) { Text("Reward (+)") }
        Button(
            onClick = {
                focus.clearFocus()
                val n = (points.toIntOrNull() ?: 0).let { if (it == 0) 0 else -kotlin.math.abs(it) }
                if (n == 0 || message.isBlank()) return@Button
                vm.sendBehaviour(kid, n, title.trim().ifBlank { "Behaviour note" }, message.trim(), panel, tv, (seconds.toIntOrNull() ?: 15).coerceAtLeast(3))
            },
            enabled = state.connected,
            modifier = Modifier.fillMaxWidth(),
            colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFFB7185)),
        ) { Text("Penalty (−)") }
    }
}
