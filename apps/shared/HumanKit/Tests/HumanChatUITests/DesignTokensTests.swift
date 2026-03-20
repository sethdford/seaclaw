import XCTest
@testable import HumanChatUI

final class DesignTokensTests: XCTestCase {
    func test_spacing_scale_is_positive() {
        XCTAssertGreaterThan(HUTokens.spaceXs, 0)
        XCTAssertGreaterThan(HUTokens.spaceMd, HUTokens.spaceXs)
    }

    func test_radius_tokens_positive() {
        XCTAssertGreaterThan(HUTokens.radiusSm, 0)
        XCTAssertGreaterThan(HUTokens.radiusMd, HUTokens.radiusSm)
    }
}
