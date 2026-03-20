package ai.human.app.data

import android.content.Context
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.map

private val Context.humanDataStore by preferencesDataStore(name = "human_prefs")

private val GATEWAY_URL_KEY = stringPreferencesKey("gateway_url")

private const val DEFAULT_GATEWAY_URL = "http://localhost:3000"

suspend fun Context.readGatewayUrl(): String =
    humanDataStore.data.map { prefs -> prefs[GATEWAY_URL_KEY] ?: DEFAULT_GATEWAY_URL }.first()

suspend fun Context.saveGatewayUrl(url: String) {
    val trimmed = url.trim().ifEmpty { DEFAULT_GATEWAY_URL }
    humanDataStore.edit { it[GATEWAY_URL_KEY] = trimmed }
}
