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

    /// Stable UI-test identifiers on `ContentView` tab items (`tab_overview`, …).
    private func tabAccessibilityIdentifier(for title: String) -> String {
        "tab_" + title.lowercased().replacingOccurrences(of: " ", with: "_")
    }

    /// Prefer the system tab bar; some iOS versions still expose a `tabBars` node even when tab controls are not its direct children.
    private var primaryTabBar: XCUIElement {
        app.tabBars.firstMatch
    }

    /// Resolves a tab control: accessibility id (preferred), `tabBars` buttons, then app-level buttons (newer iOS / SwiftUI).
    private func tabBarButton(for title: String) -> XCUIElement {
        let id = tabAccessibilityIdentifier(for: title)
        let byId = app.buttons[id]
        if byId.waitForExistence(timeout: Timeout.tabItem) {
            return byId
        }

        let bar = primaryTabBar
        _ = bar.buttons.firstMatch.waitForExistence(timeout: Timeout.tabItem)
        let exact = bar.buttons[title]
        if exact.waitForExistence(timeout: Timeout.tabItem) {
            return exact
        }
        let fuzzy = bar.buttons
            .matching(NSPredicate(format: "label CONTAINS[c] %@", title))
            .firstMatch
        if fuzzy.waitForExistence(timeout: 8) {
            return fuzzy
        }

        let appExact = app.buttons[title]
        if appExact.waitForExistence(timeout: Timeout.tabItem) {
            return appExact
        }
        let appFuzzy = app.buttons
            .matching(NSPredicate(format: "label CONTAINS[c] %@", title))
            .firstMatch
        if appFuzzy.waitForExistence(timeout: 8) {
            return appFuzzy
        }

        if title == "Overview" {
            if bar.buttons.count > 0 {
                return bar.buttons.element(boundBy: 0)
            }
            return app.buttons[id]
        }
        return exact
    }

    /// Overflow **More** tab: query at app scope so it resolves whether the control is nested under `tabBars` or flattened on newer OS builds.
    private func moreTabButton() -> XCUIElement {
        app.buttons["More"]
    }

    private func anyPrimaryTabChromeVisible() -> Bool {
        if primaryTabBar.buttons.count > 0 {
            return true
        }
        for label in primaryTabLabels {
            if app.buttons[tabAccessibilityIdentifier(for: label)].exists {
                return true
            }
        }
        return false
    }

    private func launchAndSettle() {
        app.launch()
        XCTAssertTrue(
            app.wait(for: .runningForeground, timeout: Timeout.launchForeground),
            "App should reach foreground (CI simulator startup)",
        )
        let tabBar = primaryTabBar
        let tabBarChrome = tabBar.waitForExistence(timeout: Timeout.tabBar)
        let overviewId = app.buttons[tabAccessibilityIdentifier(for: "Overview")]
        let tabsByIdentifier = overviewId.waitForExistence(timeout: Timeout.tabBar)
        XCTAssertTrue(
            tabBarChrome || tabsByIdentifier,
            "Tab bar or tab accessibility roots should appear after launch",
        )
        if tabBarChrome {
            _ = tabBar.buttons.firstMatch.waitForExistence(timeout: Timeout.tabBar)
        }
        let overview = tabBarButton(for: "Overview")
        XCTAssertTrue(
            overview.waitForExistence(timeout: Timeout.tabBar)
                || tabBar.buttons.count > 0
                || anyPrimaryTabChromeVisible(),
            "Primary shell should expose Overview (or first tab when labels differ)",
        )
    }

    /// Row for an overflow tab inside the system **More** list.
    private func moreListCell(for label: String) -> XCUIElement {
        let byLabel = NSPredicate(format: "label == %@", label)
        return app.tables.cells.containing(byLabel).firstMatch
    }

    /// Selects a root tab, using **More** when the item is not on the main tab bar (six tabs on iPhone).
    private func selectPrimaryTab(_ label: String) {
        let direct = tabBarButton(for: label)
        if direct.waitForExistence(timeout: 3), direct.isHittable {
            direct.tap()
            return
        }
        let more = moreTabButton()
        XCTAssertTrue(more.waitForExistence(timeout: Timeout.tabItem), "Expected More tab for overflow item \(label)")
        XCTAssertTrue(more.isHittable, "More tab should be tappable")
        more.tap()
        tapOverflowTabRow(label)
    }

    /// Asserts the tab is reachable with a proper touch target: main bar button or **More** list row.
    private func assertPrimaryTabHittable(_ label: String) {
        let direct = tabBarButton(for: label)
        if direct.waitForExistence(timeout: 4) {
            XCTAssertTrue(direct.isHittable, "Tab \(label) should be hittable")
            return
        }
        let more = moreTabButton()
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
                tabBarButton(for: label).waitForExistence(timeout: Timeout.tabItem),
                "Expected tab bar item \(label)",
            )
        }
        let toolsOnBar = primaryTabBar.buttons["Tools"].waitForExistence(timeout: 3)
            || app.buttons["tab_tools"].waitForExistence(timeout: 3)
            || app.buttons["Tools"].waitForExistence(timeout: 3)
        let settingsOnBar = primaryTabBar.buttons["Settings"].waitForExistence(timeout: 3)
            || app.buttons["tab_settings"].waitForExistence(timeout: 3)
            || app.buttons["Settings"].waitForExistence(timeout: 3)
        if toolsOnBar, settingsOnBar {
            XCTAssertTrue(primaryTabBar.buttons["Tools"].exists || app.buttons["tab_tools"].exists || app.buttons["Tools"].exists)
            XCTAssertTrue(
                primaryTabBar.buttons["Settings"].exists || app.buttons["tab_settings"].exists || app.buttons["Settings"].exists,
            )
        } else {
            XCTAssertTrue(
                moreTabButton().waitForExistence(timeout: Timeout.tabItem),
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
        XCTAssertTrue(tabBarButton(for: "Chat").waitForExistence(timeout: Timeout.tabItem))
    }
}
