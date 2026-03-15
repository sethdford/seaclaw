package ai.human.app.ui

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.EnterTransition
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import kotlinx.coroutines.delay

@Composable
fun StaggeredItem(
    index: Int,
    reducedMotion: Boolean,
    enter: EnterTransition,
    content: @Composable () -> Unit,
) {
    val delayMs = (index.coerceAtMost(6) * 50).toLong()
    var visible by remember { mutableStateOf(reducedMotion) }
    LaunchedEffect(Unit) {
        if (!reducedMotion) {
            delay(delayMs)
            visible = true
        }
    }
    AnimatedVisibility(
        visible = visible,
        enter = enter,
    ) {
        content()
    }
}
