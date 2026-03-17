package ai.human.app.ui.screens

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.spring
import androidx.compose.animation.fadeIn
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.pulltorefresh.PullToRefreshBox
import androidx.compose.material3.pulltorefresh.rememberPullToRefreshState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import ai.human.app.ConnectionState
import ai.human.app.GatewayClient
import ai.human.app.ui.HUTokens
import ai.human.app.ui.StaggeredItem
import ai.human.app.util.isReducedMotionEnabled

private val listItemSpring = spring<IntOffset>(
    dampingRatio = 0.86f,
    stiffness = Spring.StiffnessMediumLow,
)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun OverviewScreen(
    gateway: GatewayClient = GatewayClient(),
    connectionState: ConnectionState = ConnectionState.DISCONNECTED,
) {
    val colorScheme = MaterialTheme.colorScheme
    val reducedMotion = isReducedMotionEnabled()
    val scope = rememberCoroutineScope()
    var isRefreshing by remember { mutableStateOf(false) }
    val pullToRefreshState = rememberPullToRefreshState()
    val events by gateway.events.collectAsState()
    val sessions by gateway.sessions.collectAsState()
    val overviewActivity by gateway.overviewActivity.collectAsState()
    val recentActivity = remember { mutableStateListOf<String>() }

    LaunchedEffect(connectionState) {
        if (connectionState == ConnectionState.CONNECTED) {
            gateway.fetchOverviewData()
        }
    }

    LaunchedEffect(events) {
        events?.let { event ->
            val summary = when (event.type) {
                "message" -> "Message: ${event.payload?.optString("text", "")?.take(60) ?: ""}"
                "channel_event" -> "Channel: ${event.payload?.optString("channel", "unknown")}"
                "tool_call" -> "Tool: ${event.payload?.optString("name", "unknown")}"
                "status" -> "Status: ${event.payload?.optString("status", "")}"
                else -> "${event.type}: ${event.payload?.optString("text", "")?.take(40) ?: "event"}"
            }
            if (summary.isNotBlank()) {
                recentActivity.add(0, summary)
                if (recentActivity.size > 20) recentActivity.removeRange(20, recentActivity.size)
            }
        }
    }

    val statusLabel = when (connectionState) {
        ConnectionState.CONNECTED -> "Connected"
        ConnectionState.CONNECTING -> "Connecting..."
        ConnectionState.DISCONNECTED -> "Disconnected"
    }
    val statusColor = when (connectionState) {
        ConnectionState.CONNECTED -> colorScheme.primary
        ConnectionState.CONNECTING -> colorScheme.tertiary
        ConnectionState.DISCONNECTED -> colorScheme.error
    }

    val displayActivity = when {
        connectionState == ConnectionState.CONNECTED && overviewActivity.isNotEmpty() ->
            overviewActivity.map { "${it.type}: ${it.text.take(60)}" }
        connectionState == ConnectionState.CONNECTED -> recentActivity
        else -> emptyList<String>()
    }
    val showSkeletons = connectionState != ConnectionState.CONNECTED

    PullToRefreshBox(
        state = pullToRefreshState,
        isRefreshing = isRefreshing,
        onRefresh = {
            isRefreshing = true
            scope.launch {
                if (connectionState == ConnectionState.CONNECTED) {
                    gateway.fetchOverviewData()
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
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        text = "Welcome back",
                        style = MaterialTheme.typography.headlineLarge.copy(
                            letterSpacing = (-0.5).sp,
                        ),
                        color = colorScheme.onBackground,
                        modifier = Modifier.semantics {
                            contentDescription = "Welcome back"
                            heading()
                        },
                    )
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(HUTokens.spaceXs),
                        verticalAlignment = Alignment.CenterVertically,
                        modifier = Modifier.semantics {
                            contentDescription = "Gateway status: $statusLabel"
                        },
                    ) {
                        Box(
                            modifier = Modifier
                                .size(8.dp)
                                .clip(CircleShape)
                                .background(statusColor)
                                .clearAndSetSemantics { },
                        )
                        Text(
                            text = statusLabel,
                            style = MaterialTheme.typography.labelSmall,
                            color = colorScheme.onSurfaceVariant,
                        )
                    }
                }
            }
        }

        item {
            Spacer(modifier = Modifier.height(HUTokens.spaceSm))
        }

        item {
            StaggeredItem(
                index = 1,
                reducedMotion = reducedMotion,
                enter = fadeIn(animationSpec = spring(dampingRatio = 0.86f, stiffness = Spring.StiffnessMediumLow)) +
                    slideInVertically(
                        animationSpec = listItemSpring,
                        initialOffsetY = { it / 4 },
                    ),
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(HUTokens.spaceMd),
                ) {
                    StatCard(
                        title = "Gateway",
                        value = statusLabel,
                        modifier = Modifier.weight(1f),
                        isSkeleton = showSkeletons,
                    )
                    StatCard(
                        title = "Events",
                        value = if (showSkeletons) null else displayActivity.size,
                        modifier = Modifier.weight(1f),
                        isSkeleton = showSkeletons,
                    )
                }
            }
        }

        item {
            StaggeredItem(
                index = 2,
                reducedMotion = reducedMotion,
                enter = fadeIn(animationSpec = spring(dampingRatio = 0.86f, stiffness = Spring.StiffnessMediumLow)) +
                    slideInVertically(
                        animationSpec = listItemSpring,
                        initialOffsetY = { it / 4 },
                    ),
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(HUTokens.spaceMd),
                ) {
                    StatCard(
                        title = "Connection",
                        value = if (connectionState == ConnectionState.CONNECTED) "Active" else "Offline",
                        modifier = Modifier.weight(1f),
                        isSkeleton = showSkeletons,
                    )
                    StatCard(
                        title = "Messages",
                        value = if (showSkeletons) null else sessions.sumOf { it.turnCount },
                        modifier = Modifier.weight(1f),
                        isSkeleton = showSkeletons,
                    )
                }
            }
        }

        item {
            Spacer(modifier = Modifier.height(HUTokens.spaceLg))
        }

        item {
            StaggeredItem(
                index = 3,
                reducedMotion = reducedMotion,
                enter = fadeIn(animationSpec = spring(dampingRatio = 0.86f, stiffness = Spring.StiffnessMediumLow)) +
                    slideInVertically(
                        animationSpec = listItemSpring,
                        initialOffsetY = { it / 4 },
                    ),
            ) {
                Text(
                    text = when {
                        showSkeletons -> "Loading..."
                        displayActivity.isEmpty() -> "No activity yet"
                        else -> "Live activity"
                    },
                    style = MaterialTheme.typography.titleMedium,
                    color = colorScheme.onBackground,
                )
            }
        }

        if (showSkeletons) {
            items(3) { index ->
                StaggeredItem(
                    index = 4 + index,
                    reducedMotion = reducedMotion,
                    enter = fadeIn(animationSpec = spring(dampingRatio = 0.86f, stiffness = Spring.StiffnessMediumLow)) +
                        slideInVertically(
                            animationSpec = listItemSpring,
                            initialOffsetY = { it / 4 },
                        ),
                ) {
                    SkeletonPlaceholder()
                }
            }
        } else {
            items(displayActivity.size) { index ->
                val activity = displayActivity[index]
                StaggeredItem(
                    index = 4 + index,
                    reducedMotion = reducedMotion,
                    enter = fadeIn(animationSpec = spring(dampingRatio = 0.86f, stiffness = Spring.StiffnessMediumLow)) +
                        slideInVertically(
                            animationSpec = listItemSpring,
                            initialOffsetY = { it / 4 },
                        ),
                ) {
                    ActivityItem(text = activity)
                }
            }
        }
    }
    }
}

@Composable
private fun SkeletonPlaceholder() {
    val colorScheme = MaterialTheme.colorScheme
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(HUTokens.radiusMd))
            .background(colorScheme.surfaceContainerLow)
            .padding(HUTokens.spaceMd),
    ) {
        Column(verticalArrangement = Arrangement.spacedBy(HUTokens.spaceSm)) {
            Box(
                modifier = Modifier
                    .height(14.dp)
                    .fillMaxWidth(0.8f)
                    .clip(RoundedCornerShape(HUTokens.radiusSm))
                    .background(colorScheme.surfaceContainerHigh),
            )
            Box(
                modifier = Modifier
                    .height(12.dp)
                    .fillMaxWidth(0.5f)
                    .clip(RoundedCornerShape(HUTokens.radiusSm))
                    .background(colorScheme.surfaceContainerHigh),
            )
        }
    }
}

@Composable
private fun StatCard(
    title: String,
    value: String,
    modifier: Modifier = Modifier,
    isSkeleton: Boolean = false,
) {
    val colorScheme = MaterialTheme.colorScheme
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(HUTokens.radiusLg))
            .background(colorScheme.surfaceContainerLow)
            .padding(HUTokens.spaceMd)
            .semantics { contentDescription = "$title: $value" },
    ) {
        Column {
            Text(
                text = title,
                style = MaterialTheme.typography.labelMedium,
                color = colorScheme.onSurfaceVariant,
            )
            Spacer(modifier = Modifier.height(HUTokens.spaceXs))
            if (isSkeleton) {
                Box(
                    modifier = Modifier
                        .height(28.dp)
                        .fillMaxWidth(0.5f)
                        .clip(RoundedCornerShape(HUTokens.radiusSm))
                        .background(colorScheme.surfaceContainerHigh),
                )
            } else {
                Text(
                    text = value,
                    style = MaterialTheme.typography.displaySmall.copy(
                        letterSpacing = (-0.5).sp,
                    ),
                    color = colorScheme.onBackground,
                )
            }
        }
    }
}

@Composable
private fun StatCard(
    title: String,
    value: Int?,
    modifier: Modifier = Modifier,
    isSkeleton: Boolean = false,
) {
    val colorScheme = MaterialTheme.colorScheme
    val displayValue = value ?: 0
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(HUTokens.radiusLg))
            .background(colorScheme.surfaceContainerLow)
            .padding(HUTokens.spaceMd)
            .semantics { contentDescription = "$title: $displayValue" },
    ) {
        Column {
            Text(
                text = title,
                style = MaterialTheme.typography.labelMedium,
                color = colorScheme.onSurfaceVariant,
            )
            Spacer(modifier = Modifier.height(HUTokens.spaceXs))
            if (isSkeleton) {
                Box(
                    modifier = Modifier
                        .height(28.dp)
                        .fillMaxWidth(0.5f)
                        .clip(RoundedCornerShape(HUTokens.radiusSm))
                        .background(colorScheme.surfaceContainerHigh),
                )
            } else {
                AnimatedContent(
                    targetState = displayValue,
                    transitionSpec = {
                        val spec = spring<IntOffset>(dampingRatio = 0.86f, stiffness = Spring.StiffnessMediumLow)
                        if (targetState > initialState) {
                            slideInVertically(animationSpec = spec) { -it } togetherWith slideOutVertically(animationSpec = spec) { it }
                        } else {
                            slideInVertically(animationSpec = spec) { it } togetherWith slideOutVertically(animationSpec = spec) { -it }
                        }
                    },
                    label = "stat_counter",
                ) { count ->
                    Text(
                        text = "$count",
                        style = MaterialTheme.typography.displaySmall.copy(
                            letterSpacing = (-0.5).sp,
                        ),
                        color = colorScheme.onBackground,
                    )
                }
            }
        }
    }
}

@Composable
private fun ActivityItem(text: String) {
    val colorScheme = MaterialTheme.colorScheme
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(HUTokens.radiusMd))
            .background(colorScheme.surfaceContainerLow)
            .padding(HUTokens.spaceMd),
    ) {
        Text(
            text = text,
            style = MaterialTheme.typography.bodyMedium,
            color = colorScheme.onBackground,
        )
    }
}
