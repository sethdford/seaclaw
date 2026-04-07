import AppKit
import Foundation

class ProcessManager {
    private var process: Process?
    private var apfelProcess: Process?
    private let queue = DispatchQueue(label: "com.human.process")

    /// Surface start failures to the UI (e.g. binary missing).
    var onStartFailure: ((String) -> Void)?

    var isRunning: Bool {
        queue.sync {
            guard let p = process else { return false }
            return p.isRunning
        }
    }

    var isApfelRunning: Bool {
        queue.sync {
            guard let p = apfelProcess else { return false }
            return p.isRunning
        }
    }

    func humanPath() -> String? {
        resolveBinary("human")
    }

    func apfelPath() -> String? {
        resolveBinary("apfel")
    }

    private func resolveBinary(_ name: String) -> String? {
        if let path = ProcessInfo.processInfo.environment["PATH"] {
            for component in path.split(separator: ":") {
                let candidate = String(component) + "/\(name)"
                if FileManager.default.isExecutableFile(atPath: candidate) {
                    return candidate
                }
            }
        }
        for candidate in ["/usr/local/bin/\(name)", "/opt/homebrew/bin/\(name)"] {
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
        startApfelIfAvailable()
    }

    /// Launch the apfel on-device server if the binary is installed.
    func startApfelIfAvailable() {
        queue.async { [weak self] in
            guard let self = self else { return }
            guard self.apfelProcess == nil || (self.apfelProcess?.isRunning == false) else { return }
            guard let path = self.apfelPath() else { return }
            let p = Process()
            p.executableURL = URL(fileURLWithPath: path)
            p.arguments = ["--serve"]
            p.standardOutput = nil
            p.standardError = nil
            do {
                try p.run()
                self.apfelProcess = p
            } catch {
                // apfel is optional; silently ignore launch failures
            }
        }
    }

    func stop() {
        queue.async { [weak self] in
            guard let self = self else { return }
            self.process?.terminate()
            self.process = nil
            self.apfelProcess?.terminate()
            self.apfelProcess = nil
        }
    }
}
