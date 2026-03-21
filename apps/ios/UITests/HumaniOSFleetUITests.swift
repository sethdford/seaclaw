import XCTest

/// Native iOS smoke UI tests — run on the simulator fleet in CI (`native-apps-fleet.yml`).
final class HumaniOSFleetUITests: XCTestCase {
    override func setUpWithError() throws {
        continueAfterFailure = false
    }

    func test_launch_skips_onboarding_and_shows_tab_bar() throws {
        let app = XCUIApplication()
        app.launchArguments.append("-uitestSkipOnboarding")
        app.launch()

        let overviewTab = app.tabBars.buttons["Overview"]
        XCTAssertTrue(overviewTab.waitForExistence(timeout: 15))
        XCTAssertTrue(app.tabBars.buttons["Chat"].exists)
    }
}
