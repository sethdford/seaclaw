import Foundation

/// Minimal HTTP request parser. Handles what we need for OpenAI-compatible endpoints.
public struct HTTPRequest: Sendable {
    public let method: String
    public let path: String
    public let headers: [String: String]
    public let body: Data?

    /// Parse an HTTP request from raw data. Returns nil if incomplete.
    static func parse(_ data: Data) -> HTTPRequest? {
        guard let str = String(data: data, encoding: .utf8) else { return nil }
        guard let headerEnd = str.range(of: "\r\n\r\n") else { return nil }

        let headerSection = String(str[str.startIndex..<headerEnd.lowerBound])
        let lines = headerSection.split(separator: "\r\n", omittingEmptySubsequences: false)
        guard let requestLine = lines.first else { return nil }

        let parts = requestLine.split(separator: " ", maxSplits: 2)
        guard parts.count >= 2 else { return nil }
        let method = String(parts[0])
        let path = String(parts[1])

        var headers: [String: String] = [:]
        for line in lines.dropFirst() {
            if let colonIdx = line.firstIndex(of: ":") {
                let key = String(line[line.startIndex..<colonIdx]).trimmingCharacters(in: .whitespaces).lowercased()
                let val = String(line[line.index(after: colonIdx)...]).trimmingCharacters(in: .whitespaces)
                headers[key] = val
            }
        }

        let bodyStart = data.index(data.startIndex, offsetBy: str.distance(from: str.startIndex, to: headerEnd.upperBound))
        let remaining = data.count - bodyStart

        if let contentLength = headers["content-length"], let expected = Int(contentLength) {
            guard remaining >= expected else { return nil }
            let body = data.subdata(in: bodyStart..<(bodyStart + expected))
            return HTTPRequest(method: method, path: path, headers: headers, body: body)
        }

        let body = remaining > 0 ? data.subdata(in: bodyStart..<data.count) : nil
        return HTTPRequest(method: method, path: path, headers: headers, body: body)
    }

    public var json: [String: Any]? {
        guard let body = body else { return nil }
        return try? JSONSerialization.jsonObject(with: body) as? [String: Any]
    }
}
