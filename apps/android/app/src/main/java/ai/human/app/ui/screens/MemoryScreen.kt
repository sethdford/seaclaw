package ai.human.app.ui.screens

import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.spring
import androidx.compose.animation.fadeIn
import androidx.compose.animation.slideInVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.IntOffset
import ai.human.app.ConnectionState
import ai.human.app.GatewayClient
import ai.human.app.ui.HUTokens
import ai.human.app.ui.StaggeredItem
import ai.human.app.util.isReducedMotionEnabled

private val listItemSpring = spring<IntOffset>(
    dampingRatio = 0.86f,
    stiffness = Spring.StiffnessMediumLow,
)

@Composable
fun MemoryScreen(
    gateway: GatewayClient = GatewayClient(),
    connectionState: ConnectionState = ConnectionState.DISCONNECTED,
) {
    val colorScheme = MaterialTheme.colorScheme
    val reducedMotion = isReducedMotionEnabled()
    val entries by gateway.memoryEntries.collectAsState()

    LaunchedEffect(connectionState) {
        if (connectionState == ConnectionState.CONNECTED) {
            gateway.fetchMemoryList()
        }
    }

    LazyColumn(
        modifier = Modifier
            .fillMaxSize()
            .padding(horizontal = HUTokens.spaceMd)
            .padding(top = HUTokens.spaceMd, bottom = HUTokens.spaceLg),
        verticalArrangement = Arrangement.spacedBy(HUTokens.spaceMd),
        contentPadding = PaddingValues(bottom = HUTokens.space2xl),
    ) {
        item {
            StaggeredItem(
                index = 0,
                reducedMotion = reducedMotion,
                enter = fadeIn(animationSpec = spring(dampingRatio = 0.86f, stiffness = Spring.StiffnessMediumLow)) +
                    slideInVertically(
                        animationSpec = listItemSpring,
                        initialOffsetY = { it / 4 },
                    ),
            ) {
                Text(
                    text = "Memory",
                    style = MaterialTheme.typography.headlineLarge,
                    color = colorScheme.onBackground,
                    modifier = Modifier.semantics {
                        contentDescription = "Memory"
                        heading()
                    },
                )
            }
        }

        if (connectionState != ConnectionState.CONNECTED) {
            item {
                Text(
                    text = "Connect to the gateway to load memory entries.",
                    style = MaterialTheme.typography.bodyMedium,
                    color = colorScheme.onSurfaceVariant,
                )
            }
        } else if (entries.isEmpty()) {
            item {
                Text(
                    text = "No memory entries yet.",
                    style = MaterialTheme.typography.bodyMedium,
                    color = colorScheme.onSurfaceVariant,
                )
            }
        } else {
            itemsIndexed(entries, key = { _, e -> e.key }) { index, entry ->
                StaggeredItem(
                    index = 1 + index,
                    reducedMotion = reducedMotion,
                    enter = fadeIn(animationSpec = spring(dampingRatio = 0.86f, stiffness = Spring.StiffnessMediumLow)) +
                        slideInVertically(
                            animationSpec = listItemSpring,
                            initialOffsetY = { it / 4 },
                        ),
                ) {
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(HUTokens.radiusMd))
                            .background(colorScheme.surfaceContainerLow)
                            .padding(HUTokens.spaceMd),
                    ) {
                        Text(
                            text = entry.key,
                            style = MaterialTheme.typography.titleSmall,
                            color = colorScheme.onBackground,
                        )
                        Spacer(modifier = Modifier.height(HUTokens.spaceXs))
                        Text(
                            text = entry.content,
                            style = MaterialTheme.typography.bodyMedium,
                            color = colorScheme.onSurfaceVariant,
                        )
                    }
                }
            }
        }
    }
}
