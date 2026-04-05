package ai.human.app.ui.screens

import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.spring
import androidx.compose.animation.fadeIn
import androidx.compose.animation.slideInVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.border
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
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.ClickableText
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.IntOffset
import ai.human.app.GatewayClient
import ai.human.app.ui.HUTokens
import ai.human.app.ui.StaggeredItem
import ai.human.app.util.isReducedMotionEnabled

private data class ChatMessage(
    val id: Int,
    val text: String,
    val isUser: Boolean,
)

private val listItemSpring = spring<IntOffset>(
    dampingRatio = 0.86f,
    stiffness = Spring.StiffnessMediumLow,
)

// MARK: - Markdown Parsing

private fun parseMarkdownToAnnotatedString(
    text: String,
    linkColor: Color,
    codeBackground: Color,
): AnnotatedString {
    return buildAnnotatedString {
        val codeBlockRegex = Regex("```(?:\\w*\\n)?([\\s\\S]*?)```")
        var lastEnd = 0

        for (match in codeBlockRegex.findAll(text)) {
            if (match.range.first > lastEnd) {
                appendInlineMarkdown(
                    text.substring(lastEnd, match.range.first),
                    linkColor,
                    codeBackground,
                )
            }
            val code = match.groupValues[1].trim()
            withStyle(
                SpanStyle(
                    fontFamily = FontFamily.Monospace,
                    fontSize = HUTokens.textSm,
                    background = codeBackground,
                ),
            ) {
                append(code)
            }
            lastEnd = match.range.last + 1
        }

        if (lastEnd < text.length) {
            appendInlineMarkdown(text.substring(lastEnd), linkColor, codeBackground)
        }
    }
}

private fun AnnotatedString.Builder.appendInlineMarkdown(
    text: String,
    linkColor: Color,
    codeBackground: Color,
) {
    // Order matters: bold (**) before italic (*) so ** isn't consumed as two *
    val pattern = Regex(
        "\\*\\*(.+?)\\*\\*" +
            "|\\*(.+?)\\*" +
            "|`([^`]+)`" +
            "|\\[(.+?)\\]\\((.+?)\\)",
    )

    var lastEnd = 0
    for (match in pattern.findAll(text)) {
        if (match.range.first > lastEnd) {
            append(text.substring(lastEnd, match.range.first))
        }

        when {
            match.groupValues[1].isNotEmpty() -> {
                // Bold
                withStyle(SpanStyle(fontWeight = FontWeight.Bold)) {
                    append(match.groupValues[1])
                }
            }
            match.groupValues[2].isNotEmpty() -> {
                // Italic
                withStyle(SpanStyle(fontStyle = FontStyle.Italic)) {
                    append(match.groupValues[2])
                }
            }
            match.groupValues[3].isNotEmpty() -> {
                // Inline code
                withStyle(
                    SpanStyle(
                        fontFamily = FontFamily.Monospace,
                        fontSize = HUTokens.textSm,
                        background = codeBackground,
                    ),
                ) {
                    append(match.groupValues[3])
                }
            }
            match.groupValues[4].isNotEmpty() -> {
                // Link
                pushStringAnnotation(tag = "URL", annotation = match.groupValues[5])
                withStyle(
                    SpanStyle(
                        color = linkColor,
                        textDecoration = TextDecoration.Underline,
                    ),
                ) {
                    append(match.groupValues[4])
                }
                pop()
            }
        }

        lastEnd = match.range.last + 1
    }

    if (lastEnd < text.length) {
        append(text.substring(lastEnd))
    }
}

// MARK: - Chat Screen

@Composable
fun ChatScreen(gateway: GatewayClient = GatewayClient()) {
    val colorScheme = MaterialTheme.colorScheme
    val reducedMotion = isReducedMotionEnabled()
    val nextId = remember { mutableIntStateOf(1) }
    val messages = remember { mutableStateListOf<ChatMessage>() }
    var inputText by remember { mutableStateOf("") }
    val listState = rememberLazyListState()
    val events by gateway.events.collectAsState()

    LaunchedEffect(events) {
        events?.let { event ->
            if (event.type == "response") {
                val text = event.payload?.optString("content", "")
                    ?: event.payload?.optString("text", "")
                    ?: ""
                if (text.isNotBlank()) {
                    messages.add(ChatMessage(nextId.intValue++, text, false))
                }
            }
        }
    }

    LaunchedEffect(messages.size) {
        if (messages.isNotEmpty()) {
            listState.animateScrollToItem(messages.size - 1)
        }
    }

    Box(modifier = Modifier.fillMaxSize()) {
        Column(modifier = Modifier.fillMaxSize()) {
            LazyColumn(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxWidth()
                    .padding(horizontal = HUTokens.spaceMd)
                    .padding(top = HUTokens.spaceMd)
                    .graphicsLayer { },
                state = listState,
                verticalArrangement = Arrangement.spacedBy(HUTokens.spaceMd),
                contentPadding = PaddingValues(bottom = HUTokens.spaceMd),
            ) {
                itemsIndexed(messages, key = { _, it -> it.id }) { index, message ->
                    StaggeredItem(
                        index = index,
                        reducedMotion = reducedMotion,
                        enter = fadeIn(animationSpec = spring(dampingRatio = 0.86f, stiffness = Spring.StiffnessMediumLow)) +
                            slideInVertically(
                                animationSpec = listItemSpring,
                                initialOffsetY = { it / 4 },
                            ),
                    ) {
                        ChatBubble(
                            text = message.text,
                            isUser = message.isUser,
                        )
                    }
                }
            }

            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(HUTokens.spaceMd)
                    .clip(RoundedCornerShape(HUTokens.radiusLg))
                    .background(colorScheme.surfaceContainerHigh)
                    .border(HUTokens.spaceXs / 4, colorScheme.outlineVariant, RoundedCornerShape(HUTokens.radiusLg))
                    .padding(HUTokens.spaceSm),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(HUTokens.spaceSm),
            ) {
                BasicTextField(
                    value = inputText,
                    onValueChange = { inputText = it },
                    modifier = Modifier
                        .weight(1f)
                        .padding(horizontal = HUTokens.spaceMd, vertical = HUTokens.spaceSm)
                        .semantics { contentDescription = "Message input" },
                    textStyle = TextStyle(
                        color = colorScheme.onSurface,
                        fontSize = HUTokens.textBase,
                        fontFamily = MaterialTheme.typography.bodyMedium.fontFamily,
                    ),
                    singleLine = false,
                    maxLines = 4,
                    cursorBrush = SolidColor(colorScheme.primary),
                    decorationBox = { innerTextField ->
                        Box {
                            if (inputText.isEmpty()) {
                                Text(
                                    text = "Type a message...",
                                    style = MaterialTheme.typography.bodyMedium,
                                    color = colorScheme.onSurfaceVariant,
                                )
                            }
                            innerTextField()
                        }
                    },
                    onTextLayout = {},
                    keyboardActions = androidx.compose.foundation.text.KeyboardActions(
                        onSend = {
                            if (inputText.isNotBlank()) {
                                messages.add(ChatMessage(nextId.intValue++, inputText, true))
                                gateway.send("chat.send", mapOf("message" to inputText))
                                inputText = ""
                            }
                        },
                    ),
                    keyboardOptions = androidx.compose.foundation.text.KeyboardOptions(
                        imeAction = androidx.compose.ui.text.input.ImeAction.Send,
                    ),
                )
            }
        }
    }
}

@Composable
private fun ChatBubble(
    text: String,
    isUser: Boolean,
) {
    val colorScheme = MaterialTheme.colorScheme
    val uriHandler = LocalUriHandler.current
    val (backgroundColor, textColor) = if (isUser) {
        colorScheme.primary to colorScheme.onPrimary
    } else {
        colorScheme.surface to colorScheme.onSurface
    }
    val linkColor = if (isUser) colorScheme.onPrimary else colorScheme.primary
    val codeBackground = if (isUser) {
        colorScheme.onPrimary.copy(alpha = 0.15f)
    } else {
        colorScheme.surfaceContainerHigh
    }

    val annotatedString = remember(text, linkColor, codeBackground) {
        parseMarkdownToAnnotatedString(text, linkColor, codeBackground)
    }

    val role = if (isUser) "You" else "Assistant"
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .semantics { contentDescription = "$role said: $text" },
        horizontalArrangement = if (isUser) Arrangement.End else Arrangement.Start,
    ) {
        Box(
            modifier = Modifier
                .clip(
                    RoundedCornerShape(
                        topStart = HUTokens.radiusLg,
                        topEnd = HUTokens.radiusLg,
                        bottomStart = if (isUser) HUTokens.radiusLg else HUTokens.radiusSm,
                        bottomEnd = if (isUser) HUTokens.radiusSm else HUTokens.radiusLg,
                    ),
                )
                .background(backgroundColor)
                .padding(horizontal = HUTokens.spaceMd, vertical = HUTokens.spaceSm),
        ) {
            @Suppress("DEPRECATION")
            ClickableText(
                text = annotatedString,
                style = MaterialTheme.typography.bodyMedium.copy(color = textColor),
                onClick = { offset ->
                    annotatedString.getStringAnnotations(tag = "URL", start = offset, end = offset)
                        .firstOrNull()?.let { uriHandler.openUri(it.item) }
                },
            )
        }
    }
}
