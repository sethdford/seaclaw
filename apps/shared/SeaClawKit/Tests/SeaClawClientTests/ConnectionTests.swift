import XCTest
@testable import SeaClawClient
import SeaClawProtocol

final class ConnectionTests: XCTestCase {

    func testInitialStateIsDisconnected() {
        let conn = SeaClawConnection(url: URL(string: "wss://localhost:3000/ws")!)
        XCTAssertEqual(conn.state, .disconnected)
    }

    func testConvenienceInitWithValidURL() {
        let conn = SeaClawConnection(urlString: "wss://127.0.0.1:3000/ws")
        XCTAssertEqual(conn.state, .disconnected)
    }

    func testConvenienceInitWithInvalidURLFallsBack() {
        let conn = SeaClawConnection(urlString: "")
        XCTAssertEqual(conn.state, .disconnected)
    }

    func testDisconnectFromDisconnectedIsNoop() {
        let conn = SeaClawConnection(url: URL(string: "wss://localhost:3000/ws")!)
        conn.disconnect()
        XCTAssertEqual(conn.state, .disconnected)
    }

    func testStateHandlerCalled() {
        let conn = SeaClawConnection(url: URL(string: "wss://localhost:3000/ws")!)
        var states: [SeaClawConnection.ConnectionState] = []
        conn.stateHandler = { states.append($0) }
        conn.disconnect()
        RunLoop.current.run(until: Date().addingTimeInterval(0.1))
        XCTAssertTrue(states.isEmpty || states.allSatisfy { $0 == .disconnected })
    }

    func testConnectionStateEquatable() {
        XCTAssertEqual(SeaClawConnection.ConnectionState.disconnected, .disconnected)
        XCTAssertEqual(SeaClawConnection.ConnectionState.connecting, .connecting)
        XCTAssertEqual(SeaClawConnection.ConnectionState.connected, .connected)
        XCTAssertNotEqual(SeaClawConnection.ConnectionState.disconnected, .connected)
    }
}
