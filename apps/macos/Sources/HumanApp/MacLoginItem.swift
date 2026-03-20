import AppKit
import Foundation
import ServiceManagement

/// Registers the **main app** as a login item via `SMAppService` (macOS 13+).
enum MacLoginItem {
    static var isEnabled: Bool {
        SMAppService.mainApp.status == .enabled
    }

    static var statusDescription: String {
        switch SMAppService.mainApp.status {
        case .enabled:
            return "enabled"
        case .notRegistered:
            return "not registered"
        case .requiresApproval:
            return "requires approval in System Settings"
        case .notFound:
            return "not found"
        @unknown default:
            return "unknown"
        }
    }

    /// Applies login-item registration. Returns a user-visible error, if any.
    @discardableResult
    static func setEnabled(_ enabled: Bool) -> String? {
        let svc = SMAppService.mainApp
        do {
            if enabled {
                guard svc.status != .enabled else { return nil }
                try svc.register()
                if svc.status == .requiresApproval {
                    SMAppService.openSystemSettingsLoginItems()
                    return "Approve “\(appName)” under Login Items in System Settings."
                }
            } else {
                guard svc.status != .notRegistered else { return nil }
                try svc.unregister()
            }
            return nil
        } catch {
            return error.localizedDescription
        }
    }

    private static var appName: String {
        Bundle.main.object(forInfoDictionaryKey: "CFBundleName") as? String
            ?? Bundle.main.bundleURL.deletingPathExtension().lastPathComponent
    }
}
