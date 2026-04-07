import Foundation
import Network

/// Minimal HTTP server built on Network.framework. Zero external dependencies.
/// Handles only the routes needed for OpenAI-compatible on-device inference.
@available(macOS 14.0, iOS 17.0, *)
public final class HTTPServer: @unchecked Sendable {
    /// Upper bound for total bytes read per connection (headers + body).
    public static let maxRequestSize = 10 * 1024 * 1024

    private var listener: NWListener?
    private let port: UInt16
    private let queue = DispatchQueue(label: "com.human.ondevice.http", attributes: .concurrent)
    private let router: HTTPRouter

    public init(port: UInt16 = 11435, router: HTTPRouter) {
        self.port = port
        self.router = router
    }

    public func start() throws {
        let params = NWParameters.tcp
        params.allowLocalEndpointReuse = true
        let nwPort = NWEndpoint.Port(rawValue: port)!
        params.requiredLocalEndpoint = NWEndpoint.hostPort(host: "127.0.0.1", port: nwPort)
        listener = try NWListener(using: params)

        listener?.stateUpdateHandler = { state in
            switch state {
            case .ready:
                break
            case .failed(let error):
                print("[human-ondevice] listener failed: \(error)")
            default:
                break
            }
        }

        listener?.newConnectionHandler = { [weak self] connection in
            self?.handleConnection(connection)
        }

        listener?.start(queue: queue)
    }

    public func stop() {
        listener?.cancel()
        listener = nil
    }

    public var actualPort: UInt16 { port }

    private func handleConnection(_ connection: NWConnection) {
        connection.start(queue: queue)
        receiveHTTPRequest(connection: connection, accumulated: Data())
    }

    private func receiveHTTPRequest(connection: NWConnection, accumulated: Data) {
        connection.receive(minimumIncompleteLength: 1, maximumLength: 65536) { [weak self] content, _, isComplete, error in
            guard let self = self else { return }

            if let error = error {
                print("[human-ondevice] receive error: \(error)")
                connection.cancel()
                return
            }

            var data = accumulated
            if let content = content {
                data.append(content)
            }

            if data.count > Self.maxRequestSize {
                connection.cancel()
                return
            }

            if let request = HTTPRequest.parse(data) {
                Task {
                    let response = await self.router.handle(request)
                    self.sendResponse(connection: connection, response: response, streaming: response.isStreaming)
                }
            } else if isComplete {
                connection.cancel()
            } else {
                self.receiveHTTPRequest(connection: connection, accumulated: data)
            }
        }
    }

    private func sendResponse(connection: NWConnection, response: HTTPResponse, streaming: Bool) {
        let headerData = response.headerData(accessControlAllowOrigin: "http://127.0.0.1:\(port)")
        connection.send(content: headerData, completion: .contentProcessed { _ in
            if let chunks = response.streamChunks {
                self.sendStreamChunks(connection: connection, chunks: chunks)
            } else {
                let body = response.body
                connection.send(content: body, contentContext: .finalMessage, isComplete: true, completion: .contentProcessed { _ in
                    connection.cancel()
                })
            }
        })
    }

    private func sendStreamChunks(connection: NWConnection, chunks: AsyncStream<Data>) {
        Task {
            for await chunk in chunks {
                await withCheckedContinuation { (cont: CheckedContinuation<Void, Never>) in
                    connection.send(content: chunk, completion: .contentProcessed { _ in
                        cont.resume()
                    })
                }
            }
            connection.send(content: nil, contentContext: .finalMessage, isComplete: true, completion: .contentProcessed { _ in
                connection.cancel()
            })
        }
    }
}
