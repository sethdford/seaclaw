package ai.human.app.ui.screens

import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.spring
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import ai.human.app.ui.HUTokens

@Composable
fun SettingsScreen() {
    val colorScheme = MaterialTheme.colorScheme
    var gatewayUrl by remember { mutableStateOf("ws://localhost:3000") }
    var isConnected by remember { mutableStateOf(false) }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(HUTokens.spaceMd),
        verticalArrangement = Arrangement.spacedBy(HUTokens.spaceLg),
    ) {
        Text(
            text = "Gateway",
            style = MaterialTheme.typography.titleMedium,
            color = colorScheme.onBackground,
            modifier = Modifier.semantics { contentDescription = "Gateway settings" },
        )

        BasicTextField(
            value = gatewayUrl,
            onValueChange = { gatewayUrl = it },
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(HUTokens.radiusMd))
                .background(colorScheme.surfaceVariant)
                .padding(horizontal = HUTokens.spaceMd, vertical = HUTokens.spaceSm)
                .semantics { contentDescription = "Gateway URL: $gatewayUrl" },
            textStyle = TextStyle(
                color = colorScheme.onSurface,
                fontSize = HUTokens.textBase,
                fontFamily = MaterialTheme.typography.bodyMedium.fontFamily,
            ),
            singleLine = true,
            decorationBox = { innerTextField ->
                Box {
                    if (gatewayUrl.isEmpty()) {
                        Text(
                            text = "Gateway URL",
                            style = MaterialTheme.typography.bodyMedium,
                            color = colorScheme.onSurfaceVariant,
                        )
                    }
                    innerTextField()
                }
            },
        )

        Row(
            modifier = Modifier
                .fillMaxWidth()
                .semantics(mergeDescendants = true) {
                    contentDescription = "Connection status: ${if (isConnected) "Connected" else "Disconnected"}"
                },
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(HUTokens.spaceMd),
        ) {
            ConnectionStatusIndicator(isConnected = isConnected)
            Text(
                text = if (isConnected) "Connected" else "Disconnected",
                style = MaterialTheme.typography.bodyMedium,
                color = colorScheme.onSurfaceVariant,
            )
        }

        Spacer(modifier = Modifier.height(HUTokens.spaceSm))

        Box(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(HUTokens.radiusMd))
                .background(
                    if (isConnected) colorScheme.error else colorScheme.primary,
                    shape = RoundedCornerShape(HUTokens.radiusMd),
                )
                .clickable { isConnected = !isConnected }
                .padding(horizontal = HUTokens.spaceMd, vertical = HUTokens.spaceSm)
                .semantics { contentDescription = if (isConnected) "Disconnect from gateway" else "Connect to gateway" },
            contentAlignment = Alignment.Center,
        ) {
            Text(
                text = if (isConnected) "Disconnect" else "Connect",
                style = MaterialTheme.typography.labelLarge,
                color = colorScheme.onPrimary,
            )
        }
    }
}

@Composable
private fun ConnectionStatusIndicator(isConnected: Boolean) {
    val colorScheme = MaterialTheme.colorScheme
    val targetColor = if (isConnected) colorScheme.primary else colorScheme.error
    val animatedColor by animateColorAsState(
        targetValue = targetColor,
        animationSpec = spring(
            dampingRatio = 0.7f,
            stiffness = HUTokens.springStandardStiffness,
        ),
        label = "connection_status_color",
    )

    Box(
        modifier = Modifier
            .size(12.dp)
            .clip(CircleShape)
            .background(animatedColor),
    )
}
