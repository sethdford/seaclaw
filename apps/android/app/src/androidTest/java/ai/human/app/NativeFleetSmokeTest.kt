package ai.human.app

import android.content.Intent
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Instrumented smoke tests — run on the Android emulator fleet in CI (`native-apps-fleet.yml`).
 */
@RunWith(AndroidJUnit4::class)
class NativeFleetSmokeTest {
    private val launchIntent: Intent =
        Intent(
            ApplicationProvider.getApplicationContext(),
            MainActivity::class.java,
        ).putExtra(EXTRA_SKIP_ONBOARDING_FOR_TEST, true)

    @get:Rule(order = 0)
    val activityRule = ActivityScenarioRule<MainActivity>(launchIntent)

    @get:Rule(order = 1)
    val composeRule: AndroidComposeTestRule<ActivityScenarioRule<MainActivity>, MainActivity> =
        AndroidComposeTestRule(activityRule) { rule ->
            var activity: MainActivity? = null
            rule.scenario.onActivity { activity = it }
            requireNotNull(activity)
        }

    @Test
    fun launch_shows_bottom_nav_overview() {
        composeRule.onNodeWithContentDescription("Overview").assertIsDisplayed()
    }
}
