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
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Psychology
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.SwipeToDismissBox
import androidx.compose.material3.SwipeToDismissBoxValue
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.pulltorefresh.PullToRefreshBox
import androidx.compose.material3.rememberSwipeToDismissBoxState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import ai.human.app.ConnectionState
import ai.human.app.GatewayClient
import ai.human.app.ui.HUTokens
import ai.human.app.ui.StaggeredItem
import ai.human.app.util.isReducedMotionEnabled
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

private val listItemSpring = spring<IntOffset>(
    dampingRatio = 0.86f,
    stiffness = Spring.StiffnessMediumLow,
)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MemoryScreen(
    gateway: GatewayClient = GatewayClient(),
    connectionState: ConnectionState = ConnectionState.DISCONNECTED,
) {
    val colorScheme = MaterialTheme.colorScheme
    val reducedMotion = isReducedMotionEnabled()
    val entries by gateway.memoryEntries.collectAsState()
    var searchText by remember { mutableStateOf("") }
    var isRefreshing by remember { mutableStateOf(false) }
    val scope = rememberCoroutineScope()

    val filteredEntries = remember(entries, searchText) {
        if (searchText.isBlank()) entries
        else entries.filter { entry ->
            entry.key.contains(searchText, ignoreCase = true) ||
                entry.content.contains(searchText, ignoreCase = true) ||
                entry.category.contains(searchText, ignoreCase = true)
        }
    }

    LaunchedEffect(connectionState) {
        if (connectionState == ConnectionState.CONNECTED) {
            gateway.fetchMemoryList()
        }
    }

    PullToRefreshBox(
        isRefreshing = isRefreshing,
        onRefresh = {
            scope.launch {
                isRefreshing = true
                gateway.fetchMemoryList()
                delay(500)
                isRefreshing = false
            }
        },
        modifier = Modifier.fillMaxSize(),
    ) {
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = HUTokens.spaceMd)
                .padding(top = HUTokens.spaceMd, bottom = HUTokens.spaceLg),
            verticalArrangement = Arrangement.spacedBy(HUTokens.spaceMd),
            contentPadding = PaddingValues(bottom = HUTokens.space2xl),
        ) {
            // Header
            item {
                StaggeredItem(
                    index = 0,
                    reducedMotion = reducedMotion,
                    enter = fadeIn(
                        animationSpec = spring(
                            dampingRatio = 0.86f,
                            stiffness = Spring.StiffnessMediumLow,
                        ),
                    ) + slideInVertically(
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

            // Search bar
            item {
                OutlinedTextField(
                    value = searchText,
                    onValueChange = { searchText = it },
                    modifier = Modifier.fillMaxWidth(),
                    placeholder = {
                        Text(
                            text = "Search memories...",
                            style = MaterialTheme.typography.bodyMedium,
                        )
                    },
                    leadingIcon = {
                        Icon(Icons.Default.Search, contentDescription = null)
                    },
                    singleLine = true,
                    shape = RoundedCornerShape(HUTokens.radiusMd),
                )
            }

            if (connectionState != ConnectionState.CONNECTED) {
                // Error / disconnected state
                item {
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(vertical = HUTokens.spaceXl),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.spacedBy(HUTokens.spaceSm),
                    ) {
                        Icon(
                            imageVector = Icons.Default.Warning,
                            contentDescription = null,
                            modifier = Modifier.size(40.dp),
                            tint = colorScheme.onSurfaceVariant,
                        )
                        Text(
                            text = "Could not load memories",
                            style = MaterialTheme.typography.titleMedium,
                            color = colorScheme.onBackground,
                        )
                        Text(
                            text = "Connect to the gateway to load memory entries.",
                            style = MaterialTheme.typography.bodyMedium,
                            color = colorScheme.onSurfaceVariant,
                        )
                        TextButton(onClick = {
                            scope.launch {
                                isRefreshing = true
                                gateway.fetchMemoryList()
                                delay(500)
                                isRefreshing = false
                            }
                        }) {
                            Text("Retry")
                        }
                    }
                }
            } else if (entries.isEmpty() && !isRefreshing) {
                // Empty state
                item {
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(vertical = HUTokens.spaceXl),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.spacedBy(HUTokens.spaceSm),
                    ) {
                        Icon(
                            imageVector = Icons.Default.Psychology,
                            contentDescription = null,
                            modifier = Modifier.size(48.dp),
                            tint = colorScheme.onSurfaceVariant,
                        )
                        Text(
                            text = "No memories yet",
                            style = MaterialTheme.typography.titleMedium,
                            color = colorScheme.onBackground,
                        )
                        Text(
                            text = "Memories will appear as you chat",
                            style = MaterialTheme.typography.bodyMedium,
                            color = colorScheme.onSurfaceVariant,
                        )
                    }
                }
            } else {
                // Memory entries
                itemsIndexed(filteredEntries, key = { _, e -> e.key }) { index, entry ->
                    StaggeredItem(
                        index = 2 + index,
                        reducedMotion = reducedMotion,
                        enter = fadeIn(
                            animationSpec = spring(
                                dampingRatio = 0.86f,
                                stiffness = Spring.StiffnessMediumLow,
                            ),
                        ) + slideInVertically(
                            animationSpec = listItemSpring,
                            initialOffsetY = { it / 4 },
                        ),
                    ) {
                        val dismissState = rememberSwipeToDismissBoxState(
                            confirmValueChange = { value ->
                                if (value == SwipeToDismissBoxValue.EndToStart) {
                                    gateway.send("memory.forget", mapOf("key" to entry.key))
                                    scope.launch {
                                        gateway.fetchMemoryList()
                                    }
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
                                        .clip(RoundedCornerShape(HUTokens.radiusMd))
                                        .background(colorScheme.errorContainer)
                                        .padding(horizontal = HUTokens.spaceMd),
                                    contentAlignment = Alignment.CenterEnd,
                                ) {
                                    Icon(
                                        imageVector = Icons.Default.Delete,
                                        contentDescription = "Delete",
                                        tint = colorScheme.onErrorContainer,
                                    )
                                }
                            },
                            enableDismissFromStartToEnd = false,
                        ) {
                            Column(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .clip(RoundedCornerShape(HUTokens.radiusMd))
                                    .background(colorScheme.surfaceContainerLow)
                                    .padding(HUTokens.spaceMd),
                            ) {
                                Row(
                                    modifier = Modifier.fillMaxWidth(),
                                    horizontalArrangement = Arrangement.SpaceBetween,
                                    verticalAlignment = Alignment.Top,
                                ) {
                                    Text(
                                        text = entry.key,
                                        style = MaterialTheme.typography.titleSmall,
                                        color = colorScheme.onBackground,
                                        modifier = Modifier.weight(1f),
                                    )
                                    Text(
                                        text = entry.timestamp,
                                        style = MaterialTheme.typography.labelSmall,
                                        color = colorScheme.onSurfaceVariant,
                                    )
                                }
                                Spacer(modifier = Modifier.height(HUTokens.spaceXs))
                                Text(
                                    text = entry.content,
                                    style = MaterialTheme.typography.bodyMedium,
                                    color = colorScheme.onSurfaceVariant,
                                    maxLines = 2,
                                    overflow = TextOverflow.Ellipsis,
                                )
                                if (entry.category.isNotBlank()) {
                                    Spacer(modifier = Modifier.height(HUTokens.spaceXs))
                                    Row(horizontalArrangement = Arrangement.spacedBy(HUTokens.spaceXs)) {
                                        Text(
                                            text = entry.category,
                                            style = MaterialTheme.typography.labelSmall,
                                            color = colorScheme.primary,
                                        )
                                        if (entry.source.isNotBlank()) {
                                            Text(
                                                text = "· ${entry.source}",
                                                style = MaterialTheme.typography.labelSmall,
                                                color = colorScheme.onSurfaceVariant,
                                            )
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
