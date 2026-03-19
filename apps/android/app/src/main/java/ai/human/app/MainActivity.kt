package ai.human.app

import android.content.Intent
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.spring
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.EnterTransition
import androidx.compose.animation.ExitTransition
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.ui.res.painterResource
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.text.style.TextAlign
import ai.human.app.ui.screens.ChatScreen
import ai.human.app.ui.screens.OverviewScreen
import ai.human.app.ui.screens.SessionsScreen
import ai.human.app.ui.screens.SettingsScreen
import ai.human.app.ui.screens.ToolsScreen
import ai.human.app.ui.theme.HumanTheme
import ai.human.app.ui.HUTokens
import ai.human.app.ui.glassSurface
import ai.human.app.util.isReducedMotionEnabled
import ai.human.app.R

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            HumanTheme {
                HumanRoot(intent = intent)
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
    }
}

private fun isValidGatewayUrl(url: String): Boolean {
    val trimmed = url.trim()
    if (trimmed.isEmpty()) return false
    return trimmed.startsWith("ws://") || trimmed.startsWith("wss://") ||
        trimmed.startsWith("http://") || trimmed.startsWith("https://")
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun HumanRoot(intent: Intent?) {
    var hasOnboarded by rememberSaveable { mutableStateOf(false) }
    var gatewayUrl by rememberSaveable { mutableStateOf("http://localhost:3000") }

    if (hasOnboarded) {
        HumanApp(intent = intent, initialGatewayUrl = gatewayUrl)
    } else {
        OnboardingScreen(
            initialUrl = gatewayUrl,
            onComplete = { url ->
                gatewayUrl = url.trim()
                hasOnboarded = true
            },
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun HumanApp(intent: Intent?, initialGatewayUrl: String = "http://localhost:3000") {
    var selectedTab by remember { mutableIntStateOf(0) }
    val gateway = remember { GatewayClient() }
    val connectionState by gateway.state.collectAsState()

    DisposableEffect(selectedTab) {
        if (selectedTab in 0..3) {
            gateway.connectIfNeeded(initialGatewayUrl)
        }
        if (selectedTab == 0 && connectionState == ConnectionState.CONNECTED) {
            gateway.fetchOverviewData()
        }
        onDispose { }
    }

    DisposableEffect(Unit) {
        onDispose { gateway.disconnect() }
    }

    LaunchedEffect(intent) {
        val uri = intent?.data ?: return@LaunchedEffect
        if (uri.scheme == "human") {
            when (uri.host) {
                "chat" -> selectedTab = 1
                "sessions" -> selectedTab = 2
                "tools" -> selectedTab = 3
                "settings" -> selectedTab = 4
            }
        }
    }

    LaunchedEffect(selectedTab) {
        // Prefetch adjacent tabs for perceived instant navigation
        val prev = (selectedTab - 1).coerceIn(0, 4)
        val next = (selectedTab + 1).coerceIn(0, 4)
        if (connectionState == ConnectionState.CONNECTED) {
            when (prev) {
                2 -> gateway.prefetchSessions()
                3 -> gateway.prefetchTools()
                else -> { }
            }
            when (next) {
                2 -> gateway.prefetchSessions()
                3 -> gateway.prefetchTools()
                else -> { }
            }
        }
    }

    BackHandler(enabled = selectedTab != 0) {
        selectedTab = 0
    }

    Scaffold(
        bottomBar = {
            NavigationBar(
                containerColor = MaterialTheme.colorScheme.surfaceContainerLow,
            ) {
                NavigationBarItem(
                    selected = selectedTab == 0,
                    onClick = { selectedTab = 0 },
                    icon = {
                        Icon(
                            painter = painterResource(R.drawable.ic_phosphor_house),
                            contentDescription = "Overview",
                            tint = if (selectedTab == 0) MaterialTheme.colorScheme.primary
                            else MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    },
                    label = { Text("Overview") },
                )
                NavigationBarItem(
                    selected = selectedTab == 1,
                    onClick = { selectedTab = 1 },
                    icon = {
                        Icon(
                            painter = painterResource(R.drawable.ic_phosphor_chat_circle),
                            contentDescription = "Chat",
                            tint = if (selectedTab == 1) MaterialTheme.colorScheme.primary
                            else MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    },
                    label = { Text("Chat") },
                )
                NavigationBarItem(
                    selected = selectedTab == 2,
                    onClick = { selectedTab = 2 },
                    icon = {
                        Icon(
                            painter = painterResource(R.drawable.ic_phosphor_clock),
                            contentDescription = "Sessions",
                            tint = if (selectedTab == 2) MaterialTheme.colorScheme.primary
                            else MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    },
                    label = { Text("Sessions") },
                )
                NavigationBarItem(
                    selected = selectedTab == 3,
                    onClick = { selectedTab = 3 },
                    icon = {
                        Icon(
                            painter = painterResource(R.drawable.ic_phosphor_wrench),
                            contentDescription = "Tools",
                            tint = if (selectedTab == 3) MaterialTheme.colorScheme.primary
                            else MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    },
                    label = { Text("Tools") },
                )
                NavigationBarItem(
                    selected = selectedTab == 4,
                    onClick = { selectedTab = 4 },
                    icon = {
                        Icon(
                            painter = painterResource(R.drawable.ic_phosphor_gear),
                            contentDescription = "Settings",
                            tint = if (selectedTab == 4) MaterialTheme.colorScheme.primary
                            else MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    },
                    label = { Text("Settings") },
                )
            }
        },
    ) { padding ->
        val reducedMotion = isReducedMotionEnabled()
        Box(modifier = Modifier.padding(padding)) {
            AnimatedContent(
                targetState = selectedTab,
                transitionSpec = {
                    if (reducedMotion) {
                        EnterTransition.None togetherWith ExitTransition.None
                    } else {
                        (fadeIn(animationSpec = spring(stiffness = Spring.StiffnessMediumLow)) +
                            slideInVertically { it / 20 }) togetherWith
                            fadeOut(animationSpec = spring(stiffness = Spring.StiffnessMediumLow))
                    }
                },
                modifier = Modifier.fillMaxSize(),
                label = "screen_transition",
            ) { tab ->
                when (tab) {
                    0 -> OverviewScreen(gateway = gateway, connectionState = connectionState)
                    1 -> ChatScreen(gateway = gateway)
                    2 -> SessionsScreen(gateway = gateway, connectionState = connectionState)
                    3 -> ToolsScreen(gateway = gateway, connectionState = connectionState)
                    4 -> SettingsScreen(
                        gateway = gateway,
                        connectionState = connectionState,
                        initialGatewayUrl = initialGatewayUrl,
                    )
                    else -> OverviewScreen(gateway = gateway, connectionState = connectionState)
                }
            }
        }
    }
}

@Composable
private fun OnboardingScreen(
    initialUrl: String = "http://localhost:3000",
    onComplete: (String) -> Unit,
) {
    val colorScheme = MaterialTheme.colorScheme
    var gatewayUrl by remember { mutableStateOf(initialUrl) }
    val isValidUrl = isValidGatewayUrl(gatewayUrl)

    val pageIcons = listOf(
        R.drawable.ic_phosphor_house,
        R.drawable.ic_phosphor_lightning,
        R.drawable.ic_phosphor_chat_circle,
    )
    val pages = listOf(
        Triple("Welcome to h-uman", "not quite human.", "Minimal footprint, maximum capability."),
        Triple("Lightning Fast", "~1696 KB binary, <6 MB RAM.", "<30 ms startup. Zero dependencies."),
        Triple("34 Channels", "Telegram, Discord, Slack, and more.", "Connect your preferred messaging platform."),
    )
    val pagerState = rememberPagerState(pageCount = { pages.size })

    val permissionLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestPermission(),
    ) { /* result not required for flow */ }

    LaunchedEffect(Unit) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            permissionLauncher.launch(android.Manifest.permission.POST_NOTIFICATIONS)
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(colorScheme.background)
            .padding(HUTokens.spaceLg),
        verticalArrangement = Arrangement.SpaceBetween,
    ) {
        HorizontalPager(
            state = pagerState,
            modifier = Modifier.weight(1f),
            userScrollEnabled = true,
        ) { page ->
            val pageOffset = (pagerState.currentPage - page) + pagerState.currentPageOffsetFraction
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .graphicsLayer {
                        alpha = 1f - kotlin.math.abs(pageOffset) * 0.4f
                        translationX = pageOffset * 80f
                    },
                verticalArrangement = Arrangement.Center,
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Icon(
                    painter = painterResource(pageIcons[page]),
                    contentDescription = "Illustration",
                    modifier = Modifier.size(80.dp),
                    tint = colorScheme.primary,
                )
                Spacer(modifier = Modifier.height(HUTokens.spaceLg))
                Text(
                    text = pages[page].first,
                    style = MaterialTheme.typography.headlineLarge,
                    color = colorScheme.onBackground,
                    textAlign = TextAlign.Center,
                )
                Spacer(modifier = Modifier.height(HUTokens.spaceMd))
                Text(
                    text = pages[page].second,
                    style = MaterialTheme.typography.bodyLarge,
                    color = colorScheme.onSurfaceVariant,
                    textAlign = TextAlign.Center,
                )
                Text(
                    text = pages[page].third,
                    style = MaterialTheme.typography.bodyMedium,
                    color = colorScheme.onSurfaceVariant,
                    textAlign = TextAlign.Center,
                )
            }
        }

        Column(
            modifier = Modifier
                .fillMaxWidth()
                .glassSurface()
                .padding(HUTokens.spaceMd),
            verticalArrangement = Arrangement.spacedBy(HUTokens.spaceMd),
        ) {
            BasicTextField(
                value = gatewayUrl,
                onValueChange = { gatewayUrl = it },
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(HUTokens.radiusMd))
                    .background(colorScheme.surfaceContainerHigh)
                    .border(
                        HUTokens.spaceXs / 4,
                        if (isValidUrl) colorScheme.outlineVariant else colorScheme.error,
                        RoundedCornerShape(HUTokens.radiusMd),
                    )
                    .padding(horizontal = HUTokens.spaceMd, vertical = HUTokens.spaceSm)
                    .semantics { contentDescription = "Gateway URL" },
                textStyle = TextStyle(
                    color = colorScheme.onSurface,
                    fontSize = HUTokens.textBase,
                ),
                singleLine = true,
                cursorBrush = SolidColor(colorScheme.primary),
            )

            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(HUTokens.radiusMd))
                    .background(
                        if (isValidUrl) colorScheme.primary
                        else colorScheme.surfaceContainerHigh,
                    )
                    .then(
                        if (isValidUrl) Modifier.clickable { onComplete(gatewayUrl) }
                        else Modifier,
                    )
                    .padding(HUTokens.spaceMd)
                    .semantics {
                        contentDescription = if (isValidUrl) "Get started" else "Get started (enter valid URL first)"
                    },
                contentAlignment = Alignment.Center,
            ) {
                Text(
                    text = "Get Started",
                    style = MaterialTheme.typography.labelLarge,
                    color = if (isValidUrl) colorScheme.onPrimary else colorScheme.onSurfaceVariant,
                )
            }

            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable { onComplete("http://localhost:3000") }
                    .padding(HUTokens.spaceSm),
                contentAlignment = Alignment.Center,
            ) {
                Text(
                    text = "Skip for now",
                    style = MaterialTheme.typography.bodySmall,
                    color = colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}
