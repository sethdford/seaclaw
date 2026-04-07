import Foundation

/// Minimal HTTP response builder.
public struct HTTPResponse: Sendable {
    public let statusCode: Int
    public let statusText: String
    public let contentType: String
    public let body: Data
    public let streamChunks: AsyncStream<Data>?
    /// When set (e.g. CORS preflight), emitted after `Access-Control-Allow-Origin`.
    public let accessControlAllowMethods: String?
    public let accessControlAllowHeaders: String?

    public var isStreaming: Bool { streamChunks != nil }

    public init(
        statusCode: Int,
        statusText: String,
        contentType: String,
        body: Data,
        streamChunks: AsyncStream<Data>?,
        accessControlAllowMethods: String? = nil,
        accessControlAllowHeaders: String? = nil
    ) {
        self.statusCode = statusCode
        self.statusText = statusText
        self.contentType = contentType
        self.body = body
        self.streamChunks = streamChunks
        self.accessControlAllowMethods = accessControlAllowMethods
        self.accessControlAllowHeaders = accessControlAllowHeaders
    }

    public static func json(_ object: Any, status: Int = 200) -> HTTPResponse {
        let body = (try? JSONSerialization.data(withJSONObject: object, options: [.sortedKeys])) ?? Data()
        return HTTPResponse(
            statusCode: status,
            statusText: statusText(for: status),
            contentType: "application/json",
            body: body,
            streamChunks: nil,
            accessControlAllowMethods: nil,
            accessControlAllowHeaders: nil
        )
    }

    public static func sseStream(status: Int = 200, chunks: AsyncStream<Data>) -> HTTPResponse {
        HTTPResponse(
            statusCode: status,
            statusText: "OK",
            contentType: "text/event-stream",
            body: Data(),
            streamChunks: chunks,
            accessControlAllowMethods: nil,
            accessControlAllowHeaders: nil
        )
    }

    public static func error(_ message: String, status: Int = 500) -> HTTPResponse {
        json(["error": ["message": message, "type": "server_error"]], status: status)
    }

    public static func notFound() -> HTTPResponse {
        error("Not found", status: 404)
    }

    public static func methodNotAllowed() -> HTTPResponse {
        error("Method not allowed", status: 405)
    }

    func headerData(accessControlAllowOrigin: String? = nil) -> Data {
        var header = "HTTP/1.1 \(statusCode) \(statusText)\r\n"
        header += "Content-Type: \(contentType)\r\n"
        if let origin = accessControlAllowOrigin {
            header += "Access-Control-Allow-Origin: \(origin)\r\n"
        }
        if let methods = accessControlAllowMethods {
            header += "Access-Control-Allow-Methods: \(methods)\r\n"
        }
        if let allowHeaders = accessControlAllowHeaders {
            header += "Access-Control-Allow-Headers: \(allowHeaders)\r\n"
        }
        header += "Connection: close\r\n"
        if !isStreaming {
            header += "Content-Length: \(body.count)\r\n"
        } else {
            header += "Transfer-Encoding: chunked\r\n"
            header += "Cache-Control: no-cache\r\n"
        }
        header += "\r\n"
        return Data(header.utf8)
    }

    private static func statusText(for code: Int) -> String {
        switch code {
        case 200: return "OK"
        case 400: return "Bad Request"
        case 404: return "Not Found"
        case 401: return "Unauthorized"
        case 405: return "Method Not Allowed"
        case 500: return "Internal Server Error"
        case 503: return "Service Unavailable"
        default: return "Unknown"
        }
    }
}
