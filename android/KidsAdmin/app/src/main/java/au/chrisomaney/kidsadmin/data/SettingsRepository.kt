package au.chrisomaney.kidsadmin.data

import android.content.Context
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val Context.dataStore by preferencesDataStore("kids_admin")

class SettingsRepository(private val context: Context) {
    private val brokerKey = stringPreferencesKey("broker")
    private val userKey = stringPreferencesKey("username")
    private val passKey = stringPreferencesKey("password")
    private val rememberKey = booleanPreferencesKey("remember")

    val settings: Flow<ConnectionSettings> = context.dataStore.data.map { prefs ->
        val remember = prefs[rememberKey] ?: true
        if (!remember) {
            ConnectionSettings(remember = false)
        } else {
            ConnectionSettings(
                broker = prefs[brokerKey] ?: "ws://homeassistant.local:1884",
                username = prefs[userKey] ?: "",
                password = prefs[passKey] ?: "",
                remember = true,
            )
        }
    }

    suspend fun save(settings: ConnectionSettings) {
        context.dataStore.edit { prefs ->
            prefs[rememberKey] = settings.remember
            if (settings.remember) {
                prefs[brokerKey] = settings.broker
                prefs[userKey] = settings.username
                prefs[passKey] = settings.password
            } else {
                prefs.remove(brokerKey)
                prefs.remove(userKey)
                prefs.remove(passKey)
            }
        }
    }
}
