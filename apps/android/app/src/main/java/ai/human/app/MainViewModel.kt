package ai.human.app

import androidx.lifecycle.ViewModel

/**
 * Hosts the shared [GatewayClient] across main tabs so WebSocket state survives
 * configuration changes and is torn down when the activity finishes.
 */
class MainViewModel : ViewModel() {
    val gateway = GatewayClient()

    override fun onCleared() {
        gateway.disconnect()
        super.onCleared()
    }
}
