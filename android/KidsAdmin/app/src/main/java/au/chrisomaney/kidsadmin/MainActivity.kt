package au.chrisomaney.kidsadmin

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Checklist
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Star
import androidx.compose.material.icons.filled.Wifi
import androidx.compose.material.icons.filled.Add
import androidx.compose.material3.Icon
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier.modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.lifecycle.viewmodel.compose.viewModel
import au.chrisomaney.kidsadmin.data.AdminViewModel
import au.chrisomaney.kidsadmin.ui.screens.BehaviourScreen
import au.chrisomaney.kidsadmin.ui.screens.ConnectScreen
import au.chrisomaney.kidsadmin.ui.screens.InternetScreen
import au.chrisomaney.kidsadmin.ui.screens.JobsScreen
import au.chrisomaney.kidsadmin.ui.screens.PointsScreen
import au.chrisomaney.kidsadmin.ui.theme.KidsAdminTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            KidsAdminTheme {
                KidsAdminAppRoot()
            }
        }
    }
}

private data class Tab(val label: String, val icon: ImageVector)

@Composable
private fun KidsAdminAppRoot(vm: AdminViewModel = viewModel()) {
    val state by vm.ui.collectAsState()
    var tab by rememberSaveable { mutableIntStateOf(0) }
    val tabs = listOf(
        Tab("Internet", Icons.Default.Wifi),
        Tab("Points", Icons.Default.Add),
        Tab("Behaviour", Icons.Default.Star),
        Tab("Jobs", Icons.Default.Checklist),
        Tab("Connect", Icons.Default.Settings),
    )

    Scaffold(
        bottomBar = {
            NavigationBar {
                tabs.forEachIndexed { index, t ->
                    NavigationBarItem(
                        selected = tab == index,
                        onClick = { tab = index },
                        icon = { Icon(t.icon, contentDescription = t.label) },
                        label = { Text(t.label) },
                    )
                }
            }
        },
    ) { padding ->
        androidx.compose.foundation.layout.Box(Modifier.padding(padding)) {
            when (tab) {
                0 -> InternetScreen(state, vm)
                1 -> PointsScreen(state, vm)
                2 -> BehaviourScreen(state, vm)
                3 -> JobsScreen(state, vm)
                else -> ConnectScreen(state, vm)
            }
        }
    }
}
