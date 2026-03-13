package ai.human.app.ui.screens

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.slideInVertically
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
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import ai.human.app.ui.HUTokens

private val overviewSpring = spring<Float>(
    dampingRatio = Spring.DampingRatioMediumBouncy,
    stiffness = HUTokens.springExpressiveStiffness,
)

@Composable
fun OverviewScreen() {
    val colorScheme = MaterialTheme.colorScheme
    var visible by remember { mutableStateOf(false) }
    val recentActivity = remember {
        mutableStateListOf(
            "Chat with human via CLI",
            "Telegram message received",
            "Discord channel synced",
        )
    }

    LaunchedEffect(Unit) {
        visible = true
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
            AnimatedVisibility(
                visible = visible,
                enter = fadeIn(animationSpec = tween(HUTokens.durationNormal.toInt())) +
                    slideInVertically(
                        animationSpec = overviewSpring,
                        initialOffsetY = { it / 4 },
                    ),
            ) {
                Text(
                    text = "Welcome back",
                    style = MaterialTheme.typography.headlineLarge,
                    color = colorScheme.onBackground,
                )
            }
        }

        item {
            Spacer(modifier = Modifier.height(HUTokens.spaceSm))
        }

        item {
            AnimatedVisibility(
                visible = visible,
                enter = fadeIn(animationSpec = tween(HUTokens.durationNormal.toInt(), delayMillis = 50)) +
                    slideInVertically(
                        animationSpec = overviewSpring,
                        initialOffsetY = { it / 4 },
                    ),
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(HUTokens.spaceMd),
                ) {
                    StatCard(
                        title = "Messages sent",
                        value = "1,247",
                        modifier = Modifier.weight(1f),
                    )
                    StatCard(
                        title = "Active channels",
                        value = "3",
                        modifier = Modifier.weight(1f),
                    )
                }
            }
        }

        item {
            AnimatedVisibility(
                visible = visible,
                enter = fadeIn(animationSpec = tween(HUTokens.durationNormal.toInt(), delayMillis = 100)) +
                    slideInVertically(
                        animationSpec = overviewSpring,
                        initialOffsetY = { it / 4 },
                    ),
            ) {
                StatCard(
                    title = "Uptime",
                    value = "4d 12h",
                    modifier = Modifier.fillMaxWidth(),
                )
            }
        }

        item {
            Spacer(modifier = Modifier.height(HUTokens.spaceLg))
        }

        item {
            AnimatedVisibility(
                visible = visible,
                enter = fadeIn(animationSpec = tween(HUTokens.durationNormal.toInt(), delayMillis = 150)) +
                    slideInVertically(
                        animationSpec = overviewSpring,
                        initialOffsetY = { it / 4 },
                    ),
            ) {
                Text(
                    text = "Recent activity",
                    style = MaterialTheme.typography.titleMedium,
                    color = colorScheme.onBackground,
                )
            }
        }

        items(recentActivity) { activity ->
            AnimatedVisibility(
                visible = visible,
                enter = fadeIn(animationSpec = tween(HUTokens.durationNormal.toInt(), delayMillis = 200)) +
                    slideInVertically(
                        animationSpec = spring(
                            dampingRatio = Spring.DampingRatioMediumBouncy,
                            stiffness = HUTokens.springStandardStiffness,
                        ),
                        initialOffsetY = { it / 4 },
                    ),
            ) {
                ActivityItem(text = activity)
            }
        }
    }
}

@Composable
private fun StatCard(
    title: String,
    value: String,
    modifier: Modifier = Modifier,
) {
    val colorScheme = MaterialTheme.colorScheme
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(HUTokens.radiusLg))
            .background(colorScheme.primaryContainer)
            .padding(HUTokens.spaceMd),
    ) {
        Column {
            Text(
                text = title,
                style = MaterialTheme.typography.labelMedium,
                color = colorScheme.onSurfaceVariant,
            )
            Spacer(modifier = Modifier.height(HUTokens.spaceXs))
            Text(
                text = value,
                style = MaterialTheme.typography.titleLarge,
                color = colorScheme.onBackground,
            )
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
            .background(colorScheme.primaryContainer)
            .padding(HUTokens.spaceMd),
    ) {
        Text(
            text = text,
            style = MaterialTheme.typography.bodyMedium,
            color = colorScheme.onBackground,
        )
    }
}
