package au.chrisomaney.kidsadmin.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.foundation.layout.width
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier.modifier
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import au.chrisomaney.kidsadmin.data.AdminKids
import au.chrisomaney.kidsadmin.data.AdminUiState
import au.chrisomaney.kidsadmin.data.AdminViewModel
import au.chrisomaney.kidsadmin.data.JobItem

@Composable
fun JobsScreen(state: AdminUiState, vm: AdminViewModel) {
    var addCat by rememberSaveable { mutableStateOf("expectation") }
    var newLabel by rememberSaveable { mutableStateOf("") }
    var newDetail by rememberSaveable { mutableStateOf("") }
    var newRequired by rememberSaveable { mutableStateOf(true) }
    var newPts by rememberSaveable { mutableStateOf("1") }
    var editingId by rememberSaveable { mutableStateOf<String?>(null) }
    val focus = LocalFocusManager.current

    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("Jobs", style = MaterialTheme.typography.headlineMedium)
        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Expectations bundle: ${state.expBundle} pts", style = MaterialTheme.typography.titleMedium)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = { vm.setExpBundle(state.expBundle - 1) }) { Text("−") }
                    Button(onClick = { vm.setExpBundle(state.expBundle + 1) }) { Text("+") }
                    Button(
                        onClick = {
                            focus.clearFocus()
                            vm.savePoints()
                        },
                        enabled = state.connected,
                    ) { Text("Save points") }
                }
                Text(
                    "Kids earn the bundle only when all assigned expectations are done.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }

        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Add", style = MaterialTheme.typography.titleMedium)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    FilterChip(
                        selected = addCat == "expectation",
                        onClick = { addCat = "expectation" },
                        label = { Text("Expectation") },
                    )
                    FilterChip(
                        selected = addCat == "paid",
                        onClick = { addCat = "paid" },
                        label = { Text("Paid job") },
                    )
                }
                OutlinedTextField(
                    value = newLabel,
                    onValueChange = { newLabel = it },
                    label = { Text("Name") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = newDetail,
                    onValueChange = { newDetail = it },
                    label = { Text("Hint (optional)") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                if (addCat == "paid") {
                    OutlinedTextField(
                        value = newPts,
                        onValueChange = { newPts = it.filter { ch -> ch.isDigit() } },
                        label = { Text("Points") },
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
                Row(
                    Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text("Needed for internet")
                    Switch(checked = newRequired, onCheckedChange = { newRequired = it })
                }
                Button(
                    onClick = {
                        focus.clearFocus()
                        val label = newLabel.trim()
                        if (label.isEmpty()) return@Button
                        vm.addItem(
                            label = label,
                            cat = addCat,
                            detail = newDetail.trim(),
                            required = newRequired,
                            points = if (addCat == "paid") (newPts.toIntOrNull() ?: 0).coerceAtLeast(0) else null,
                        )
                        newLabel = ""
                        newDetail = ""
                        newPts = "1"
                        newRequired = true
                    },
                    enabled = state.connected && (state.customSlotsFree ?: 1) != 0,
                    modifier = Modifier.fillMaxWidth(),
                ) { Text("Add to panel") }
                state.customSlotsFree?.let { free ->
                    Text(
                        if (free == 0) "No free custom slots" else "$free custom slot(s) free",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        }

        Text("Expectations", style = MaterialTheme.typography.titleLarge)
        state.jobs.filter { it.cat != "paid" }.forEach { job ->
            JobCard(
                job = job,
                points = null,
                editing = editingId == job.id,
                connected = state.connected,
                onToggleEdit = { editingId = if (editingId == job.id) null else job.id },
                onSave = {
                    focus.clearFocus()
                    vm.saveJob(it)
                    vm.setJobPoints(it.id, 0)
                    vm.savePoints()
                    editingId = null
                },
                onRemove = {
                    focus.clearFocus()
                    vm.removeJob(job.id)
                    editingId = null
                },
            )
        }

        Text("Jobs (paid)", style = MaterialTheme.typography.titleLarge)
        state.jobs.filter { it.cat == "paid" }.forEach { job ->
            JobCard(
                job = job,
                points = state.jobPoints[job.id] ?: 1,
                editing = editingId == job.id,
                connected = state.connected,
                onPoints = { vm.setJobPoints(job.id, it) },
                onToggleEdit = { editingId = if (editingId == job.id) null else job.id },
                onSave = {
                    focus.clearFocus()
                    vm.saveJob(it)
                    vm.savePoints()
                    editingId = null
                },
                onRemove = {
                    focus.clearFocus()
                    vm.removeJob(job.id)
                    editingId = null
                },
            )
        }
    }
}

@Composable
private fun JobCard(
    job: JobItem,
    points: Int?,
    editing: Boolean,
    connected: Boolean,
    onPoints: ((Int) -> Unit)? = null,
    onToggleEdit: () -> Unit,
    onSave: (JobItem) -> Unit,
    onRemove: () -> Unit,
) {
    var label by rememberSaveable(job.id, job.label) { mutableStateOf(job.label) }
    var detail by rememberSaveable(job.id, job.detail) { mutableStateOf(job.detail) }
    var cat by rememberSaveable(job.id, job.cat) { mutableStateOf(job.cat) }
    var required by rememberSaveable(job.id, job.required) { mutableStateOf(job.required) }
    var kids by rememberSaveable(job.id, job.kids.joinToString()) { mutableStateOf(job.kids) }
    var ptsText by rememberSaveable(job.id, points) { mutableStateOf((points ?: 0).toString()) }

    Card(Modifier.fillMaxWidth().padding(bottom = 4.dp)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Column(Modifier.weight(1f)) {
                    Text(job.label, style = MaterialTheme.typography.titleMedium)
                    Text(
                        buildString {
                            append(job.id)
                            if (job.custom) append(" · custom")
                            append(if (job.required) " · needed for internet" else " · optional")
                        },
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                if (points != null && !editing) {
                    OutlinedTextField(
                        value = ptsText,
                        onValueChange = {
                            ptsText = it.filter { ch -> ch.isDigit() }
                            onPoints?.invoke(ptsText.toIntOrNull() ?: 0)
                        },
                        label = { Text("pts") },
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                        singleLine = true,
                        modifier = Modifier.width(88.dp),
                    )
                }
                TextButton(onClick = onToggleEdit) { Text(if (editing) "Close" else "Edit") }
            }
            if (editing) {
                OutlinedTextField(value = label, onValueChange = { label = it }, label = { Text("Name") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                OutlinedTextField(value = detail, onValueChange = { detail = it }, label = { Text("Hint") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    FilterChip(selected = cat == "expectation", onClick = { cat = "expectation" }, label = { Text("Expectation") })
                    FilterChip(selected = cat == "paid", onClick = { cat = "paid" }, label = { Text("Paid job") })
                }
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                    Text("Needed for internet")
                    Switch(checked = required, onCheckedChange = { required = it })
                }
                Text("Who does this", style = MaterialTheme.typography.labelLarge)
                AdminKids.all.forEach { kid ->
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                        Text(kid.name)
                        Switch(
                            checked = kid.slug in kids,
                            onCheckedChange = { on ->
                                kids = if (on) kids + kid.slug else kids - kid.slug
                            },
                        )
                    }
                }
                Button(
                    onClick = {
                        if (label.isBlank()) return@Button
                        if (points != null) onPoints?.invoke(ptsText.toIntOrNull() ?: 0)
                        onSave(
                            job.copy(
                                label = label.trim(),
                                detail = detail.trim(),
                                cat = cat,
                                required = required,
                                kids = kids.distinct(),
                            ),
                        )
                    },
                    enabled = connected,
                    modifier = Modifier.fillMaxWidth(),
                ) { Text("Save to panel") }
                if (job.custom) {
                    TextButton(onClick = onRemove, enabled = connected, modifier = Modifier.fillMaxWidth()) {
                        Text("Remove from panel", color = MaterialTheme.colorScheme.error)
                    }
                }
            }
        }
    }
}
