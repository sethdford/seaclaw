import XCTest

/// Native iOS UI tests for the CI fleet (`native-apps-fleet.yml`).
/// Aligned with award-tier expectations: primary journeys, accessibility labels, and touch targets.
final class HumaniOSFleetUITests: XCTestCase {
    private var app: XCUIApplication!

    override func setUpWithError() throws {
        continueAfterFailure = false
        app = XCUIApplication()
        app.launchArguments.append("-uitestSkipOnboarding")
    }

    override func tearDownWithError() throws {
        app = nil
    }

    func test_launch_shows_tab_bar_with_core_destinations() throws {
        app.launch()
        XCTAssertTrue(app.tabBars.buttons["Overview"].waitForExistence(timeout: 20))
        for label in ["Overview", "Chat", "Sessions", "Tools", "Settings"] {
            XCTAssertTrue(
                app.tabBars.buttons[label].exists,
                "Expected tab bar item \(label)",
            )
        }
    }

    func test_primary_tabs_are_hittable_award_touch_targets() throws {
        app.launch()
        XCTAssertTrue(app.tabBars.buttons["Overview"].waitForExistence(timeout: 20))
        for label in ["Overview", "Chat", "Sessions", "Tools", "Settings"] {
            let tab = app.tabBars.buttons[label]
            XCTAssertTrue(tab.waitForExistence(timeout: 5))
            XCTAssertTrue(tab.isHittable, "Tab \(label) should be hittable (touch target / hit testing)")
        }
    }

    func test_overview_shows_recent_activity_section() throws {
        app.launch()
        XCTAssertTrue(app.tabBars.buttons["Overview"].waitForExistence(timeout: 20))
        XCTAssertTrue(
            app.staticTexts["Recent Activity"].waitForExistence(timeout: 15),
            "Overview should surface Recent Activity (density / information architecture)",
        )
    }

    func test_tab_journey_reaches_settings_with_labeled_gateway_field() throws {
        app.launch()
        XCTAssertTrue(app.tabBars.buttons["Settings"].waitForExistence(timeout: 20))
        app.tabBars.buttons["Settings"].tap()
        let serverField = app.textFields["Server URL"]
        XCTAssertTrue(
            serverField.waitForExistence(timeout: 15),
            "Settings must expose a labeled gateway field (VoiceOver / ADA alignment)",
        )
    }

    func test_chat_tab_loads_without_crash() throws {
        app.launch()
        XCTAssertTrue(app.tabBars.buttons["Chat"].waitForExistence(timeout: 20))
        app.tabBars.buttons["Chat"].tap()
        XCTAssertTrue(app.tabBars.buttons["Chat"].exists)
    }
}
