import Testing
import Foundation
@testable import HumanOnDeviceServer

@Suite("HTTPRequest Parsing")
struct HTTPRequestParsingTests {
    @Test("Parse simple GET request")
    func parseGet() {
        let raw = "GET /v1/models HTTP/1.1\r\nHost: localhost\r\n\r\n"
        let req = HTTPRequest.parse(Data(raw.utf8))
        #expect(req != nil)
        #expect(req?.method == "GET")
        #expect(req?.path == "/v1/models")
    }

    @Test("Parse POST with JSON body")
    func parsePostWithBody() {
        let body = "{\"model\":\"test\"}"
        let raw = "POST /v1/chat/completions HTTP/1.1\r\nHost: localhost\r\nContent-Length: \(body.count)\r\n\r\n\(body)"
        let req = HTTPRequest.parse(Data(raw.utf8))
        #expect(req != nil)
        #expect(req?.method == "POST")
        #expect(req?.path == "/v1/chat/completions")
        #expect(req?.json != nil)
        #expect(req?.json?["model"] as? String == "test")
    }

    @Test("Incomplete request returns nil")
    func incompleteRequest() {
        let raw = "GET /v1/models HTTP/1.1\r\nHost: loc"
        let req = HTTPRequest.parse(Data(raw.utf8))
        #expect(req == nil)
    }

    @Test("POST with incomplete body returns nil")
    func incompleteBody() {
        let raw = "POST /v1/chat HTTP/1.1\r\nContent-Length: 100\r\n\r\nshort"
        let req = HTTPRequest.parse(Data(raw.utf8))
        #expect(req == nil)
    }

    @Test("Headers are case-insensitive")
    func headersLowercased() {
        let raw = "GET / HTTP/1.1\r\nContent-Type: application/json\r\nX-Custom: value\r\n\r\n"
        let req = HTTPRequest.parse(Data(raw.utf8))
        #expect(req?.headers["content-type"] == "application/json")
        #expect(req?.headers["x-custom"] == "value")
    }
}

@Suite("HTTPResponse")
struct HTTPResponseTests {
    @Test("JSON response has correct content-type")
    func jsonResponse() {
        let resp = HTTPResponse.json(["key": "value"])
        #expect(resp.statusCode == 200)
        #expect(resp.contentType == "application/json")
        #expect(!resp.isStreaming)
    }

    @Test("Error response")
    func errorResponse() {
        let resp = HTTPResponse.error("test error", status: 500)
        #expect(resp.statusCode == 500)
        let json = try? JSONSerialization.jsonObject(with: resp.body) as? [String: Any]
        let err = json?["error"] as? [String: Any]
        #expect(err?["message"] as? String == "test error")
    }

    @Test("Header data includes content-length")
    func headerData() {
        let resp = HTTPResponse.json(["a": 1])
        let header = String(data: resp.headerData(), encoding: .utf8)!
        #expect(header.contains("Content-Length:"))
        #expect(header.contains("HTTP/1.1 200 OK"))
    }
}

@available(macOS 14.0, iOS 17.0, *)
@Suite("OnDeviceServer")
struct OnDeviceServerTests {
    @Test("Server initializes with default port")
    func defaultPort() {
        let server = OnDeviceServer()
        #expect(server.port == 11435)
        #expect(server.baseURL == "http://127.0.0.1:11435/v1")
    }

    @Test("Server initializes with custom port")
    func customPort() {
        let server = OnDeviceServer(port: 9999)
        #expect(server.port == 9999)
        #expect(server.baseURL == "http://127.0.0.1:9999/v1")
    }
}
