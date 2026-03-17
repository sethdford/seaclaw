package ai.human.app.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Shape

/**
 * Semi-transparent glass effect for bottom sheets and dialogs.
 * Uses surfaceContainerHighest with alpha for a frosted appearance.
 */
@Composable
fun Modifier.glassSurface(
    shape: Shape = RoundedCornerShape(HUTokens.radiusXl),
    alpha: Float = HUTokens.glassChatActionsBgOpacity,
): Modifier {
    val colorScheme = MaterialTheme.colorScheme
    val surfaceColor = colorScheme.surfaceContainerHighest.copy(alpha = alpha)
    return this
        .clip(shape)
        .background(surfaceColor, shape)
}
