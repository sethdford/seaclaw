import AppKit
import Foundation

class ProcessManager {
    private var process: Process?
    private let queue = DispatchQueue(label: "com.human.process")

    /// Surface start failures to the UI (e.g. binary missing).
    var onStartFailure: ((String) -> Void)?

    var isRunning: Bool {
        queue.sync {
            guard let p = process else { return false }
            return p.isRunning
        }
    }

    func humanPath() -> String? {
        if let path = ProcessInfo.processInfo.environment["PATH"] {
            for component in path.split(separator: ":") {
                let candidate = String(component) + "/human"
                if FileManager.default.isExecutableFile(atPath: candidate) {
                    return candidate
                }
            }
        }
        for candidate in ["/usr/local/bin/human", "/opt/homebrew/bin/human"] {
            if FileManager.default.isExecutableFile(atPath: candidate) {
                return candidate
            }
        }
        return nil
    }

    func start() {
        queue.async { [weak self] in
            guard let self = self else { return }
            guard self.process == nil || (self.process?.isRunning == false) else { return }
            guard let path = self.humanPath() else {
                DispatchQueue.main.async {
                    self.onStartFailure?("human binary not found in PATH.")
                }
                return
            }
            let p = Process()
            p.executableURL = URL(fileURLWithPath: path)
            p.arguments = ["service"]
            p.standardOutput = nil
            p.standardError = nil
            do {
                try p.run()
                self.process = p
            } catch {
                DispatchQueue.main.async {
                    self.onStartFailure?(error.localizedDescription)
                }
            }
        }
    }

    func stop() {
        queue.async { [weak self] in
            guard let self = self else { return }
            self.process?.terminate()
            self.process = nil
        }
    }
}
