package ai.human.app.ui

import android.os.Build
import androidx.compose.foundation.background
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.asComposeRenderEffect
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.Dp

/**
 * Glass surface modifier with blur + tinted overlay.
 * Uses RenderEffect blur on Android 12+ (API 31+), falls back to alpha overlay.
 */
@Composable
fun Modifier.glassSurface(
    shape: Shape = RoundedCornerShape(HUTokens.radiusXl),
    alpha: Float = HUTokens.glassChatActionsBgOpacity,
    blurRadius: Dp = HUTokens.glassStandardBlur,
): Modifier {
    val colorScheme = MaterialTheme.colorScheme
    val surfaceColor = colorScheme.surfaceContainerHighest.copy(alpha = alpha)
    val density = LocalDensity.current
    val blurRadiusPx = with(density) { blurRadius.toPx() }

    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        this
            .clip(shape)
            .graphicsLayer {
                renderEffect = android.graphics.RenderEffect
                    .createBlurEffect(blurRadiusPx, blurRadiusPx, android.graphics.Shader.TileMode.CLAMP)
                    .asComposeRenderEffect()
            }
            .background(surfaceColor, shape)
    } else {
        this
            .clip(shape)
            .background(surfaceColor, shape)
    }
}
