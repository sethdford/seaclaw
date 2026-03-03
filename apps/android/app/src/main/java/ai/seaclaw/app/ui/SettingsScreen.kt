package ai.seaclaw.app.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Button
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import ai.seaclaw.app.GatewayManager

@Composable
fun SettingsScreen(gatewayManager: GatewayManager) {
    var url by remember { mutableStateOf(gatewayManager.gatewayUrl) }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(SCTokens.spaceMd)
    ) {
        OutlinedTextField(
            value = url,
            onValueChange = { url = it; gatewayManager.gatewayUrl = it },
            label = { Text("Gateway URL") },
            modifier = Modifier.fillMaxWidth(),
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Uri),
            singleLine = true
        )
        Text(
            text = "WebSocket URL, e.g. wss://10.0.2.2:3000/ws",
            modifier = Modifier.padding(top = SCTokens.spaceSm),
            style = androidx.compose.material3.MaterialTheme.typography.bodySmall,
            color = androidx.compose.material3.MaterialTheme.colorScheme.onSurfaceVariant
        )
        Button(
            onClick = { gatewayManager.reconnect() },
            modifier = Modifier.padding(top = SCTokens.spaceMd)
        ) {
            Text("Reconnect")
        }
    }
}
