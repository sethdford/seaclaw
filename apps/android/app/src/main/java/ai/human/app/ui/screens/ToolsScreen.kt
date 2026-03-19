package ai.human.app.ui.screens

import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.spring
import androidx.compose.animation.fadeIn
import androidx.compose.animation.slideInVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Build
import androidx.compose.material3.Icon
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
import ai.human.app.ToolInfo
import ai.human.app.ui.HUTokens
import ai.human.app.ui.StaggeredItem
import ai.human.app.util.isReducedMotionEnabled

private val listItemSpring = spring<IntOffset>(
    dampingRatio = 0.86f,
    stiffness = Spring.StiffnessMediumLow,
)

@Composable
fun ToolsScreen(
    gateway: GatewayClient = GatewayClient(),
    connectionState: ConnectionState = ConnectionState.DISCONNECTED,
) {
    val colorScheme = MaterialTheme.colorScheme
    val tools by gateway.tools.collectAsState()
    val reducedMotion = isReducedMotionEnabled()
    val categories = tools.groupBy { it.category }.map { (title, list) -> title to list }

    LaunchedEffect(Unit) {
        if (connectionState == ConnectionState.CONNECTED) {
            gateway.fetchTools()
        }
    }

    LazyColumn(
        modifier = Modifier
            .fillMaxSize()
            .padding(horizontal = HUTokens.spaceMd)
            .padding(top = HUTokens.spaceMd, bottom = HUTokens.spaceLg),
        verticalArrangement = Arrangement.spacedBy(HUTokens.spaceLg),
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
                    text = "Tools",
                    style = MaterialTheme.typography.headlineLarge,
                    color = colorScheme.onBackground,
                    modifier = Modifier.semantics {
                        contentDescription = "Tools"
                        heading()
                    },
                )
            }
        }

        categories.forEachIndexed { index, (categoryTitle, categoryTools) ->
            item(key = "header_$categoryTitle") {
                StaggeredItem(
                    index = 1 + index * 2,
                    reducedMotion = reducedMotion,
                    enter = fadeIn(animationSpec = spring(dampingRatio = 0.86f, stiffness = Spring.StiffnessMediumLow)) +
                        slideInVertically(
                            animationSpec = listItemSpring,
                            initialOffsetY = { it / 4 },
                        ),
                ) {
                    Text(
                        text = categoryTitle,
                        style = MaterialTheme.typography.titleMedium,
                        color = colorScheme.onSurfaceVariant,
                        modifier = Modifier
                            .padding(top = HUTokens.spaceMd)
                            .semantics { contentDescription = "Category: $categoryTitle" },
                    )
                }
            }

            item(key = "grid_$categoryTitle") {
                StaggeredItem(
                    index = 2 + index * 2,
                    reducedMotion = reducedMotion,
                    enter = fadeIn(animationSpec = spring(dampingRatio = 0.86f, stiffness = Spring.StiffnessMediumLow)) +
                        slideInVertically(
                            animationSpec = listItemSpring,
                            initialOffsetY = { it / 4 },
                        ),
                ) {
                    Column(
                        verticalArrangement = Arrangement.spacedBy(HUTokens.spaceMd),
                    ) {
                        categoryTools.chunked(2).forEach { rowTools ->
                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.spacedBy(HUTokens.spaceMd),
                            ) {
                                rowTools.forEach { tool ->
                                    Box(modifier = Modifier.weight(1f)) {
                                        ToolCard(
                                            name = tool.name,
                                            description = tool.description,
                                            icon = Icons.Filled.Build,
                                        )
                                    }
                                }
                                if (rowTools.size == 1) {
                                    Spacer(modifier = Modifier.weight(1f))
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun ToolCard(
    name: String,
    description: String,
    icon: androidx.compose.ui.graphics.vector.ImageVector,
) {
    val colorScheme = MaterialTheme.colorScheme

    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(HUTokens.radiusLg))
            .background(colorScheme.surfaceContainer)
            .padding(HUTokens.spaceMd)
            .semantics(mergeDescendants = true) {
                contentDescription = "$name: $description"
            },
    ) {
        Column {
            Icon(
                imageVector = icon,
                contentDescription = "Icon for $name",
                tint = colorScheme.primary,
                modifier = Modifier.size(HUTokens.spaceLg),
            )
            Text(
                text = name,
                style = MaterialTheme.typography.titleSmall,
                color = colorScheme.onSurface,
                modifier = Modifier.padding(top = HUTokens.spaceSm),
            )
            Text(
                text = description,
                style = MaterialTheme.typography.bodySmall,
                color = colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(top = HUTokens.spaceXs),
            )
        }
    }
}
