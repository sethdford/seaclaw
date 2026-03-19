import SwiftUI

/// Defers building content until the view is actually rendered.
/// Use for heavy tab/navigation destinations to avoid initializing until navigated to.
public struct LazyView<Content: View>: View {
    let build: () -> Content
    @State private var hasAppeared = false

    public init(_ build: @autoclosure @escaping () -> Content) {
        self.build = build
    }

    public var body: some View {
        Group {
            if hasAppeared {
                build()
            }
        }
        .onAppear {
            if !hasAppeared {
                hasAppeared = true
            }
        }
    }
}
