package ai.human.app

import androidx.compose.runtime.Stable
import androidx.compose.runtime.Immutable
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import org.json.JSONObject
import java.util.concurrent.TimeUnit

@Stable
enum class ConnectionState { DISCONNECTED, CONNECTING, CONNECTED }

@Immutable
data class GatewayEvent(val type: String, val payload: JSONObject?)

class GatewayClient {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val client = OkHttpClient.Builder()
        .readTimeout(0, TimeUnit.SECONDS)
        .pingInterval(30, TimeUnit.SECONDS)
        .build()

    private var webSocket: WebSocket? = null
    private var requestCounter = 0

    private val _state = MutableStateFlow(ConnectionState.DISCONNECTED)
    val state: StateFlow<ConnectionState> = _state

    private val _events = MutableStateFlow<GatewayEvent?>(null)
    val events: StateFlow<GatewayEvent?> = _events

    /** Connect only when needed (e.g. when Chat tab is selected). Idempotent. */
    fun connectIfNeeded(url: String) {
        if (_state.value == ConnectionState.CONNECTING || _state.value == ConnectionState.CONNECTED) return
        connect(url)
    }

    fun connect(url: String) {
        if (_state.value == ConnectionState.CONNECTING || _state.value == ConnectionState.CONNECTED) return
        _state.value = ConnectionState.CONNECTING

        val wsUrl = url.replace("http://", "ws://").replace("https://", "wss://")
            .trimEnd('/') + "/ws"

        val request = Request.Builder().url(wsUrl).build()
        webSocket = client.newWebSocket(request, object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                scope.launch { _state.value = ConnectionState.CONNECTED }
            }

            override fun onMessage(webSocket: WebSocket, text: String) {
                scope.launch {
                    try {
                        val json = JSONObject(text)
                        val type = json.optString("type", "")
                        if (type == "res") {
                            val result = json.optJSONObject("result")
                            val content = result?.optString("content", "")
                                ?: result?.optString("text", "") ?: ""
                            if (content.isNotBlank()) {
                                _events.value = GatewayEvent("response", result)
                            }
                        } else if (type == "event") {
                            _events.value = GatewayEvent(
                                json.optString("event", ""),
                                json.optJSONObject("data") ?: json
                            )
                        }
                    } catch (_: Exception) { }
                }
            }

            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
                scope.launch { _state.value = ConnectionState.DISCONNECTED }
            }

            override fun onClosed(webSocket: WebSocket, code: Int, reason: String) {
                scope.launch { _state.value = ConnectionState.DISCONNECTED }
            }
        })
    }

    fun send(method: String, params: Map<String, Any> = emptyMap()) {
        val id = "req-${++requestCounter}"
        val json = JSONObject().apply {
            put("type", "req")
            put("id", id)
            put("method", method)
            put("params", JSONObject(params))
        }
        webSocket?.send(json.toString())
    }

    fun disconnect() {
        webSocket?.close(1000, "User disconnect")
        webSocket = null
        _state.value = ConnectionState.DISCONNECTED
    }

    /** Prefetch sessions list for adjacent tab navigation. Placeholder until ViewModel caches. */
    fun prefetchSessions() {
        send("sessions.list", emptyMap())
    }

    /** Prefetch tools catalog for adjacent tab navigation. Placeholder until ViewModel caches. */
    fun prefetchTools() {
        send("tools.catalog", emptyMap())
    }
}
