// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "SeaClaw",
    platforms: [.macOS(.v14)],
    targets: [
        .executableTarget(
            name: "SeaClaw",
            path: "Sources/SeaClawApp"
        ),
    ]
)
