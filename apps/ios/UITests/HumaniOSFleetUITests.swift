import XCTest

/// Native iOS UI tests for the CI fleet (`native-apps-fleet.yml`).
/// Aligned with award-tier expectations: primary journeys, accessibility labels, and touch targets.
final class HumaniOSFleetUITests: XCTestCase {
    private var app: XCUIApplication!

    /// iPhone tab bars only show four items plus **More** when there are six `TabView` children; iPad often shows all six.
    private let primaryTabLabels = ["Overview", "Chat", "Memory", "Sessions", "Tools", "Settings"]

    private enum Timeout {
        /// CI simulators (especially SE / cold workers) need headroom beyond default 20s.
        static let launchForeground: TimeInterval = 45
        static let tabBar: TimeInterval = 45
        static let tabItem: TimeInterval = 25
        static let content: TimeInterval = 25
    }

    override func setUpWithError() throws {
        continueAfterFailure = false
        app = XCUIApplication()
        app.launchArguments.append("-uitestSkipOnboarding")
    }

    override func tearDownWithError() throws {
        app = nil
    }

    // MARK: - Lifecycle helpers

    private func launchAndSettle() {
        app.launch()
        XCTAssertTrue(
            app.wait(for: .runningForeground, timeout: Timeout.launchForeground),
            "App should reach foreground (CI simulator startup)",
        )
        XCTAssertTrue(
            app.tabBars.firstMatch.waitForExistence(timeout: Timeout.tabBar),
            "Tab bar should appear after launch",
        )
        XCTAssertTrue(
            app.tabBars.buttons["Overview"].waitForExistence(timeout: Timeout.tabBar),
            "Primary shell should expose Overview",
        )
    }

    /// Row for an overflow tab inside the system **More** list.
    private func moreListCell(for label: String) -> XCUIElement {
        let byLabel = NSPredicate(format: "label == %@", label)
        return app.tables.cells.containing(byLabel).firstMatch
    }

    /// Selects a root tab, using **More** when the item is not on the main tab bar (six tabs on iPhone).
    private func selectPrimaryTab(_ label: String) {
        let direct = app.tabBars.buttons[label]
        if direct.waitForExistence(timeout: 3), direct.isHittable {
            direct.tap()
            return
        }
        let more = app.tabBars.buttons["More"]
        XCTAssertTrue(more.waitForExistence(timeout: Timeout.tabItem), "Expected More tab for overflow item \(label)")
        XCTAssertTrue(more.isHittable, "More tab should be tappable")
        more.tap()
        tapOverflowTabRow(label)
    }

    /// Asserts the tab is reachable with a proper touch target: main bar button or **More** list row.
    private func assertPrimaryTabHittable(_ label: String) {
        let direct = app.tabBars.buttons[label]
        if direct.waitForExistence(timeout: 4) {
            XCTAssertTrue(direct.isHittable, "Tab \(label) should be hittable")
            return
        }
        let more = app.tabBars.buttons["More"]
        XCTAssertTrue(more.waitForExistence(timeout: Timeout.tabItem), "Expected More tab for overflow item \(label)")
        XCTAssertTrue(more.isHittable, "More tab should be hittable (touch target / hit testing)")
        more.tap()
        tapOverflowTabRow(label)
    }

    private func tapOverflowTabRow(_ label: String) {
        let cell = moreListCell(for: label)
        if cell.waitForExistence(timeout: Timeout.tabItem) {
            XCTAssertTrue(cell.isHittable, "More list row \(label) should be hittable")
            cell.tap()
            return
        }
        let alt = app.tables.staticTexts[label].firstMatch
        XCTAssertTrue(alt.waitForExistence(timeout: 8), "Expected \(label) in More tab list")
        XCTAssertTrue(alt.isHittable, "More list row \(label) should be hittable")
        alt.tap()
    }

    // MARK: - Tests

    func test_launch_shows_tab_bar_with_core_destinations() throws {
        launchAndSettle()
        for label in ["Overview", "Chat", "Memory", "Sessions"] {
            XCTAssertTrue(
                app.tabBars.buttons[label].waitForExistence(timeout: Timeout.tabItem),
                "Expected tab bar item \(label)",
            )
        }
        let toolsOnBar = app.tabBars.buttons["Tools"].waitForExistence(timeout: 3)
        let settingsOnBar = app.tabBars.buttons["Settings"].waitForExistence(timeout: 3)
        if toolsOnBar, settingsOnBar {
            XCTAssertTrue(app.tabBars.buttons["Tools"].exists)
            XCTAssertTrue(app.tabBars.buttons["Settings"].exists)
        } else {
            XCTAssertTrue(
                app.tabBars.buttons["More"].waitForExistence(timeout: Timeout.tabItem),
                "With six tabs on iPhone, overflow destinations appear under More",
            )
        }
    }

    func test_primary_tabs_are_hittable_award_touch_targets() throws {
        launchAndSettle()
        for label in primaryTabLabels {
            assertPrimaryTabHittable(label)
        }
    }

    func test_overview_shows_recent_activity_section() throws {
        launchAndSettle()
        selectPrimaryTab("Overview")
        XCTAssertTrue(
            app.staticTexts["Recent Activity"].waitForExistence(timeout: Timeout.content),
            "Overview should surface Recent Activity (density / information architecture)",
        )
    }

    func test_tab_journey_reaches_settings_with_labeled_gateway_field() throws {
        launchAndSettle()
        selectPrimaryTab("Settings")
        XCTAssertTrue(
            app.navigationBars["Settings"].waitForExistence(timeout: Timeout.content),
            "Settings navigation title should appear",
        )
        let serverURL = app.textFields["Server URL"]
        let gatewayURL = app.textFields["Gateway URL"]
        XCTAssertTrue(
            serverURL.waitForExistence(timeout: Timeout.content)
                || gatewayURL.waitForExistence(timeout: 8),
            "Settings must expose a labeled gateway field (VoiceOver / ADA alignment)",
        )
    }

    func test_chat_tab_loads_without_crash() throws {
        launchAndSettle()
        selectPrimaryTab("Chat")
        XCTAssertTrue(app.tabBars.buttons["Chat"].waitForExistence(timeout: Timeout.tabItem))
    }
}
