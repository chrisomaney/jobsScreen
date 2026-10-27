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
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AddCircle
import androidx.compose.material.icons.filled.RemoveCircle
import androidx.compose.material3.Card
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier.modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import au.chrisomaney.kidsadmin.data.AdminKids
import au.chrisomaney.kidsadmin.data.AdminUiState
import au.chrisomaney.kidsadmin.data.AdminViewModel

@Composable
fun PointsScreen(state: AdminUiState, vm: AdminViewModel) {
    var amount by rememberSaveable { mutableStateOf("1") }
    val focus = LocalFocusManager.current
    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("Points", style = MaterialTheme.typography.headlineMedium)
        OutlinedTextField(
            value = amount,
            onValueChange = { amount = it.filter { ch -> ch.isDigit() } },
            label = { Text("Amount") },
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
            supportingText = {
                Text("Tallies never go below 0. Tap outside or Back to close the keyboard.")
            },
        )
        for (kid in AdminKids.all) {
            val tally = state.status[kid.slug]?.tally ?: 0
            Card(Modifier.fillMaxWidth()) {
                Row(
                    Modifier.padding(12.dp).fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    Box(Modifier.size(12.dp).clip(CircleShape).background(kid.color))
                    Text(kid.name, modifier = Modifier.weight(1f))
                    Text(
                        "$tally",
                        style = MaterialTheme.typography.titleLarge,
                        color = Color(0xFF22D3EE),
                    )
                    IconButton(
                        onClick = {
                            focus.clearFocus()
                            vm.adjustPoints(kid.slug, (amount.toIntOrNull() ?: 1).coerceAtLeast(1))
                        },
                        enabled = state.connected,
                    ) {
                        Icon(Icons.Default.AddCircle, contentDescription = "Add", tint = Color(0xFF34D399))
                    }
                    IconButton(
                        onClick = {
                            focus.clearFocus()
                            vm.adjustPoints(kid.slug, -(amount.toIntOrNull() ?: 1).coerceAtLeast(1))
                        },
                        enabled = state.connected,
                    ) {
                        Icon(Icons.Default.RemoveCircle, contentDescription = "Remove", tint = Color(0xFFFB7185))
                    }
                }
            }
        }
    }
}
