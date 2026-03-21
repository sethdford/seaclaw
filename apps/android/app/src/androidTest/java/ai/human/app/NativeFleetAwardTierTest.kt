package ai.human.app

import android.content.Intent
import androidx.compose.ui.test.assertExists
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.performClick
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Instrumented journeys aligned with Google Play “Best app” / accessibility expectations:
 * bottom-nav coverage, settings gateway heading, and stable content descriptions.
 */
@RunWith(AndroidJUnit4::class)
class NativeFleetAwardTierTest {
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
    fun bottom_nav_destinations_exist() {
        for (label in
            listOf(
                "Overview",
                "Chat",
                "Sessions",
                "Tools",
                "Settings",
            )) {
            composeRule.onNodeWithContentDescription(label).assertExists()
        }
    }

    @Test
    fun overview_shows_welcome_heading() {
        composeRule.onNodeWithContentDescription("Overview").performClick()
        composeRule.onNodeWithContentDescription("Welcome back").assertIsDisplayed()
    }

    @Test
    fun journey_visits_each_tab_then_opens_gateway_settings() {
        for (label in
            listOf(
                "Overview",
                "Chat",
                "Sessions",
                "Tools",
                "Settings",
            )) {
            composeRule.onNodeWithContentDescription(label).performClick()
        }
        composeRule.onNodeWithContentDescription("Gateway settings").assertIsDisplayed()
    }
}
