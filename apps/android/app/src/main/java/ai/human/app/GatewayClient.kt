package ai.human.app

import androidx.compose.runtime.Stable
import androidx.compose.runtime.Immutable
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
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

@Immutable
data class SessionSummary(
    val key: String,
    val label: String,
    val turnCount: Int,
    val lastActive: Long,
    val archived: Boolean = false,
)

@Immutable
data class ActivityEvent(
    val id: String,
    val type: String,
    val description: String,
    val timestamp: Long = System.currentTimeMillis(),
)

@Immutable
data class ToolInfo(
    val id: String,
    val name: String,
    val description: String,
    val category: String,
)

@Immutable
data class MemoryEntry(
    val key: String,
    val content: String,
    val category: String,
    val source: String,
    val timestamp: String,
)

@Immutable
data class HealthStatus(
    val status: String,
    val uptimeSecs: Long = 0,
)

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

    private val _sessions = MutableStateFlow<List<SessionSummary>>(emptyList())
    val sessions: StateFlow<List<SessionSummary>> = _sessions.asStateFlow()

    private val _activity = MutableStateFlow<List<ActivityEvent>>(emptyList())
    val activity: StateFlow<List<ActivityEvent>> = _activity.asStateFlow()

    private val _tools = MutableStateFlow<List<ToolInfo>>(emptyList())
    val tools: StateFlow<List<ToolInfo>> = _tools.asStateFlow()

    private val _memoryEntries = MutableStateFlow<List<MemoryEntry>>(emptyList())
    val memoryEntries: StateFlow<List<MemoryEntry>> = _memoryEntries.asStateFlow()

    private val _healthStatus = MutableStateFlow<HealthStatus?>(null)
    val healthStatus: StateFlow<HealthStatus?> = _healthStatus.asStateFlow()

    private val _overviewLoading = MutableStateFlow(false)
    val overviewLoading: StateFlow<Boolean> = _overviewLoading.asStateFlow()

    private val _hulaProgramCount = MutableStateFlow(0)
    val hulaProgramCount: StateFlow<Int> = _hulaProgramCount.asStateFlow()
    private val _hulaSuccessRate = MutableStateFlow(0)
    val hulaSuccessRate: StateFlow<Int> = _hulaSuccessRate.asStateFlow()

    private val _lastError = MutableStateFlow<String?>(null)
    val lastError: StateFlow<String?> = _lastError.asStateFlow()

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
                scope.launch {
                    _lastError.value = null
                    _state.value = ConnectionState.CONNECTED
                }
            }

            override fun onMessage(webSocket: WebSocket, text: String) {
                scope.launch {
                    try {
                        val json = JSONObject(text)
                        val type = json.optString("type", "")
                        if (type == "res") {
                            val result = json.optJSONObject("result") ?: json.optJSONObject("payload") ?: JSONObject()
                            val content = result.optString("content", "")
                                .ifBlank { result.optString("text", "") }
                            if (content.isNotBlank()) {
                                _events.value = GatewayEvent("response", result)
                            }
                            val sessionsArr = result.optJSONArray("sessions")
                            if (sessionsArr != null) {
                                val list = mutableListOf<SessionSummary>()
                                for (i in 0 until sessionsArr.length()) {
                                    val o = sessionsArr.optJSONObject(i) ?: continue
                                    val st = o.optString("status", "active")
                                    list.add(
                                        SessionSummary(
                                            key = o.optString("key", ""),
                                            label = o.optString("label", "Untitled"),
                                            turnCount = o.optInt("turn_count", 0),
                                            lastActive = (o.optDouble("last_active", 0.0) * 1000).toLong(),
                                            archived = st.equals("archived", ignoreCase = true),
                                        ),
                                    )
                                }
                                _sessions.value = list
                                _overviewLoading.value = false
                            }
                            val eventsArr = result.optJSONArray("events")
                            if (eventsArr != null) {
                                val list = mutableListOf<ActivityEvent>()
                                for (i in 0 until eventsArr.length()) {
                                    val o = eventsArr.optJSONObject(i) ?: continue
                                    val tsRaw = o.optDouble("time", o.optDouble("timestamp", 0.0))
                                    val tsMs = if (tsRaw > 1e12) tsRaw.toLong() else (tsRaw * 1000).toLong()
                                    val desc = o.optString("message", o.optString("text", o.optString("content", o.optString("preview", ""))))
                                    list.add(
                                        ActivityEvent(
                                            id = o.optString("id", "ev-$i-${tsMs}"),
                                            type = o.optString("type", ""),
                                            description = desc,
                                            timestamp = tsMs,
                                        ),
                                    )
                                }
                                _activity.value = list
                                _overviewLoading.value = false
                            }
                            val toolsArr = result.optJSONArray("tools")
                            if (toolsArr != null) {
                                val list = mutableListOf<ToolInfo>()
                                for (i in 0 until toolsArr.length()) {
                                    val o = toolsArr.optJSONObject(i) ?: continue
                                    val name = o.optString("name", "")
                                    if (name.isBlank()) continue
                                    list.add(
                                        ToolInfo(
                                            id = o.optString("id", name),
                                            name = name,
                                            description = o.optString("description", ""),
                                            category = o.optString("category", "General"),
                                        ),
                                    )
                                }
                                _tools.value = list
                            }
                            val memArr = result.optJSONArray("entries")
                            if (memArr != null) {
                                val list = mutableListOf<MemoryEntry>()
                                for (i in 0 until memArr.length()) {
                                    val o = memArr.optJSONObject(i) ?: continue
                                    val key = o.optString("key", "")
                                    if (key.isBlank()) continue
                                    list.add(
                                        MemoryEntry(
                                            key = key,
                                            content = o.optString("content", ""),
                                            category = o.optString("category", ""),
                                            source = o.optString("source", ""),
                                            timestamp = o.optString("timestamp", ""),
                                        ),
                                    )
                                }
                                _memoryEntries.value = list
                            }
                            val hulaSummary = result.optJSONObject("summary")
                            if (hulaSummary != null && hulaSummary.has("file_count")) {
                                _hulaProgramCount.value = hulaSummary.optInt("file_count", 0)
                                val successes = hulaSummary.optInt("success_count", 0)
                                val total = successes + hulaSummary.optInt("fail_count", 0)
                                _hulaSuccessRate.value = if (total > 0) (successes * 100 / total) else 0
                            }
                            if (result.has("status") || result.has("uptime_seconds") || result.has("uptime_secs")) {
                                val up = when {
                                    result.has("uptime_seconds") -> result.optLong("uptime_seconds", 0L)
                                    result.has("uptime_secs") -> result.optLong("uptime_secs", 0L)
                                    else -> 0L
                                }
                                _healthStatus.value = HealthStatus(
                                    status = result.optString("status", "unknown"),
                                    uptimeSecs = up,
                                )
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
                scope.launch {
                    _lastError.value = t.message ?: "Connection failed"
                    _state.value = ConnectionState.DISCONNECTED
                }
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

    /** Fetch sessions list. Updates _sessions when response received. */
    fun fetchSessions() {
        send("sessions.list", emptyMap())
    }

    fun setSessionArchived(key: String, archived: Boolean) {
        val status = if (archived) "archived" else "active"
        send("sessions.patch", mapOf("key" to key, "status" to status))
    }

    /** Fetch tools catalog. Updates _tools when response received. */
    fun fetchTools() {
        send("tools.catalog", emptyMap())
    }

    /** Fetch recent activity. Updates _activity when response received. */
    fun fetchRecentActivity() {
        send("activity.recent", emptyMap())
    }

    /** Fetch health status. Updates _healthStatus when response received. */
    fun fetchHealthStatus() {
        send("health", emptyMap())
    }

    /** Prefetch sessions list for adjacent tab navigation. */
    fun prefetchSessions() {
        fetchSessions()
    }

    /** Prefetch tools catalog for adjacent tab navigation. */
    fun prefetchTools() {
        fetchTools()
    }

    fun prefetchMemory() {
        fetchMemoryList()
    }

    /** Fetch overview data: sessions, activity, health, and HuLa analytics. */
    fun fetchOverviewData() {
        _overviewLoading.value = true
        send("sessions.list", emptyMap())
        send("activity.recent", emptyMap())
        send("health", emptyMap())
        send("hula.traces.analytics", emptyMap())
    }
}
