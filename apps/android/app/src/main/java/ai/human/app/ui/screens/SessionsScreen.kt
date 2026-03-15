package ai.human.app.ui.screens

import androidx.compose.animation.AnimatedVisibility
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
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.IntOffset
import ai.human.app.ui.HUTokens
import ai.human.app.ui.StaggeredItem
import ai.human.app.util.isReducedMotionEnabled

private data class SessionItem(
    val id: String,
    val title: String,
    val timestamp: String,
    val messageCount: Int,
    val preview: String,
)

private val listItemSpring = spring<IntOffset>(
    dampingRatio = 0.86f,
    stiffness = Spring.StiffnessMediumLow,
)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SessionsScreen() {
    val colorScheme = MaterialTheme.colorScheme
    val sessions = remember {
        mutableStateListOf(
            SessionItem("1", "CLI conversation", "2 min ago", 12, "I'll check the forecast for you."),
            SessionItem("2", "Telegram support", "1 hour ago", 8, "Here's my suggested refactor..."),
            SessionItem("3", "Discord channel sync", "3 hours ago", 24, "Based on your preferences..."),
            SessionItem("4", "Slack workspace", "Yesterday", 15, "Sure, I can help with that."),
            SessionItem("5", "Email thread", "2 days ago", 6, "Let me look into this."),
        )
    }
    val reducedMotion = isReducedMotionEnabled()

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
            key = { _, it -> it.id },
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
                    onDismiss = { sessions.remove(session) },
                )
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SessionListItem(
    session: SessionItem,
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
                    contentDescription = "Delete session ${session.title}",
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
                .background(colorScheme.primaryContainer)
                .padding(HUTokens.spaceMd)
                .semantics(mergeDescendants = true) {
                    contentDescription = "${session.title}, ${session.messageCount} messages, ${session.timestamp}, preview: ${session.preview}"
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
                            text = session.title,
                            style = MaterialTheme.typography.titleMedium,
                            color = colorScheme.onSurface,
                        )
                        Text(
                            text = "${session.messageCount}",
                            style = MaterialTheme.typography.labelSmall,
                            color = colorScheme.primary,
                            modifier = Modifier
                                .background(
                                    colorScheme.primary.copy(alpha = 0.2f),
                                    RoundedCornerShape(HUTokens.radiusSm),
                                )
                                .padding(horizontal = HUTokens.spaceXs, vertical = 2),
                        )
                    }
                    Text(
                        text = (session.preview.take(40) + if (session.preview.length > 40) "…" else ""),
                        style = MaterialTheme.typography.bodySmall,
                        color = colorScheme.onSurfaceVariant,
                    )
                    Text(
                        text = session.timestamp,
                        style = MaterialTheme.typography.labelSmall,
                        color = colorScheme.onSurfaceVariant,
                    )
                }
                Text(
                    text = "${session.messageCount} msgs",
                    style = MaterialTheme.typography.labelMedium,
                    color = colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}
