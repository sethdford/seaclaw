package ai.human.app.ui.screens

import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.spring
import androidx.compose.animation.fadeIn
import androidx.compose.animation.slideInVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material3.pulltorefresh.PullToRefreshBox
import androidx.compose.material3.pulltorefresh.rememberPullToRefreshState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.SwipeToDismissBox
import androidx.compose.material3.SwipeToDismissBoxValue
import androidx.compose.material3.Text
import androidx.compose.material3.rememberSwipeToDismissBoxState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import ai.human.app.ConnectionState
import ai.human.app.GatewayClient
import ai.human.app.SessionSummary
import ai.human.app.ui.HUTokens
import ai.human.app.ui.StaggeredItem
import ai.human.app.util.isReducedMotionEnabled

private fun formatRelativeTime(ms: Long): String {
    val diff = System.currentTimeMillis() - ms
    return when {
        diff < 60_000 -> "Just now"
        diff < 3600_000 -> "${diff / 60_000} min ago"
        diff < 86_400_000 -> "${diff / 3600_000} hours ago"
        diff < 172_800_000 -> "Yesterday"
        else -> "${diff / 86_400_000} days ago"
    }
}

private val listItemSpring = spring<IntOffset>(
    dampingRatio = 0.86f,
    stiffness = Spring.StiffnessMediumLow,
)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SessionsScreen(
    gateway: GatewayClient = GatewayClient(),
    connectionState: ConnectionState = ConnectionState.DISCONNECTED,
) {
    val colorScheme = MaterialTheme.colorScheme
    val scope = rememberCoroutineScope()
    var isRefreshing by remember { mutableStateOf(false) }
    val pullToRefreshState = rememberPullToRefreshState()
    val sessions by gateway.sessions.collectAsState()
    val reducedMotion = isReducedMotionEnabled()

    LaunchedEffect(connectionState) {
        if (connectionState == ConnectionState.CONNECTED) {
            gateway.fetchSessions()
        }
    }

    PullToRefreshBox(
        state = pullToRefreshState,
        isRefreshing = isRefreshing,
        onRefresh = {
            isRefreshing = true
            scope.launch {
                if (connectionState == ConnectionState.CONNECTED) {
                    gateway.fetchSessions()
                }
                delay(HUTokens.durationNormal.toLong())
                isRefreshing = false
            }
        },
    ) {
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
                    text = "Sessions",
                    style = MaterialTheme.typography.headlineLarge,
                    color = colorScheme.onBackground,
                    modifier = Modifier.semantics {
                        contentDescription = "Sessions"
                        heading()
                    },
                )
            }
        }

        itemsIndexed(
            items = sessions,
            key = { _, it -> it.key },
        ) { index, session ->
            StaggeredItem(
                index = 1 + index,
                reducedMotion = reducedMotion,
                enter = fadeIn(animationSpec = spring(dampingRatio = 0.86f, stiffness = Spring.StiffnessMediumLow)) +
                    slideInVertically(
                        animationSpec = listItemSpring,
                        initialOffsetY = { it / 4 },
                    ),
            ) {
                SessionListItem(
                    session = session,
                    onDismiss = {
                        gateway.send("sessions.delete", mapOf("key" to session.key))
                        scope.launch {
                            delay(300)
                            gateway.fetchSessions()
                        }
                    },
                )
            }
        }
    }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SessionListItem(
    session: SessionSummary,
    onDismiss: () -> Unit,
) {
    val colorScheme = MaterialTheme.colorScheme
    val dismissState = rememberSwipeToDismissBoxState(
        confirmValueChange = { value ->
            if (value == SwipeToDismissBoxValue.EndToStart) {
                onDismiss()
                true
            } else {
                false
            }
        },
    )

    SwipeToDismissBox(
        state = dismissState,
        backgroundContent = {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .clip(RoundedCornerShape(HUTokens.radiusLg))
                    .background(colorScheme.error)
                    .padding(horizontal = HUTokens.spaceLg),
                contentAlignment = Alignment.CenterEnd,
            ) {
                Icon(
                    imageVector = Icons.Filled.Delete,
                    contentDescription = "Delete session ${session.label}",
                    tint = colorScheme.onError,
                )
            }
        },
        enableDismissFromStartToEnd = false,
        enableDismissFromEndToStart = true,
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(HUTokens.radiusLg))
                .background(colorScheme.surfaceContainer)
                .padding(HUTokens.spaceMd)
                .semantics(mergeDescendants = true) {
                    contentDescription = "${session.label}, ${session.turnCount} messages, ${formatRelativeTime(session.lastActive)}"
                },
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(HUTokens.spaceXs),
                    ) {
                        Text(
                            text = session.label,
                            style = MaterialTheme.typography.titleMedium,
                            color = colorScheme.onSurface,
                        )
                        Text(
                            text = "${session.turnCount}",
                            style = MaterialTheme.typography.labelSmall,
                            color = colorScheme.primary,
                            modifier = Modifier
                                .background(
                                    colorScheme.primary.copy(alpha = 0.2f),
                                    RoundedCornerShape(HUTokens.radiusSm),
                                )
                                .padding(horizontal = HUTokens.spaceXs, vertical = 2.dp),
                        )
                    }
                    Text(
                        text = "—",
                        style = MaterialTheme.typography.bodySmall,
                        color = colorScheme.onSurfaceVariant,
                    )
                    Text(
                        text = formatRelativeTime(session.lastActive),
                        style = MaterialTheme.typography.labelSmall,
                        color = colorScheme.onSurfaceVariant,
                    )
                }
                Text(
                    text = "${session.turnCount} msgs",
                    style = MaterialTheme.typography.labelMedium,
                    color = colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}
